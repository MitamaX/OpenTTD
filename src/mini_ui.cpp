/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mini_ui.cpp Stand-alone minimalist UI; reads game state, issues commands, owns its own framebuffer. */

#include "stdafx.h"

#include "mini_ui.h"

#include "blitter/factory.hpp"
#include "bridge_map.h"
#include "clear_map.h"
#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "core/math_func.hpp"
#include "fileio_func.h"
#include "gfx_func.h"
#include "ini_type.h"
#include "landscape.h"
#include "misc_cmd.h"
#include "openttd.h"
#include "palette_func.h"
#include "rail_cmd.h"
#include "rail_map.h"
#include "road_map.h"
#include "slope_func.h"
#include "station_map.h"
#include "tile_map.h"
#include "timer/timer_game_calendar.h"
#include "tunnelbridge_map.h"
#include "vehicle_base.h"
#include "video/video_driver.hpp"
#include "water_map.h"
#include "window_func.h"
#include "window_gui.h"

#include "safeguards.h"

static bool _mini_active = false;

/* Framebuffer, 0xAARRGGBB, matches the memory layout of the 32bpp blitters. */
static std::vector<uint32_t> _fb;
static int _fbw, _fbh;

static const double MIN_PPT = 4.0;
static const double MAX_PPT = 64.0;

/* Camera position in tile units at the screen centre; ppt is pixels per tile. */
static double _cam_x, _cam_y, _cam_ppt = 16.0;
static double _dest_ppt = 16.0;

enum class MiniTool : uint8_t {
	None,
	Rail,
};

static MiniTool _tool = MiniTool::None;
static bool _dragging = false;
static bool _drag_remove = false;
static double _drag_ax, _drag_ay;

static bool _prev_left = false;

struct MiniRailPlan {
	TileIndex start = INVALID_TILE;
	TileIndex end = INVALID_TILE;
	Track track = INVALID_TRACK;
	std::vector<std::pair<TileIndex, Track>> pieces;
};

static MiniRailPlan _plan;

struct MiniSettings {
	double pan_speed = 1600.0;
	double pan_speed_fast = 4000.0;
	double zoom_step = 1.25;
	double zoom_smooth_ms = 80.0;
	int hud_scale = 2;
	int contour_alpha = 120;
	int relief_strength = 22;
	int edge_scroll = 1;
	int edge_margin = 24;
	double edge_scroll_speed = 1600.0;
	double drag_pan_multiplier = 2.0;
};

static MiniSettings _ms;

static bool _zoom_anchored = false;
static int _zoom_sx, _zoom_sy;
static double _zoom_wx, _zoom_wy;

static void ReadIniNumber(IniGroup &group, std::string_view name, double &v)
{
	if (const IniItem *item = group.GetItem(name); item != nullptr && item->value.has_value()) {
		const std::string &s = *item->value;
		double parsed;
		if (std::from_chars(s.data(), s.data() + s.size(), parsed).ec == std::errc{}) v = parsed;
	} else {
		group.GetOrCreateItem(name).SetValue(fmt::format("{}", v));
	}
}

static void ReadIniNumber(IniGroup &group, std::string_view name, int &v)
{
	if (const IniItem *item = group.GetItem(name); item != nullptr && item->value.has_value()) {
		const std::string &s = *item->value;
		int parsed;
		if (std::from_chars(s.data(), s.data() + s.size(), parsed).ec == std::errc{}) v = parsed;
	} else {
		group.GetOrCreateItem(name).SetValue(fmt::format("{}", v));
	}
}

/* Re-read on every activation, so tuning only needs an F9 round trip. */
static void LoadMiniSettings()
{
	_ms = {};

	std::string path = _personal_dir + "mini_ui.cfg";
	IniFile ini;
	ini.LoadFromDisk(path, NO_DIRECTORY);
	IniGroup &group = ini.GetOrCreateGroup("mini");

	ReadIniNumber(group, "pan_speed", _ms.pan_speed);
	ReadIniNumber(group, "pan_speed_fast", _ms.pan_speed_fast);
	ReadIniNumber(group, "zoom_step", _ms.zoom_step);
	ReadIniNumber(group, "zoom_smooth_ms", _ms.zoom_smooth_ms);
	ReadIniNumber(group, "hud_scale", _ms.hud_scale);
	ReadIniNumber(group, "contour_alpha", _ms.contour_alpha);
	ReadIniNumber(group, "relief_strength", _ms.relief_strength);
	ReadIniNumber(group, "edge_scroll", _ms.edge_scroll);
	ReadIniNumber(group, "edge_margin", _ms.edge_margin);
	ReadIniNumber(group, "edge_scroll_speed", _ms.edge_scroll_speed);
	ReadIniNumber(group, "drag_pan_multiplier", _ms.drag_pan_multiplier);

	_ms.pan_speed = Clamp(_ms.pan_speed, 100.0, 10000.0);
	_ms.pan_speed_fast = Clamp(_ms.pan_speed_fast, 100.0, 20000.0);
	_ms.zoom_step = Clamp(_ms.zoom_step, 1.05, 2.0);
	_ms.zoom_smooth_ms = Clamp(_ms.zoom_smooth_ms, 1.0, 500.0);
	_ms.hud_scale = Clamp(_ms.hud_scale, 1, 4);
	_ms.contour_alpha = Clamp(_ms.contour_alpha, 0, 255);
	_ms.relief_strength = Clamp(_ms.relief_strength, 0, 60);
	_ms.edge_margin = Clamp(_ms.edge_margin, 2, 200);
	_ms.edge_scroll_speed = Clamp(_ms.edge_scroll_speed, 100.0, 10000.0);
	_ms.drag_pan_multiplier = Clamp(_ms.drag_pan_multiplier, 0.5, 8.0);

	ini.SaveToDisk(path);
}

static const uint32_t COL_VOID = 0xFF0A0A0AU;
static const uint32_t COL_WATER = 0xFF2F6EA5U;
static const uint32_t COL_SNOW = 0xFFF0F4F7U;
static const uint32_t COL_DESERT = 0xFFE0CA8CU;
static const uint32_t COL_ROCKS = 0xFF8E979EU;
static const uint32_t COL_FIELDS = 0xFFD4B23AU;
static const uint32_t COL_TREE = 0xFF2F4A2AU;
static const uint32_t COL_RAIL = 0xFF33383DU;
static const uint32_t COL_ROAD = 0xFF61686EU;
static const uint32_t COL_HOUSE = 0xFF9C8A76U;
static const uint32_t COL_HOUSE_B = 0xFF6E5F4EU;
static const uint32_t COL_IND = 0xFFD07A4AU;
static const uint32_t COL_IND_B = 0xFF8F4E2BU;
static const uint32_t COL_OBJ = 0xFFB0B4B8U;
static const uint32_t COL_OBJ_B = 0xFF80858AU;
static const uint32_t COL_TUNNEL = 0xFF1E2124U;
static const uint32_t COL_BRIDGE = 0xFF9AA0A6U;
static const uint32_t COL_BP = 0xFF7FD1FFU;
static const uint32_t COL_BP_RM = 0xFFFF6B6BU;

/* All-green ramp like the old top-down renderer settled on: one step
 * darker per height level, hue constant so slopes match their neighbours. */
static const uint32_t _height_ramp[16] = {
	0xFF9CCB74U, 0xFF91C16CU, 0xFF86B765U, 0xFF7CAD5EU,
	0xFF71A357U, 0xFF679950U, 0xFF5D8F49U, 0xFF538443U,
	0xFF4A7A3DU, 0xFF417037U, 0xFF386631U, 0xFF305C2BU,
	0xFF285226U, 0xFF214921U, 0xFF1A3F1CU, 0xFF143618U,
};

static const uint32_t _company_rgb[16] = {
	0xFF1F3A93U, 0xFF9CCC65U, 0xFFEC8FB0U, 0xFFF2D24BU,
	0xFFD64541U, 0xFF4FC3F7U, 0xFF66BB6AU, 0xFF2E7D32U,
	0xFF3D6DCCU, 0xFFEFE5C0U, 0xFF9E8FA8U, 0xFF8E6FB8U,
	0xFFF29C4AU, 0xFF8D6E63U, 0xFF9E9E9EU, 0xFFF5F5F5U,
};

/* Classic 5x7 column-major bitmap font, ASCII 0x20..0x5F. */
static const uint8_t _font5x7[96 * 5] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5F, 0x00, 0x00,
	0x00, 0x07, 0x00, 0x07, 0x00, 0x14, 0x7F, 0x14, 0x7F, 0x14,
	0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x23, 0x13, 0x08, 0x64, 0x62,
	0x36, 0x49, 0x55, 0x22, 0x50, 0x00, 0x05, 0x03, 0x00, 0x00,
	0x00, 0x1C, 0x22, 0x41, 0x00, 0x00, 0x41, 0x22, 0x1C, 0x00,
	0x08, 0x2A, 0x1C, 0x2A, 0x08, 0x08, 0x08, 0x3E, 0x08, 0x08,
	0x00, 0x50, 0x30, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x00, 0x60, 0x60, 0x00, 0x00, 0x20, 0x10, 0x08, 0x04, 0x02,
	0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00, 0x42, 0x7F, 0x40, 0x00,
	0x42, 0x61, 0x51, 0x49, 0x46, 0x21, 0x41, 0x45, 0x4B, 0x31,
	0x18, 0x14, 0x12, 0x7F, 0x10, 0x27, 0x45, 0x45, 0x45, 0x39,
	0x3C, 0x4A, 0x49, 0x49, 0x30, 0x01, 0x71, 0x09, 0x05, 0x03,
	0x36, 0x49, 0x49, 0x49, 0x36, 0x06, 0x49, 0x49, 0x29, 0x1E,
	0x00, 0x36, 0x36, 0x00, 0x00, 0x00, 0x56, 0x36, 0x00, 0x00,
	0x00, 0x08, 0x14, 0x22, 0x41, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x41, 0x22, 0x14, 0x08, 0x00, 0x02, 0x01, 0x51, 0x09, 0x06,
	0x32, 0x49, 0x79, 0x41, 0x3E, 0x7E, 0x11, 0x11, 0x11, 0x7E,
	0x7F, 0x49, 0x49, 0x49, 0x36, 0x3E, 0x41, 0x41, 0x41, 0x22,
	0x7F, 0x41, 0x41, 0x22, 0x1C, 0x7F, 0x49, 0x49, 0x49, 0x41,
	0x7F, 0x09, 0x09, 0x09, 0x01, 0x3E, 0x41, 0x41, 0x51, 0x32,
	0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00, 0x41, 0x7F, 0x41, 0x00,
	0x20, 0x40, 0x41, 0x3F, 0x01, 0x7F, 0x08, 0x14, 0x22, 0x41,
	0x7F, 0x40, 0x40, 0x40, 0x40, 0x7F, 0x02, 0x0C, 0x02, 0x7F,
	0x7F, 0x04, 0x08, 0x10, 0x7F, 0x3E, 0x41, 0x41, 0x41, 0x3E,
	0x7F, 0x09, 0x09, 0x09, 0x06, 0x3E, 0x41, 0x51, 0x21, 0x5E,
	0x7F, 0x09, 0x19, 0x29, 0x46, 0x46, 0x49, 0x49, 0x49, 0x31,
	0x01, 0x01, 0x7F, 0x01, 0x01, 0x3F, 0x40, 0x40, 0x40, 0x3F,
	0x1F, 0x20, 0x40, 0x20, 0x1F, 0x3F, 0x40, 0x38, 0x40, 0x3F,
	0x63, 0x14, 0x08, 0x14, 0x63, 0x07, 0x08, 0x70, 0x08, 0x07,
	0x61, 0x51, 0x49, 0x45, 0x43, 0x00, 0x7F, 0x41, 0x41, 0x00,
	0x02, 0x04, 0x08, 0x10, 0x20, 0x00, 0x41, 0x41, 0x7F, 0x00,
	0x04, 0x02, 0x01, 0x02, 0x04, 0x40, 0x40, 0x40, 0x40, 0x40,
};

bool MiniUiActive()
{
	return _mini_active;
}

static double PxBaseX() { return _fbw * 0.5 - _cam_x * _cam_ppt; }
static double PxBaseY() { return _fbh * 0.5 - _cam_y * _cam_ppt; }

static int PxX(double tx) { return (int)std::lround(tx * _cam_ppt + PxBaseX()); }
static int PxY(double ty) { return (int)std::lround(ty * _cam_ppt + PxBaseY()); }

static double WorldX(int sx) { return (sx - PxBaseX()) / _cam_ppt; }
static double WorldY(int sy) { return (sy - PxBaseY()) / _cam_ppt; }

static void FillRect(int x0, int y0, int x1, int y1, uint32_t c)
{
	x0 = std::max(x0, 0);
	y0 = std::max(y0, 0);
	x1 = std::min(x1, _fbw - 1);
	y1 = std::min(y1, _fbh - 1);
	if (x1 < x0 || y1 < y0) return;
	for (int y = y0; y <= y1; y++) {
		std::fill_n(_fb.data() + (size_t)y * _fbw + x0, x1 - x0 + 1, c);
	}
}

static uint32_t Mix(uint32_t dst, uint32_t src, uint alpha)
{
	uint inv = 255 - alpha;
	uint32_t rb = ((dst & 0xFF00FFU) * inv + (src & 0xFF00FFU) * alpha) >> 8;
	uint32_t g = ((dst & 0x00FF00U) * inv + (src & 0x00FF00U) * alpha) >> 8;
	return 0xFF000000U | (rb & 0xFF00FFU) | (g & 0x00FF00U);
}

static void BlendRect(int x0, int y0, int x1, int y1, uint32_t c, uint alpha)
{
	x0 = std::max(x0, 0);
	y0 = std::max(y0, 0);
	x1 = std::min(x1, _fbw - 1);
	y1 = std::min(y1, _fbh - 1);
	if (x1 < x0 || y1 < y0) return;
	for (int y = y0; y <= y1; y++) {
		uint32_t *row = _fb.data() + (size_t)y * _fbw;
		for (int x = x0; x <= x1; x++) row[x] = Mix(row[x], c, alpha);
	}
}

static void ThickLine(int x0, int y0, int x1, int y1, int width, uint32_t c)
{
	int steps = std::max(abs(x1 - x0), abs(y1 - y0));
	int half = width / 2;
	for (int i = 0; i <= steps; i++) {
		int x = x0 + (x1 - x0) * i / std::max(steps, 1);
		int y = y0 + (y1 - y0) * i / std::max(steps, 1);
		FillRect(x - half, y - half, x - half + width - 1, y - half + width - 1, c);
	}
}

static void DrawChar(int x, int y, int scale, char c, uint32_t col)
{
	uint8_t uc = (uint8_t)c;
	if (uc < 0x20 || uc >= 0x80) uc = '?';
	if (uc >= 'a' && uc <= 'z') uc -= 0x20;
	if (uc >= 0x60) uc = '?';
	c = (char)uc;
	const uint8_t *glyph = _font5x7 + (c - 0x20) * 5;
	for (int cx = 0; cx < 5; cx++) {
		for (int cy = 0; cy < 7; cy++) {
			if (glyph[cx] & (1 << cy)) {
				FillRect(x + cx * scale, y + cy * scale, x + (cx + 1) * scale - 1, y + (cy + 1) * scale - 1, col);
			}
		}
	}
}

static void DrawText(int x, int y, int scale, std::string_view text)
{
	for (char c : text) {
		DrawChar(x + scale, y + scale, scale, c, 0xFF14181CU);
		DrawChar(x, y, scale, c, 0xFFEDF2F7U);
		x += 6 * scale;
	}
}

static uint32_t GroundColour(TileIndex tile, int h)
{
	h = Clamp(h, 0, 15);
	switch (GetTileType(tile)) {
		case MP_CLEAR:
			switch (GetClearGround(tile)) {
				case CLEAR_FIELDS: return COL_FIELDS;
				case CLEAR_ROCKS: return COL_ROCKS;
				case CLEAR_SNOW: return COL_SNOW;
				case CLEAR_DESERT: return COL_DESERT;
				default: return _height_ramp[h];
			}
		default:
			return _height_ramp[h];
	}
}

/* Base colour comes from the lowest corner; sloped tiles darken toward their
 * higher corners with a bilinear gradient, matching the darker-is-higher ramp. */
static void DrawGround(TileIndex tile, int x0, int y0, int x1, int y1, int ppt)
{
	auto [s, hbase] = GetTileSlopeZ(tile);
	FillRect(x0, y0, x1, y1, GroundColour(tile, hbase));
	if (_ms.relief_strength == 0 || s == SLOPE_FLAT) return;

	int hn = GetSlopeZInCorner(s, CORNER_N);
	int hw = GetSlopeZInCorner(s, CORNER_W);
	int he = GetSlopeZInCorner(s, CORNER_E);
	int hs = GetSlopeZInCorner(s, CORNER_S);

	if (ppt < 8) {
		BlendRect(x0, y0, x1, y1, 0xFF000000U, _ms.relief_strength * (hn + hw + he + hs) / 4);
		return;
	}

	const int sub = Clamp(ppt / 8, 2, 6);
	int wpx = x1 - x0 + 1;
	int hpx = y1 - y0 + 1;
	int denom = (sub - 1) * (sub - 1);
	for (int j = 0; j < sub; j++) {
		for (int i = 0; i < sub; i++) {
			int top = hn * (sub - 1 - i) + hw * i;
			int bot = he * (sub - 1 - i) + hs * i;
			int zq = top * (sub - 1 - j) + bot * j;
			if (zq == 0) continue;
			int sx0 = x0 + wpx * i / sub;
			int sy0 = y0 + hpx * j / sub;
			int sx1 = x0 + wpx * (i + 1) / sub - 1;
			int sy1 = y0 + hpx * (j + 1) / sub - 1;
			if (sx1 < sx0 || sy1 < sy0) continue;
			BlendRect(sx0, sy0, sx1, sy1, 0xFF000000U, std::min(255, _ms.relief_strength * zq / denom));
		}
	}
}

static void DrawTrackPiece(Track t, int x0, int y0, int x1, int y1, int width, uint32_t c)
{
	int cx = (x0 + x1) / 2;
	int cy = (y0 + y1) / 2;
	switch (t) {
		case TRACK_X: FillRect(x0, cy - width / 2, x1, cy - width / 2 + width - 1, c); break;
		case TRACK_Y: FillRect(cx - width / 2, y0, cx - width / 2 + width - 1, y1, c); break;
		case TRACK_UPPER: ThickLine(x0, cy, cx, y0, width, c); break;
		case TRACK_LOWER: ThickLine(cx, y1, x1, cy, width, c); break;
		case TRACK_LEFT: ThickLine(cx, y0, x1, cy, width, c); break;
		case TRACK_RIGHT: ThickLine(x0, cy, cx, y1, width, c); break;
		default: break;
	}
}

static void DrawTrackBitsPx(TrackBits bits, int x0, int y0, int x1, int y1, int width, uint32_t c)
{
	for (Track t : {TRACK_X, TRACK_Y, TRACK_UPPER, TRACK_LOWER, TRACK_LEFT, TRACK_RIGHT}) {
		if (bits & TrackToTrackBits(t)) DrawTrackPiece(t, x0, y0, x1, y1, width, c);
	}
}

static void DrawRoadBitsPx(RoadBits bits, int x0, int y0, int x1, int y1, int width, uint32_t c)
{
	int cx = (x0 + x1) / 2;
	int cy = (y0 + y1) / 2;
	int lo = width / 2;
	if (bits & ROAD_NW) FillRect(cx - lo, y0, cx - lo + width - 1, cy, c);
	if (bits & ROAD_SE) FillRect(cx - lo, cy, cx - lo + width - 1, y1, c);
	if (bits & ROAD_NE) FillRect(x0, cy - lo, cx, cy - lo + width - 1, c);
	if (bits & ROAD_SW) FillRect(cx, cy - lo, x1, cy - lo + width - 1, c);
}

static void DrawBlock(int x0, int y0, int x1, int y1, int ppt, uint32_t fill, uint32_t border)
{
	int inset = std::max(1, ppt / 10);
	int b = std::max(1, ppt / 10);
	FillRect(x0 + inset, y0 + inset, x1 - inset, y1 - inset, border);
	FillRect(x0 + inset + b, y0 + inset + b, x1 - inset - b, y1 - inset - b, fill);
}

static void DrawAxisBand(Axis axis, int x0, int y0, int x1, int y1, int width, uint32_t c)
{
	int cx = (x0 + x1) / 2;
	int cy = (y0 + y1) / 2;
	int lo = width / 2;
	if (axis == AXIS_X) {
		FillRect(x0, cy - lo, x1, cy - lo + width - 1, c);
	} else {
		FillRect(cx - lo, y0, cx - lo + width - 1, y1, c);
	}
}

static void DrawTile(TileIndex tile, int tx, int ty, int ppt)
{
	int x0 = PxX(tx);
	int y0 = PxY(ty);
	int x1 = PxX(tx + 1) - 1;
	int y1 = PxY(ty + 1) - 1;
	if (x1 < 0 || y1 < 0 || x0 >= _fbw || y0 >= _fbh) return;

	int rail_w = std::max(1, ppt / 6);
	int road_w = std::max(2, ppt / 3);

	bool water_tile = false;

	switch (GetTileType(tile)) {
		case MP_VOID:
			FillRect(x0, y0, x1, y1, COL_VOID);
			return;

		case MP_WATER:
			FillRect(x0, y0, x1, y1, COL_WATER);
			water_tile = true;
			if (IsShipDepot(tile)) DrawBlock(x0, y0, x1, y1, ppt, COL_ROAD, COL_RAIL);
			break;

		case MP_CLEAR:
			DrawGround(tile, x0, y0, x1, y1, ppt);
			break;

		case MP_TREES: {
			DrawGround(tile, x0, y0, x1, y1, ppt);
			int cx = (x0 + x1) / 2;
			int cy = (y0 + y1) / 2;
			int r = std::max(1, ppt / 8);
			FillRect(cx - r, cy - r, cx + r, cy + r, COL_TREE);
			break;
		}

		case MP_RAILWAY:
			DrawGround(tile, x0, y0, x1, y1, ppt);
			if (IsRailDepot(tile)) {
				DrawBlock(x0, y0, x1, y1, ppt, COL_ROAD, COL_RAIL);
			} else {
				DrawTrackBitsPx(GetTrackBits(tile), x0, y0, x1, y1, rail_w, COL_RAIL);
			}
			break;

		case MP_ROAD:
			DrawGround(tile, x0, y0, x1, y1, ppt);
			if (IsLevelCrossing(tile)) {
				DrawAxisBand(GetCrossingRoadAxis(tile), x0, y0, x1, y1, road_w, COL_ROAD);
				DrawTrackBitsPx(GetCrossingRailBits(tile), x0, y0, x1, y1, rail_w, COL_RAIL);
			} else if (IsRoadDepot(tile)) {
				DrawBlock(x0, y0, x1, y1, ppt, COL_ROAD, COL_RAIL);
			} else {
				RoadBits bits = GetAnyRoadBits(tile, RTT_ROAD, true) | GetAnyRoadBits(tile, RTT_TRAM, true);
				DrawRoadBitsPx(bits, x0, y0, x1, y1, road_w, COL_ROAD);
			}
			break;

		case MP_HOUSE:
			DrawGround(tile, x0, y0, x1, y1, ppt);
			DrawBlock(x0, y0, x1, y1, ppt, COL_HOUSE, COL_HOUSE_B);
			break;

		case MP_INDUSTRY:
			DrawGround(tile, x0, y0, x1, y1, ppt);
			DrawBlock(x0, y0, x1, y1, ppt, COL_IND, COL_IND_B);
			break;

		case MP_STATION: {
			uint32_t fill, border;
			bool on_water = false;
			switch (GetStationType(tile)) {
				case StationType::Rail:
				case StationType::RailWaypoint: fill = 0xFF4A6FA5U; border = 0xFF2F4A73U; break;
				case StationType::Airport: fill = 0xFF8E6FB8U; border = 0xFF64488AU; break;
				case StationType::Truck:
				case StationType::Bus:
				case StationType::RoadWaypoint: fill = 0xFF7FA8C9U; border = 0xFF54789AU; break;
				case StationType::Dock: fill = 0xFF9A7FA8U; border = 0xFF6E5479U; on_water = true; break;
				case StationType::Buoy: fill = 0xFFD8C86AU; border = 0xFFA6963FU; on_water = true; break;
				default: fill = COL_OBJ; border = COL_OBJ_B; on_water = true; break;
			}
			if (on_water) {
				FillRect(x0, y0, x1, y1, COL_WATER);
				water_tile = true;
			} else {
				DrawGround(tile, x0, y0, x1, y1, ppt);
			}
			DrawBlock(x0, y0, x1, y1, ppt, fill, border);
			if (HasStationRail(tile)) DrawAxisBand(GetRailStationAxis(tile), x0, y0, x1, y1, rail_w, COL_RAIL);
			break;
		}

		case MP_OBJECT:
			DrawGround(tile, x0, y0, x1, y1, ppt);
			DrawBlock(x0, y0, x1, y1, ppt, COL_OBJ, COL_OBJ_B);
			break;

		case MP_TUNNELBRIDGE: {
			DrawGround(tile, x0, y0, x1, y1, ppt);
			Axis axis = DiagDirToAxis(GetTunnelBridgeDirection(tile));
			if (IsTunnel(tile)) {
				DrawBlock(x0, y0, x1, y1, ppt, COL_TUNNEL, COL_RAIL);
			} else {
				DrawAxisBand(axis, x0, y0, x1, y1, road_w, COL_BRIDGE);
			}
			break;
		}

		default:
			FillRect(x0, y0, x1, y1, COL_OBJ);
			break;
	}

	if (IsBridgeAbove(tile)) {
		DrawAxisBand(GetBridgeAxis(tile), x0, y0, x1, y1, road_w, COL_BRIDGE);
	}

	if (!water_tile) {
		int cw = std::max(1, ppt / 8);
		uint h = TileHeight(tile);
		if (tx + 1 < (int)Map::SizeX() && TileHeight(TileXY(tx + 1, ty)) != h) BlendRect(x1 - cw + 1, y0, x1, y1, 0xFF000000U, _ms.contour_alpha);
		if (ty + 1 < (int)Map::SizeY() && TileHeight(TileXY(tx, ty + 1)) != h) BlendRect(x0, y1 - cw + 1, x1, y1, 0xFF000000U, _ms.contour_alpha);
	}
}

static void DrawVehicles(int ppt)
{
	int half = std::max(3, ppt * 2 / 5) / 2;
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->type > VEH_AIRCRAFT) continue;
		if (v->vehstatus.Test(VehState::Hidden)) continue;
		int cx = PxX(v->x_pos / (double)TILE_SIZE);
		int cy = PxY(v->y_pos / (double)TILE_SIZE);
		if (cx < -half || cy < -half || cx >= _fbw + half || cy >= _fbh + half) continue;
		uint32_t c = Company::IsValidID(v->owner) ? _company_rgb[_company_colours[v->owner]] : COL_OBJ;
		FillRect(cx - half - 1, cy - half - 1, cx + half + 1, cy + half + 1, 0xFF14181CU);
		FillRect(cx - half, cy - half, cx + half, cy + half, c);
	}
}

/* Mirrors the zigzag walk of CmdRailTrackHelper: non-diagonal pieces alternate
 * between the two halves of the pair while stepping one tile per piece. */
static void WalkRail(TileIndex start, Track track, int sx, int sy, int steps, std::vector<std::pair<TileIndex, Track>> &out)
{
	int tx = TileX(start);
	int ty = TileY(start);
	Track t = track;
	for (int i = 0; i <= steps; i++) {
		if (tx < 0 || ty < 0 || tx >= (int)Map::SizeX() - 1 || ty >= (int)Map::SizeY() - 1) break;
		out.emplace_back(TileXY(tx, ty), t);
		if (i == steps) break;
		switch (t) {
			case TRACK_X: tx += sx; break;
			case TRACK_Y: ty += sy; break;
			case TRACK_UPPER: if (sy < 0) ty--; else tx--; t = TRACK_LOWER; break;
			case TRACK_LOWER: if (sy < 0) tx++; else ty++; t = TRACK_UPPER; break;
			case TRACK_LEFT: if (sx > 0) tx++; else ty--; t = TRACK_RIGHT; break;
			case TRACK_RIGHT: if (sx > 0) ty++; else tx--; t = TRACK_LEFT; break;
			default: return;
		}
	}
}

static void UpdateRailPlan(double wx, double wy)
{
	_plan.pieces.clear();
	_plan.start = INVALID_TILE;

	int atx = Clamp<int>((int)std::floor(_drag_ax), 0, Map::SizeX() - 2);
	int aty = Clamp<int>((int)std::floor(_drag_ay), 0, Map::SizeY() - 2);
	double dx = wx - _drag_ax;
	double dy = wy - _drag_ay;

	TileIndex start = TileXY(atx, aty);
	double ax = std::abs(dx), ay = std::abs(dy);
	int sx = dx >= 0 ? 1 : -1;
	int sy = dy >= 0 ? 1 : -1;

	Track track;
	int steps;
	/* Snap to the nearest of the four rail directions by comparing axis dominance. */
	if (ax > ay * 2.414) {
		track = TRACK_X;
		steps = std::min<int>((int)std::lround(ax), 127);
	} else if (ay > ax * 2.414) {
		track = TRACK_Y;
		steps = std::min<int>((int)std::lround(ay), 127);
	} else {
		steps = std::min<int>((int)std::lround(ax + ay), 254);
		double fx = _drag_ax - std::floor(_drag_ax);
		double fy = _drag_ay - std::floor(_drag_ay);
		if (sx != sy) {
			track = (fx + fy < 1.0) ? TRACK_UPPER : TRACK_LOWER;
		} else {
			track = (fx > fy) ? TRACK_LEFT : TRACK_RIGHT;
		}
	}

	WalkRail(start, track, sx, sy, steps, _plan.pieces);
	if (_plan.pieces.empty()) return;

	_plan.start = _plan.pieces.front().first;
	_plan.end = _plan.pieces.back().first;
	_plan.track = _plan.pieces.front().second;
}

static void DrawRailPlan(int ppt)
{
	uint32_t c = _drag_remove ? COL_BP_RM : COL_BP;
	int w = std::max(2, ppt / 5);
	for (const auto &[tile, t] : _plan.pieces) {
		int tx = TileX(tile);
		int ty = TileY(tile);
		int x0 = PxX(tx);
		int y0 = PxY(ty);
		int x1 = PxX(tx + 1) - 1;
		int y1 = PxY(ty + 1) - 1;
		DrawTrackPiece(t, x0, y0, x1, y1, w, c);
	}
}

static RailType PickRailType()
{
	const Company *c = Company::GetIfValid(_local_company);
	if (c != nullptr) {
		for (RailType rt = RAILTYPE_BEGIN; rt != RAILTYPE_END; rt++) {
			if (c->avail_railtypes.Test(rt)) return rt;
		}
	}
	return RAILTYPE_RAIL;
}

static void CommitRailPlan()
{
	if (_plan.start == INVALID_TILE) return;
	if (_drag_remove) {
		Command<CMD_REMOVE_RAILROAD_TRACK>::Post(_plan.end, _plan.start, _plan.track);
	} else {
		Command<CMD_BUILD_RAILROAD_TRACK>::Post(_plan.end, _plan.start, PickRailType(), _plan.track, true, false);
	}
	_plan.pieces.clear();
	_plan.start = INVALID_TILE;
}

static void DrawCursor()
{
	int x = _cursor.pos.x;
	int y = _cursor.pos.y;
	FillRect(x - 9, y - 1, x + 9, y + 1, 0xFF14181CU);
	FillRect(x - 1, y - 9, x + 1, y + 9, 0xFF14181CU);
	FillRect(x - 8, y, x + 8, y, 0xFFEDF2F7U);
	FillRect(x, y - 8, x, y + 8, 0xFFEDF2F7U);
}

static std::string FormatMoney(int64_t m)
{
	bool neg = m < 0;
	uint64_t v = neg ? (uint64_t)-m : (uint64_t)m;
	std::string s;
	int group = 0;
	do {
		s.insert(s.begin(), (char)('0' + v % 10));
		v /= 10;
		if (++group == 3 && v != 0) {
			s.insert(s.begin(), ',');
			group = 0;
		}
	} while (v != 0);
	if (neg) s.insert(s.begin(), '-');
	return s;
}

static void DrawHud()
{
	static const std::string_view months[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
	int s = _ms.hud_scale;
	TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::date);
	std::string line = fmt::format("{} {} {}", ymd.day, months[ymd.month], ymd.year.base());
	const Company *c = Company::GetIfValid(_local_company);
	if (c != nullptr) line += fmt::format("   {}", FormatMoney((int64_t)c->money));
	DrawText(6 * s, 6 * s, s, line);

	if (_pause_mode.Any()) DrawText(_fbw / 2 - 3 * 6 * s, 6 * s, s, "PAUSED");

	std::string_view hint = _tool == MiniTool::Rail
			? "RAIL: DRAG BUILD / CTRL DRAG REMOVE / RMB CANCEL"
			: "R RAIL   SPACE PAUSE   F9 EXIT";
	DrawText(6 * s, _fbh - 13 * s, s, hint);
}

static void Present()
{
	uint32_t *dst = (uint32_t *)_screen.dst_ptr;
	for (int y = 0; y < _fbh; y++) {
		std::copy_n(_fb.data() + (size_t)y * _fbw, _fbw, dst + (size_t)y * _screen.pitch);
	}
	/* The 40bpp-anim path composes the screen from colour and palette-index
	 * buffers in a shader; stale indexes override direct colour writes, so
	 * clear them or the old interface stays baked over the frame. */
	if (uint8_t *anim = VideoDriver::GetInstance()->GetAnimBuffer(); anim != nullptr) {
		for (int y = 0; y < _fbh; y++) {
			std::fill_n(anim + (size_t)y * _screen.pitch, _fbw, 0);
		}
	}
	VideoDriver::GetInstance()->MakeDirty(0, 0, _fbw, _fbh);
}

static void ClampCamera()
{
	_cam_x = Clamp<double>(_cam_x, 0.0, (double)Map::SizeX());
	_cam_y = Clamp<double>(_cam_y, 0.0, (double)Map::SizeY());
}

static void ZoomAt(int sx, int sy, bool in)
{
	_dest_ppt = Clamp(_dest_ppt * (in ? _ms.zoom_step : 1.0 / _ms.zoom_step), MIN_PPT, MAX_PPT);
	/* Anchor the world point under the cursor; the camera follows it every
	 * frame while the scale animates, so the point never drifts. */
	_zoom_sx = sx;
	_zoom_sy = sy;
	_zoom_wx = WorldX(sx);
	_zoom_wy = WorldY(sy);
	_zoom_anchored = true;
}

static void Deactivate()
{
	_mini_active = false;
	_tool = MiniTool::None;
	_dragging = false;
	_zoom_anchored = false;
	_plan.pieces.clear();
	_plan.start = INVALID_TILE;
	MarkWholeScreenDirty();
}

void MiniUiToggle()
{
	if (_mini_active) {
		Deactivate();
		return;
	}
	if (_game_mode != GM_NORMAL && _game_mode != GM_EDITOR) return;
	if (BlitterFactory::GetCurrentBlitter()->GetScreenDepth() != 32) return;

	LoadMiniSettings();
	UndrawMouseCursor();
	/* One palette-driven fill resets the 32bpp-anim mapping buffer, so later
	 * direct framebuffer writes are not overwritten by palette animation. */
	GfxFillRect(0, 0, _screen.width - 1, _screen.height - 1, PC_BLACK);

	if (Window *w = GetMainWindow(); w != nullptr && w->viewport != nullptr) {
		Point centre = InverseRemapCoords(w->viewport->virtual_left + w->viewport->virtual_width / 2, w->viewport->virtual_top + w->viewport->virtual_height / 2);
		_cam_x = centre.x / (double)TILE_SIZE;
		_cam_y = centre.y / (double)TILE_SIZE;
	} else {
		_cam_x = Map::SizeX() / 2.0;
		_cam_y = Map::SizeY() / 2.0;
	}
	ClampCamera();
	_prev_left = _left_button_down;
	_mini_active = true;
}

bool MiniUiHandleMouseEvents()
{
	if (!_mini_active) return false;

	if (_middle_button_down && (_cursor.delta.x != 0 || _cursor.delta.y != 0)) {
		_zoom_anchored = false;
		_cam_x -= _cursor.delta.x * _ms.drag_pan_multiplier / _cam_ppt;
		_cam_y -= _cursor.delta.y * _ms.drag_pan_multiplier / _cam_ppt;
		ClampCamera();
	}

	if (_cursor.wheel != 0) {
		ZoomAt(_cursor.pos.x, _cursor.pos.y, _cursor.wheel < 0);
		_cursor.wheel = 0;
	}

	if (_left_button_down && !_left_button_clicked) {
		_left_button_clicked = true;
		if (_tool == MiniTool::Rail) {
			_dragging = true;
			_drag_remove = _ctrl_pressed;
			_drag_ax = WorldX(_cursor.pos.x);
			_drag_ay = WorldY(_cursor.pos.y);
			UpdateRailPlan(_drag_ax, _drag_ay);
		}
	}

	if (!_left_button_down && _prev_left && _dragging) {
		_dragging = false;
		CommitRailPlan();
	}
	_prev_left = _left_button_down;

	if (_right_button_clicked) {
		_right_button_clicked = false;
		if (_dragging) {
			_dragging = false;
			_plan.pieces.clear();
			_plan.start = INVALID_TILE;
		} else {
			_tool = MiniTool::None;
		}
	}

	_cursor.delta.x = 0;
	_cursor.delta.y = 0;
	_cursor.wheel_moved = false;
	return true;
}

bool MiniUiHandleKeypress(uint keycode, char32_t)
{
	uint kc = keycode & ~WKC_SPECIAL_KEYS;

	if (!_mini_active) {
		if (kc == WKC_F9) {
			MiniUiToggle();
			return _mini_active;
		}
		return false;
	}

	switch (kc) {
		case WKC_F9:
			Deactivate();
			break;

		case WKC_ESC:
			if (_dragging) {
				_dragging = false;
				_plan.pieces.clear();
				_plan.start = INVALID_TILE;
			} else if (_tool != MiniTool::None) {
				_tool = MiniTool::None;
			} else {
				Deactivate();
			}
			break;

		case 'R':
			_tool = _tool == MiniTool::Rail ? MiniTool::None : MiniTool::Rail;
			break;

		case WKC_SPACE:
			Command<CMD_PAUSE>::Post(PauseMode::Normal, !_pause_mode.Test(PauseMode::Normal));
			break;

		default:
			break;
	}
	return true;
}

bool MiniUiFrame(uint delta_ms)
{
	if (!_mini_active) return false;
	if (_game_mode != GM_NORMAL && _game_mode != GM_EDITOR) {
		Deactivate();
		return false;
	}

	if (_fbw != _screen.width || _fbh != _screen.height) {
		_fbw = _screen.width;
		_fbh = _screen.height;
		_fb.assign((size_t)_fbw * _fbh, COL_VOID);
	}
	if (_fbw <= 0 || _fbh <= 0) return true;

	/* WASD and arrows arrive via _dirkeys; pan speed is constant in screen space. */
	if (_dirkeys != 0) {
		_zoom_anchored = false;
		double px = (_shift_pressed ? _ms.pan_speed_fast : _ms.pan_speed) * delta_ms / 1000.0 / _cam_ppt;
		if (_dirkeys & 1) _cam_x -= px;
		if (_dirkeys & 2) _cam_y -= px;
		if (_dirkeys & 4) _cam_x += px;
		if (_dirkeys & 8) _cam_y += px;
		ClampCamera();
	}

	if (_ms.edge_scroll != 0 && _cursor.in_window && !_middle_button_down) {
		double px = _ms.edge_scroll_speed * delta_ms / 1000.0 / _cam_ppt;
		double ex = 0.0, ey = 0.0;
		if (_cursor.pos.x < _ms.edge_margin) ex = -px;
		if (_cursor.pos.x >= _fbw - _ms.edge_margin) ex = px;
		if (_cursor.pos.y < _ms.edge_margin) ey = -px;
		if (_cursor.pos.y >= _fbh - _ms.edge_margin) ey = px;
		if (ex != 0.0 || ey != 0.0) {
			_zoom_anchored = false;
			_cam_x += ex;
			_cam_y += ey;
			ClampCamera();
		}
	}

	if (_cam_ppt != _dest_ppt) {
		double f = 1.0 - std::exp(delta_ms / -_ms.zoom_smooth_ms);
		_cam_ppt = std::exp(std::log(_cam_ppt) + (std::log(_dest_ppt) - std::log(_cam_ppt)) * f);
		if (std::abs(_dest_ppt - _cam_ppt) < _dest_ppt * 0.002) _cam_ppt = _dest_ppt;
	}
	if (_zoom_anchored) {
		_cam_x = _zoom_wx - (_zoom_sx - _fbw * 0.5) / _cam_ppt;
		_cam_y = _zoom_wy - (_zoom_sy - _fbh * 0.5) / _cam_ppt;
		ClampCamera();
		if (_cam_ppt == _dest_ppt) _zoom_anchored = false;
	}

	int ppt = std::max(1, (int)std::lround(_cam_ppt));

	int tx0 = std::max(0, (int)std::floor(WorldX(0)));
	int ty0 = std::max(0, (int)std::floor(WorldY(0)));
	int tx1 = std::min<int>(Map::SizeX() - 1, (int)std::floor(WorldX(_fbw - 1)));
	int ty1 = std::min<int>(Map::SizeY() - 1, (int)std::floor(WorldY(_fbh - 1)));

	std::fill(_fb.begin(), _fb.end(), COL_VOID);
	for (int ty = ty0; ty <= ty1; ty++) {
		for (int tx = tx0; tx <= tx1; tx++) {
			DrawTile(TileXY(tx, ty), tx, ty, ppt);
		}
	}

	if (_dragging) {
		UpdateRailPlan(WorldX(_cursor.pos.x), WorldY(_cursor.pos.y));
		DrawRailPlan(ppt);
	} else if (_tool == MiniTool::Rail) {
		int htx = (int)std::floor(WorldX(_cursor.pos.x));
		int hty = (int)std::floor(WorldY(_cursor.pos.y));
		if (htx >= 0 && hty >= 0 && htx < (int)Map::SizeX() && hty < (int)Map::SizeY()) {
			BlendRect(PxX(htx), PxY(hty), PxX(htx + 1) - 1, PxY(hty + 1) - 1, COL_BP, 70);
		}
	}

	DrawVehicles(ppt);
	DrawHud();
	DrawCursor();
	Present();
	return true;
}
