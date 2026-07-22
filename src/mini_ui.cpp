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
#include "cargotype.h"
#include "clear_map.h"
#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "elrail_func.h"
#include "engine_base.h"
#include "core/backup_type.hpp"
#include "core/math_func.hpp"
#include "fileio_func.h"
#include "gfx_func.h"
#include "ground_vehicle.hpp"
#include "ini_type.h"
#include "landscape.h"
#include "landscape_cmd.h"
#include "misc_cmd.h"
#include "network/network_type.h"
#include "newgrf_roadstop.h"
#include "newgrf_station.h"
#include "openttd.h"
#include "order_base.h"
#include "order_cmd.h"
#include "rail.h"
#include "palette_func.h"
#include "rail_cmd.h"
#include "rail_map.h"
#include "road.h"
#include "road_cmd.h"
#include "road_map.h"
#include "settings_type.h"
#include "slope_func.h"
#include "station_base.h"
#include "station_cmd.h"
#include "station_func.h"
#include "strings_func.h"
#include "station_map.h"
#include "terraform_cmd.h"
#include "town.h"
#include "tile_map.h"
#include "timer/timer_game_calendar.h"
#include "tunnelbridge_cmd.h"
#include "tunnelbridge_map.h"
#include "vehicle_base.h"
#include "vehicle_cmd.h"
#include "video/video_driver.hpp"
#include "water_map.h"
#include "window_func.h"
#include "window_gui.h"

#include "table/strings.h"

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
	Road,
	Station,
	BusStop,
	TruckStop,
	TrainDepot,
	RoadDepot,
	Demolish,
	Signal,
	RailBridge,
	RoadBridge,
	Terraform,
};

static bool IsRectTool(MiniTool t)
{
	return t == MiniTool::Station || t == MiniTool::Demolish || t == MiniTool::Terraform;
}

static bool IsBridgeTool(MiniTool t)
{
	return t == MiniTool::RailBridge || t == MiniTool::RoadBridge;
}

static bool IsPointTool(MiniTool t)
{
	return t == MiniTool::BusStop || t == MiniTool::TruckStop || t == MiniTool::TrainDepot || t == MiniTool::RoadDepot || t == MiniTool::Signal;
}

static MiniTool _tool = MiniTool::None;
static bool _dragging = false;
static bool _drag_remove = false;
static double _drag_ax, _drag_ay;

static VehicleID _sel_vehicle = VehicleID::Invalid();
static bool _follow = false;

static bool _prev_left = false;

struct MiniRailPlan {
	TileIndex start = INVALID_TILE;
	TileIndex end = INVALID_TILE;
	Track track = INVALID_TRACK;
	std::vector<std::pair<TileIndex, Track>> pieces;
};

static MiniRailPlan _plan;

struct MiniRoadPlan {
	TileIndex start = INVALID_TILE;
	TileIndex end = INVALID_TILE;
	Axis axis = AXIS_X;
	std::vector<TileIndex> tiles;
};

static MiniRoadPlan _road_plan;

struct MiniRectPlan {
	bool valid = false;
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};

static MiniRectPlan _rect_plan;

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
	double jump_ppt = 16.0;
	double glide_ms = 250.0;
};

static MiniSettings _ms;

static bool _zoom_anchored = false;
static int _zoom_sx, _zoom_sy;
static double _zoom_wx, _zoom_wy;

static bool _glide = false;
static double _glide_x, _glide_y;

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
	ReadIniNumber(group, "jump_ppt", _ms.jump_ppt);
	ReadIniNumber(group, "glide_ms", _ms.glide_ms);

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
	_ms.jump_ppt = Clamp(_ms.jump_ppt, MIN_PPT, MAX_PPT);
	_ms.glide_ms = Clamp(_ms.glide_ms, 1.0, 2000.0);

	ini.SaveToDisk(path);
}

static constexpr uint32_t Mix(uint32_t dst, uint32_t src, uint alpha)
{
	uint inv = 255 - alpha;
	uint32_t rb = ((dst & 0xFF00FFU) * inv + (src & 0xFF00FFU) * alpha) >> 8;
	uint32_t g = ((dst & 0x00FF00U) * inv + (src & 0x00FF00U) * alpha) >> 8;
	return 0xFF000000U | (rb & 0xFF00FFU) | (g & 0x00FF00U);
}

/* Ink draws outlines, paper draws highlights; everything else is a fill. */
static const uint32_t COL_INK = 0xFF14181CU;
static const uint32_t COL_PAPER = 0xFFEDF2F7U;
static const uint32_t COL_SHADOW = 0xFF000000U;

/* Every bordered block darkens its fill by the same cut. */
static constexpr uint32_t Darken(uint32_t c)
{
	return Mix(c, COL_SHADOW, 82);
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
static const uint32_t COL_BRIDGE = 0xFF9AA0A6U;
static const uint32_t COL_TUNNEL = 0xFF1E2124U;
static const uint32_t COL_CATENARY = 0xFFE8C94AU;
static const uint32_t COL_DEPOT = 0xFF3A3F45U;

static const uint32_t COL_HOUSE = 0xFF9C8A76U;
static const uint32_t COL_HOUSE_B = Darken(COL_HOUSE);
static const uint32_t COL_IND = 0xFFD07A4AU;
static const uint32_t COL_IND_B = Darken(COL_IND);
static const uint32_t COL_OBJ = 0xFFB0B4B8U;
static const uint32_t COL_OBJ_B = Darken(COL_OBJ);

static const uint32_t COL_ST_RAIL = 0xFF4A6FA5U;
static const uint32_t COL_ST_RAIL_B = Darken(COL_ST_RAIL);
static const uint32_t COL_ST_AIR = 0xFF8E6FB8U;
static const uint32_t COL_ST_AIR_B = Darken(COL_ST_AIR);
static const uint32_t COL_ST_ROAD = 0xFF7FA8C9U;
static const uint32_t COL_ST_ROAD_B = Darken(COL_ST_ROAD);
static const uint32_t COL_ST_DOCK = 0xFF9A7FA8U;
static const uint32_t COL_ST_DOCK_B = Darken(COL_ST_DOCK);
static const uint32_t COL_ST_BUOY = 0xFFD8C86AU;
static const uint32_t COL_ST_BUOY_B = Darken(COL_ST_BUOY);

static const uint32_t COL_GO = 0xFF3FCB6AU;
static const uint32_t COL_STOP = 0xFFE04B4BU;
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

static void FillCircle(int cx, int cy, int r, uint32_t c)
{
	for (int dy = -r; dy <= r; dy++) {
		int w = (int)std::lround(std::sqrt((double)(r * r - dy * dy)));
		FillRect(cx - w, cy + dy, cx + w, cy + dy, c);
	}
}

static void FillDiamond(int cx, int cy, int r, uint32_t c)
{
	for (int dy = -r; dy <= r; dy++) {
		int w = r - abs(dy);
		FillRect(cx - w, cy + dy, cx + w, cy + dy, c);
	}
}

static void FillTriangle(int cx, int cy, int r, uint32_t c)
{
	for (int dy = -r; dy <= r; dy++) {
		int w = (dy + r) / 2;
		FillRect(cx - w, cy + dy, cx + w, cy + dy, c);
	}
}

/* Text goes through the native font cache onto _screen after Present() has
 * copied the frame, so any TrueType fallback font covers non-Latin names. */
static void DrawScreenText(int x, int y, std::string_view text, TextColour colour = TC_WHITE)
{
	AutoRestoreBackup dpi_backup(_cur_dpi, &_screen);
	DrawString(x, _fbw - 1, y, text, colour, SA_LEFT | SA_FORCE);
}

static void DrawScreenTextCentred(int cx, int y, std::string_view text, TextColour colour = TC_WHITE)
{
	AutoRestoreBackup dpi_backup(_cur_dpi, &_screen);
	int half = GetStringBoundingBox(text).width / 2 + 1;
	DrawString(cx - half, cx + half, y, text, colour, SA_HOR_CENTER | SA_FORCE);
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

static uint32_t RampLerp(double h)
{
	h = Clamp(h, 0.0, 15.0);
	int i = (int)h;
	if (i >= 15) return _height_ramp[15];
	return Mix(_height_ramp[i], _height_ramp[i + 1], (uint)((h - i) * 255.0));
}

static bool IsSpecialGround(TileIndex tile)
{
	if (GetTileType(tile) != MP_CLEAR) return false;
	switch (GetClearGround(tile)) {
		case CLEAR_FIELDS:
		case CLEAR_ROCKS:
		case CLEAR_SNOW:
		case CLEAR_DESERT:
			return true;
		default:
			return false;
	}
}

/* Sloped ramp ground samples the height ramp per subcell at its absolute
 * interpolated height, endpoint inclusive, so the top of a slope lands on
 * exactly the colour of the next level and gradients run tile to tile. */
static void DrawGround(TileIndex tile, int x0, int y0, int x1, int y1, int ppt)
{
	auto [s, hbase] = GetTileSlopeZ(tile);
	if (s == SLOPE_FLAT) {
		FillRect(x0, y0, x1, y1, GroundColour(tile, hbase));
		return;
	}

	bool special = IsSpecialGround(tile);
	uint32_t flat = GroundColour(tile, hbase);
	int hn = GetSlopeZInCorner(s, CORNER_N);
	int hw = GetSlopeZInCorner(s, CORNER_W);
	int he = GetSlopeZInCorner(s, CORNER_E);
	int hs = GetSlopeZInCorner(s, CORNER_S);

	if (ppt < 8) {
		double avg = (hn + hw + he + hs) / 4.0;
		if (special) {
			FillRect(x0, y0, x1, y1, Mix(flat, COL_SHADOW, std::min(255, (int)(_ms.relief_strength * avg))));
		} else {
			FillRect(x0, y0, x1, y1, RampLerp(hbase + avg));
		}
		return;
	}

	const int sub = Clamp(ppt / 4, 2, 12);
	int wpx = x1 - x0 + 1;
	int hpx = y1 - y0 + 1;
	double denom = (sub - 1) * (sub - 1);
	for (int j = 0; j < sub; j++) {
		for (int i = 0; i < sub; i++) {
			int top = hn * (sub - 1 - i) + hw * i;
			int bot = he * (sub - 1 - i) + hs * i;
			double z = (top * (sub - 1 - j) + bot * j) / denom;
			int sx0 = x0 + wpx * i / sub;
			int sy0 = y0 + hpx * j / sub;
			int sx1 = x0 + wpx * (i + 1) / sub - 1;
			int sy1 = y0 + hpx * (j + 1) / sub - 1;
			if (sx1 < sx0 || sy1 < sy0) continue;
			uint32_t c = special ? Mix(flat, COL_SHADOW, std::min(255, (int)(_ms.relief_strength * z))) : RampLerp(hbase + z);
			FillRect(sx0, sy0, sx1, sy1, c);
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

struct ZoomDetail {
	bool tree_dots;
	bool block_borders;
	bool signals;
	bool oneway;
	bool cargo_dots;
	bool vehicle_shapes;
	bool station_names;
};

static ZoomDetail _zd;

/* Zoom tiers: below 8 ppt the map is a terrain overview, mid zoom shows
 * infrastructure, close zoom adds per-unit detail. */
static void ComputeZoomDetail(int ppt)
{
	_zd.tree_dots = ppt >= 8;
	_zd.block_borders = ppt >= 8;
	_zd.signals = ppt >= 8;
	_zd.oneway = ppt >= 8;
	_zd.cargo_dots = ppt >= 16;
	_zd.vehicle_shapes = ppt >= 8;
	_zd.station_names = ppt >= 8;
}

static const int _diag_dx[4] = {-1, 0, 1, 0};
static const int _diag_dy[4] = {0, 1, 0, -1};

static void DrawSignals(TileIndex tile, int x0, int y0, int x1, int y1, int ppt)
{
	if (!_zd.signals) return;
	int r = std::max(1, ppt / 10);
	int cx = (x0 + x1) / 2;
	int cy = (y0 + y1) / 2;
	int off = (int)((x1 - x0 + 1) * 0.36);
	for (Track t : {TRACK_X, TRACK_Y, TRACK_UPPER, TRACK_LOWER, TRACK_LEFT, TRACK_RIGHT}) {
		if (!HasSignalOnTrack(tile, t)) continue;
		for (Trackdir td : {TrackToTrackdir(t), ReverseTrackdir(TrackToTrackdir(t))}) {
			if (!HasSignalOnTrackdir(tile, td)) continue;
			DiagDirection d = TrackdirToExitdir(td);
			int px = cx + _diag_dx[d] * off;
			int py = cy + _diag_dy[d] * off;
			uint32_t c = GetSignalStateByTrackdir(tile, td) == SIGNAL_STATE_GREEN ? COL_GO : COL_STOP;
			FillRect(px - r - 1, py - r - 1, px + r + 1, py + r + 1, COL_INK);
			FillRect(px - r, py - r, px + r, py + r, c);
		}
	}
}

static void DrawOneWay(TileIndex tile, int x0, int y0, int x1, int y1, int ppt)
{
	if (!_zd.oneway) return;
	DisallowedRoadDirections drd = GetDisallowedRoadDirections(tile);
	if (drd == DRD_NONE) return;

	int cx = (x0 + x1) / 2;
	int cy = (y0 + y1) / 2;
	int s = std::max(2, ppt / 4);

	if (drd == DRD_BOTH) {
		FillRect(cx - s, cy - s / 3, cx + s, cy + s / 3, COL_STOP);
		return;
	}

	RoadBits rb = GetRoadBits(tile, RTT_ROAD);
	bool axis_x = (rb & ROAD_X) == ROAD_X;
	bool axis_y = (rb & ROAD_Y) == ROAD_Y;
	if (axis_x == axis_y) return;

	/* Northbound traffic heads toward smaller map coordinates. */
	int dir = drd == DRD_SOUTHBOUND ? -1 : 1;
	for (int i = 0; i <= s; i++) {
		int w = (s - i) / 2;
		if (axis_x) {
			int px = cx + dir * (i - s / 2);
			FillRect(px, cy - w, px, cy + w, COL_PAPER);
		} else {
			int py = cy + dir * (i - s / 2);
			FillRect(cx - w, py, cx + w, py, COL_PAPER);
		}
	}
}

static void DrawBlock(int x0, int y0, int x1, int y1, int ppt, uint32_t fill, uint32_t border)
{
	if (!_zd.block_borders) {
		FillRect(x0, y0, x1, y1, fill);
		return;
	}
	int inset = std::max(1, ppt / 10);
	int b = std::max(1, ppt / 10);
	FillRect(x0 + inset, y0 + inset, x1 - inset, y1 - inset, border);
	FillRect(x0 + inset + b, y0 + inset + b, x1 - inset - b, y1 - inset - b, fill);
}

/* Dark block with a bright tick pointing out of the exit side. */
static void DrawDepot(int x0, int y0, int x1, int y1, int ppt, DiagDirection exit)
{
	DrawBlock(x0, y0, x1, y1, ppt, COL_DEPOT, COL_INK);
	if (!_zd.block_borders) return;
	int cx = (x0 + x1) / 2;
	int cy = (y0 + y1) / 2;
	int w = std::max(2, ppt / 5);
	ThickLine(cx, cy, cx + _diag_dx[exit] * (ppt / 2), cy + _diag_dy[exit] * (ppt / 2), w, COL_PAPER);
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
	int cat_w = rail_w >= 2 ? std::max(1, rail_w / 3) : 0;

	bool water_tile = false;

	switch (GetTileType(tile)) {
		case MP_VOID:
			FillRect(x0, y0, x1, y1, COL_VOID);
			return;

		case MP_WATER:
			FillRect(x0, y0, x1, y1, COL_WATER);
			water_tile = true;
			if (IsShipDepot(tile)) DrawDepot(x0, y0, x1, y1, ppt, GetShipDepotDirection(tile));
			break;

		case MP_CLEAR:
			DrawGround(tile, x0, y0, x1, y1, ppt);
			break;

		case MP_TREES: {
			DrawGround(tile, x0, y0, x1, y1, ppt);
			if (_zd.tree_dots) {
				int cx = (x0 + x1) / 2;
				int cy = (y0 + y1) / 2;
				int r = std::max(1, ppt / 8);
				FillRect(cx - r, cy - r, cx + r, cy + r, COL_TREE);
			}
			break;
		}

		case MP_RAILWAY:
			DrawGround(tile, x0, y0, x1, y1, ppt);
			if (IsRailDepot(tile)) {
				DrawDepot(x0, y0, x1, y1, ppt, GetRailDepotDirection(tile));
			} else {
				TrackBits bits = GetTrackBits(tile);
				DrawTrackBitsPx(bits, x0, y0, x1, y1, rail_w, COL_RAIL);
				if (cat_w > 0 && HasRailCatenary(GetRailType(tile))) {
					DrawTrackBitsPx(bits, x0, y0, x1, y1, cat_w, COL_CATENARY);
				}
				if (HasSignals(tile)) DrawSignals(tile, x0, y0, x1, y1, ppt);
			}
			break;

		case MP_ROAD:
			DrawGround(tile, x0, y0, x1, y1, ppt);
			if (IsLevelCrossing(tile)) {
				DrawAxisBand(GetCrossingRoadAxis(tile), x0, y0, x1, y1, road_w, COL_ROAD);
				DrawTrackBitsPx(GetCrossingRailBits(tile), x0, y0, x1, y1, rail_w, COL_RAIL);
			} else if (IsRoadDepot(tile)) {
				DrawDepot(x0, y0, x1, y1, ppt, GetRoadDepotDirection(tile));
			} else {
				RoadBits bits = GetAnyRoadBits(tile, RTT_ROAD, true) | GetAnyRoadBits(tile, RTT_TRAM, true);
				DrawRoadBitsPx(bits, x0, y0, x1, y1, road_w, COL_ROAD);
				if (IsNormalRoad(tile)) DrawOneWay(tile, x0, y0, x1, y1, ppt);
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
				case StationType::RailWaypoint: fill = COL_ST_RAIL; border = COL_ST_RAIL_B; break;
				case StationType::Airport: fill = COL_ST_AIR; border = COL_ST_AIR_B; break;
				case StationType::Truck:
				case StationType::Bus:
				case StationType::RoadWaypoint: fill = COL_ST_ROAD; border = COL_ST_ROAD_B; break;
				case StationType::Dock: fill = COL_ST_DOCK; border = COL_ST_DOCK_B; on_water = true; break;
				case StationType::Buoy: fill = COL_ST_BUOY; border = COL_ST_BUOY_B; on_water = true; break;
				default: fill = COL_OBJ; border = COL_OBJ_B; on_water = true; break;
			}
			if (on_water) {
				FillRect(x0, y0, x1, y1, COL_WATER);
				water_tile = true;
			} else {
				DrawGround(tile, x0, y0, x1, y1, ppt);
			}
			DrawBlock(x0, y0, x1, y1, ppt, fill, border);
			if (IsDriveThroughStopTile(tile)) {
				DrawAxisBand(GetDriveThroughStopAxis(tile), x0, y0, x1, y1, road_w, COL_ROAD);
			}
			if (HasStationRail(tile)) {
				DrawAxisBand(GetRailStationAxis(tile), x0, y0, x1, y1, rail_w, COL_RAIL);
				if (cat_w > 0 && HasRailCatenary(GetRailType(tile))) {
					DrawAxisBand(GetRailStationAxis(tile), x0, y0, x1, y1, cat_w, COL_CATENARY);
				}
			}
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
				if (cat_w > 0 && GetTunnelBridgeTransportType(tile) == TRANSPORT_RAIL && HasRailCatenary(GetRailType(tile))) {
					DrawAxisBand(axis, x0, y0, x1, y1, cat_w, COL_CATENARY);
				}
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
		if (tx + 1 < (int)Map::SizeX() && TileHeight(TileXY(tx + 1, ty)) != h) BlendRect(x1 - cw + 1, y0, x1, y1, COL_SHADOW, _ms.contour_alpha);
		if (ty + 1 < (int)Map::SizeY() && TileHeight(TileXY(tx, ty + 1)) != h) BlendRect(x0, y1 - cw + 1, x1, y1, COL_SHADOW, _ms.contour_alpha);
	}
}

static const int8_t _dir_dx[8] = {-1, -1, -1, 0, 1, 1, 1, 0};
static const int8_t _dir_dy[8] = {-1, 0, 1, 1, 1, 0, -1, -1};

static uint32_t CargoRgb(CargoType ct)
{
	Colour c = _cur_palette.palette[CargoSpec::Get(ct)->legend_colour.p];
	return 0xFF000000U | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

/* Silhouette tells the vehicle type apart: square train, round road
 * vehicle, diamond ship, triangle aircraft. The centre dot is the unit's
 * cargo in its legend colour. */
static void DrawVehicles(int ppt)
{
	int half = std::max(3, ppt * 2 / 5) / 2;
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->type > VEH_AIRCRAFT) continue;
		if (v->vehstatus.Test(VehState::Hidden)) continue;
		if (v->type == VEH_AIRCRAFT && !v->IsPrimaryVehicle()) continue;
		int r = (v->type == VEH_SHIP || v->type == VEH_AIRCRAFT) ? half + 2 : half;
		int cx = PxX(v->x_pos / (double)TILE_SIZE);
		int cy = PxY(v->y_pos / (double)TILE_SIZE);
		if (cx < -r - 1 || cy < -r - 1 || cx >= _fbw + r + 1 || cy >= _fbh + r + 1) continue;
		uint32_t c = Company::IsValidID(v->owner) ? _company_rgb[_company_colours[v->owner]] : COL_OBJ;
		if (!_zd.vehicle_shapes) {
			FillRect(cx - 1, cy - 1, cx + 1, cy + 1, c);
			continue;
		}
		switch (v->type) {
			case VEH_ROAD:
				FillCircle(cx, cy, r + 1, COL_INK);
				FillCircle(cx, cy, r, c);
				break;
			case VEH_SHIP:
				FillDiamond(cx, cy, r + 1, COL_INK);
				FillDiamond(cx, cy, r, c);
				break;
			case VEH_AIRCRAFT:
				FillTriangle(cx, cy, r + 1, COL_INK);
				FillTriangle(cx, cy, r, c);
				break;
			case VEH_TRAIN: {
				/* Each unit is a segment of its cached length along its heading,
				 * so a consist reads as one continuous line on the track. */
				double len = v->GetGroundVehicleCache()->cached_veh_length * ppt / (double)TILE_SIZE;
				double norm = (v->direction & 1) ? 0.5 : 0.35355339;
				int hx = (int)std::lround(_dir_dx[v->direction] * norm * len);
				int hy = (int)std::lround(_dir_dy[v->direction] * norm * len);
				int w = std::max(2, ppt / 4);
				ThickLine(cx - hx, cy - hy, cx + hx, cy + hy, w + 2, COL_INK);
				ThickLine(cx - hx, cy - hy, cx + hx, cy + hy, w, c);
				if (v->IsPrimaryVehicle()) {
					int tr = std::max(1, w / 2 - 1);
					FillCircle(cx + hx, cy + hy, tr, COL_PAPER);
				}
				break;
			}
			default:
				FillRect(cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1, COL_INK);
				FillRect(cx - r, cy - r, cx + r, cy + r, c);
				break;
		}
		if (_zd.cargo_dots && v->cargo_cap > 0 && IsValidCargoType(v->cargo_type)) {
			int dr = std::max(1, half - 2);
			FillCircle(cx, cy, dr + 1, COL_INK);
			FillCircle(cx, cy, dr, CargoRgb(v->cargo_type));
		}
	}
}

/* No engine list in the mini UI: the buy key auto-picks per depot type.
 * Locomotives go by power, everything else by capacity then speed; the
 * alternate mode buys wagons at rail depots and freight elsewhere. */
static EngineID PickEngine(TileIndex depot, VehicleType vt, bool alt)
{
	EngineID best = EngineID::Invalid();
	int64_t best_score = -1;
	for (const Engine *e : Engine::IterateType(vt)) {
		if (!e->IsEnabled() || !e->company_avail.Test(_local_company)) continue;
		CargoType ct = e->GetDefaultCargoType();
		bool pax = IsValidCargoType(ct) && IsCargoInClass(ct, CargoClass::Passengers);
		int64_t score;
		switch (vt) {
			case VEH_TRAIN: {
				const RailVehicleInfo &rvi = e->VehInfo<RailVehicleInfo>();
				bool wagon = rvi.railveh_type == RAILVEH_WAGON;
				if (wagon != alt) continue;
				if (wagon) {
					if (!IsCompatibleRail(rvi.railtypes, GetRailType(depot))) continue;
					if (!pax) continue;
					score = e->GetDisplayDefaultCapacity();
				} else {
					if (!HasPowerOnRail(rvi.railtypes, GetRailType(depot))) continue;
					score = e->GetPower();
				}
				break;
			}
			case VEH_ROAD: {
				const RoadVehicleInfo &rvi = e->VehInfo<RoadVehicleInfo>();
				RoadType depot_rt = GetRoadTypeRoad(depot) != INVALID_ROADTYPE ? GetRoadTypeRoad(depot) : GetRoadTypeTram(depot);
				if (!HasPowerOnRoad(rvi.roadtype, depot_rt)) continue;
				if (pax == alt) continue;
				score = (int64_t)e->GetDisplayDefaultCapacity() * 1000 + e->GetDisplayMaxSpeed();
				break;
			}
			case VEH_SHIP:
				if (pax == alt) continue;
				score = (int64_t)e->GetDisplayDefaultCapacity() * 1000 + e->GetDisplayMaxSpeed();
				break;
			default:
				continue;
		}
		if (score > best_score) {
			best_score = score;
			best = e->index;
		}
	}
	return best;
}

static void BuyAtDepot(bool alt)
{
	int tx = (int)std::floor(WorldX(_cursor.pos.x));
	int ty = (int)std::floor(WorldY(_cursor.pos.y));
	if (tx < 0 || ty < 0 || tx >= (int)Map::SizeX() || ty >= (int)Map::SizeY()) return;
	TileIndex tile = TileXY(tx, ty);

	VehicleType vt;
	if (IsRailDepotTile(tile)) {
		vt = VEH_TRAIN;
	} else if (IsRoadDepotTile(tile)) {
		vt = VEH_ROAD;
	} else if (IsTileType(tile, MP_WATER) && IsShipDepot(tile)) {
		vt = VEH_SHIP;
	} else {
		return;
	}
	if (GetTileOwner(tile) != _local_company) return;

	EngineID eid = PickEngine(tile, vt, alt);
	if (eid == EngineID::Invalid()) return;
	Command<CMD_BUILD_VEHICLE>::Post(tile, eid, true, INVALID_CARGO, INVALID_CLIENT_ID);
}

/* Clicking a compatible station with a vehicle selected appends a go-to
 * order, mirroring the defaults of the order window's goto click. */
static bool TryAppendOrder(int sx, int sy)
{
	const Vehicle *v = Vehicle::GetIfValid(_sel_vehicle);
	if (v == nullptr || !v->IsPrimaryVehicle() || v->owner != _local_company) return false;

	int tx = (int)std::floor(WorldX(sx));
	int ty = (int)std::floor(WorldY(sy));
	if (tx < 0 || ty < 0 || tx >= (int)Map::SizeX() || ty >= (int)Map::SizeY()) return false;
	TileIndex tile = TileXY(tx, ty);
	if (!IsTileType(tile, MP_STATION)) return false;
	switch (GetStationType(tile)) {
		case StationType::RailWaypoint:
		case StationType::RoadWaypoint:
		case StationType::Buoy:
			return false;
		default:
			break;
	}
	const Station *st = Station::GetByTile(tile);
	if (st == nullptr || (st->owner != _local_company && st->owner != OWNER_NONE)) return false;

	StationFacilities facil;
	switch (v->type) {
		case VEH_SHIP: facil = StationFacility::Dock; break;
		case VEH_TRAIN: facil = StationFacility::Train; break;
		case VEH_AIRCRAFT: facil = StationFacility::Airport; break;
		case VEH_ROAD: facil = {StationFacility::BusStop, StationFacility::TruckStop}; break;
		default: return false;
	}
	if (!st->facilities.Any(facil)) return false;

	Order order;
	order.MakeGoToStation(st->index);
	if (_ctrl_pressed) order.SetLoadType(OrderLoadType::FullLoadAny);
	if (_settings_client.gui.new_nonstop && v->IsGroundVehicle()) order.SetNonStopType(OrderNonStopFlag::NoIntermediate);
	order.SetStopLocation(v->type == VEH_TRAIN ? (OrderStopLocation)(_settings_client.gui.stop_location) : OrderStopLocation::FarEnd);
	Command<CMD_INSERT_ORDER>::Post(v->tile, v->index, (VehicleOrderID)v->GetNumOrders(), order);
	return true;
}

/* Any unit of a consist selects its head, so the info line always
 * describes the whole vehicle. */
static void SelectVehicleAt(int sx, int sy)
{
	const Vehicle *best = nullptr;
	int best_d2 = 15 * 15;
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->type > VEH_AIRCRAFT) continue;
		if (v->vehstatus.Test(VehState::Hidden)) continue;
		int dx = PxX(v->x_pos / (double)TILE_SIZE) - sx;
		int dy = PxY(v->y_pos / (double)TILE_SIZE) - sy;
		int d2 = dx * dx + dy * dy;
		if (d2 < best_d2) {
			best_d2 = d2;
			best = v;
		}
	}
	_sel_vehicle = best != nullptr ? best->First()->index : VehicleID::Invalid();
}

/* Route preview for the selected vehicle: stop-to-stop legs in blueprint
 * blue, the leg from the vehicle to its current destination highlighted. */
static void DrawOrderRoute()
{
	const Vehicle *v = Vehicle::GetIfValid(_sel_vehicle);
	if (v == nullptr || v->GetNumOrders() < 1) return;

	std::vector<std::pair<int, int>> stops;
	int cur_stop = -1;
	int i = 0;
	for (const Order &o : v->Orders()) {
		if (o.IsType(OT_GOTO_STATION)) {
			const Station *st = Station::GetIfValid(o.GetDestination().ToStationID());
			if (st != nullptr) {
				if (i == v->cur_real_order_index) cur_stop = (int)stops.size();
				stops.emplace_back(PxX(TileX(st->xy) + 0.5), PxY(TileY(st->xy) + 0.5));
			}
		}
		i++;
	}
	if (stops.empty()) return;

	size_t legs = stops.size() > 2 ? stops.size() : stops.size() - 1;
	for (size_t n = 0; n < legs; n++) {
		auto [x0, y0] = stops[n];
		auto [x1, y1] = stops[(n + 1) % stops.size()];
		ThickLine(x0, y0, x1, y1, 2, COL_BP);
	}
	for (auto [x, y] : stops) FillCircle(x, y, 4, COL_BP);

	if (cur_stop >= 0) {
		int vx = PxX(v->x_pos / (double)TILE_SIZE);
		int vy = PxY(v->y_pos / (double)TILE_SIZE);
		ThickLine(vx, vy, stops[cur_stop].first, stops[cur_stop].second, 2, COL_PAPER);
	}
}

static void DrawSelectionRing(int ppt)
{
	const Vehicle *v = Vehicle::GetIfValid(_sel_vehicle);
	if (v == nullptr) return;
	int cx = PxX(v->x_pos / (double)TILE_SIZE);
	int cy = PxY(v->y_pos / (double)TILE_SIZE);
	int r = std::max(6, ppt / 2 + 3);
	FillRect(cx - r, cy - r, cx + r, cy - r + 1, COL_PAPER);
	FillRect(cx - r, cy + r - 1, cx + r, cy + r, COL_PAPER);
	FillRect(cx - r, cy - r, cx - r + 1, cy + r, COL_PAPER);
	FillRect(cx + r - 1, cy - r, cx + r, cy + r, COL_PAPER);
}

/* Town names always show for navigation; station names join at the
 * infrastructure zoom tier. Labels sit centred above their sign tile. */
static std::vector<std::pair<Rect, TownID>> _town_label_hits;
static std::vector<std::pair<Rect, StationID>> _station_label_hits;

/* Same plate-and-string construction as the native viewport signs, drawn in
 * mini UI screen space because the sign kdtree lives in viewport coordinates. */
static Rect DrawLabelPlate(int cx, int cy, std::string_view str, Colours plate, bool transparent, TextColour tc)
{
	AutoRestoreBackup dpi_backup(_cur_dpi, &_screen);
	const RectPadding &bevel = WidgetDimensions::scaled.fullbevel;
	int w = GetStringBoundingBox(str).width + bevel.left + bevel.right + 4;
	int h = bevel.top + GetCharacterHeight(FS_NORMAL) + bevel.bottom;
	Rect r = {cx - w / 2, cy - h - 3, cx - w / 2 + w - 1, cy - 4};
	DrawFrameRect(r.left, r.top, r.right, r.bottom, plate, transparent ? FrameFlags{FrameFlag::Transparent} : FrameFlags{});
	DrawString(r.left + bevel.left, r.right - bevel.right, r.top + bevel.top, str, tc, SA_HOR_CENTER);
	return r;
}

static void DrawLabels()
{
	_town_label_hits.clear();
	_station_label_hits.clear();
	int margin = 300;
	int limit = GetCharacterHeight(FS_NORMAL) + 20;
	for (const Town *t : Town::Iterate()) {
		int cx = PxX(TileX(t->xy) + 0.5);
		int cy = PxY(TileY(t->xy) + 0.5);
		if (cx < -margin || cy < 0 || cx >= _fbw + margin || cy >= _fbh + limit) continue;
		std::string str = GetString(t->larger_town ? STR_VIEWPORT_TOWN_CITY_POP : STR_VIEWPORT_TOWN_POP, t->index, t->cache.population);
		Rect r = DrawLabelPlate(cx, cy, str, COLOUR_GREY, true, TC_WHITE);
		_town_label_hits.emplace_back(r, t->index);
	}
	if (!_zd.station_names) return;
	for (const Station *st : Station::Iterate()) {
		int cx = PxX(TileX(st->xy) + 0.5);
		int cy = PxY(TileY(st->xy) + 0.5);
		if (cx < -margin || cy < 0 || cx >= _fbw + margin || cy >= _fbh + limit) continue;
		std::string str = GetString(STR_VIEWPORT_STATION, st->index, st->facilities);
		Colours plate = (st->owner == OWNER_NONE || !st->IsInUse()) ? COLOUR_GREY : _company_colours[st->owner];
		Rect r = DrawLabelPlate(cx, cy, str, plate, false, TC_BLACK);
		_station_label_hits.emplace_back(r, st->index);
	}
}

static bool HandleLabelClick(int x, int y)
{
	for (const auto &[r, id] : _station_label_hits) {
		if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom) {
			ShowStationViewWindow(id);
			return true;
		}
	}
	for (const auto &[r, id] : _town_label_hits) {
		if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom) {
			ShowTownViewWindow(id);
			return true;
		}
	}
	return false;
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

static void ClearPlans()
{
	_plan.pieces.clear();
	_plan.start = INVALID_TILE;
	_road_plan.tiles.clear();
	_road_plan.start = INVALID_TILE;
	_rect_plan.valid = false;
}

static void CommitRailPlan()
{
	if (_plan.start == INVALID_TILE) return;
	if (_drag_remove) {
		Command<CMD_REMOVE_RAILROAD_TRACK>::Post(_plan.end, _plan.start, _plan.track);
	} else {
		Command<CMD_BUILD_RAILROAD_TRACK>::Post(_plan.end, _plan.start, PickRailType(), _plan.track, true, false);
	}
	ClearPlans();
}

static void UpdateRoadPlan(double wx, double wy)
{
	_road_plan.tiles.clear();
	_road_plan.start = INVALID_TILE;

	int atx = Clamp<int>((int)std::floor(_drag_ax), 0, Map::SizeX() - 2);
	int aty = Clamp<int>((int)std::floor(_drag_ay), 0, Map::SizeY() - 2);
	double dx = wx - _drag_ax;
	double dy = wy - _drag_ay;

	int steps;
	if (std::abs(dx) >= std::abs(dy)) {
		_road_plan.axis = AXIS_X;
		steps = Clamp((int)std::lround(dx), -127, 127);
	} else {
		_road_plan.axis = AXIS_Y;
		steps = Clamp((int)std::lround(dy), -127, 127);
	}
	int dir = steps >= 0 ? 1 : -1;
	for (int i = 0; ; i += dir) {
		int tx = _road_plan.axis == AXIS_X ? atx + i : atx;
		int ty = _road_plan.axis == AXIS_Y ? aty + i : aty;
		if (tx < 0 || ty < 0 || tx > (int)Map::SizeX() - 2 || ty > (int)Map::SizeY() - 2) break;
		_road_plan.tiles.push_back(TileXY(tx, ty));
		if (i == steps) break;
	}
	if (_road_plan.tiles.empty()) return;

	_road_plan.start = _road_plan.tiles.front();
	_road_plan.end = _road_plan.tiles.back();
}

static void DrawRoadPlan(int ppt)
{
	uint32_t c = _drag_remove ? COL_BP_RM : COL_BP;
	int w = std::max(2, ppt / 3);
	for (TileIndex tile : _road_plan.tiles) {
		int tx = TileX(tile);
		int ty = TileY(tile);
		DrawAxisBand(_road_plan.axis, PxX(tx), PxY(ty), PxX(tx + 1) - 1, PxY(ty + 1) - 1, w, c);
	}
}

static RoadType PickRoadType()
{
	const Company *c = Company::GetIfValid(_local_company);
	if (c != nullptr) {
		for (RoadType rt = ROADTYPE_BEGIN; rt != ROADTYPE_END; rt++) {
			if (GetRoadTramType(rt) == RTT_ROAD && c->avail_roadtypes.Test(rt)) return rt;
		}
	}
	return ROADTYPE_ROAD;
}

/* Rectangle drag; the far corner truncates at the size limit so the
 * anchor corner always stays inside the allowed area. */
static void UpdateRectPlan(double wx, double wy, int limit)
{
	int ax = Clamp<int>((int)std::floor(_drag_ax), 1, Map::SizeX() - 2);
	int ay = Clamp<int>((int)std::floor(_drag_ay), 1, Map::SizeY() - 2);
	int bx = Clamp<int>((int)std::floor(wx), 1, Map::SizeX() - 2);
	int by = Clamp<int>((int)std::floor(wy), 1, Map::SizeY() - 2);

	if (bx >= ax) {
		_rect_plan.x0 = ax;
		_rect_plan.x1 = std::min(bx, ax + limit - 1);
	} else {
		_rect_plan.x0 = std::max(bx, ax - limit + 1);
		_rect_plan.x1 = ax;
	}
	if (by >= ay) {
		_rect_plan.y0 = ay;
		_rect_plan.y1 = std::min(by, ay + limit - 1);
	} else {
		_rect_plan.y0 = std::max(by, ay - limit + 1);
		_rect_plan.y1 = ay;
	}
	_rect_plan.valid = true;
}

static int RectPlanLimit()
{
	if (_tool == MiniTool::Station) return _settings_game.station.station_spread;
	return std::max<int>(Map::SizeX(), Map::SizeY());
}

static void DrawRectPlan(int ppt)
{
	if (!_rect_plan.valid) return;
	uint32_t c = (_drag_remove || _tool == MiniTool::Demolish) ? COL_BP_RM : COL_BP;
	int px0 = PxX(_rect_plan.x0);
	int py0 = PxY(_rect_plan.y0);
	int px1 = PxX(_rect_plan.x1 + 1) - 1;
	int py1 = PxY(_rect_plan.y1 + 1) - 1;
	BlendRect(px0, py0, px1, py1, c, 90);
	int b = std::max(1, ppt / 8);
	FillRect(px0, py0, px1, py0 + b - 1, c);
	FillRect(px0, py1 - b + 1, px1, py1, c);
	FillRect(px0, py0, px0 + b - 1, py1, c);
	FillRect(px1 - b + 1, py0, px1, py1, c);
}

static void CommitStationPlan()
{
	if (!_rect_plan.valid) return;
	int w = _rect_plan.x1 - _rect_plan.x0 + 1;
	int h = _rect_plan.y1 - _rect_plan.y0 + 1;
	TileIndex org = TileXY(_rect_plan.x0, _rect_plan.y0);
	if (_drag_remove) {
		Command<CMD_REMOVE_FROM_RAIL_STATION>::Post(org, TileXY(_rect_plan.x1, _rect_plan.y1), true);
	} else {
		Axis axis = w >= h ? AXIS_X : AXIS_Y;
		uint8_t plat_len = (uint8_t)(axis == AXIS_X ? w : h);
		uint8_t numtracks = (uint8_t)(axis == AXIS_X ? h : w);
		Command<CMD_BUILD_RAIL_STATION>::Post(org, PickRailType(), axis, numtracks, plat_len, STAT_CLASS_DFLT, 0, StationID::Invalid(), false);
	}
	ClearPlans();
}

static void CommitDemolishPlan()
{
	if (!_rect_plan.valid) return;
	Command<CMD_CLEAR_AREA>::Post(TileXY(_rect_plan.x1, _rect_plan.y1), TileXY(_rect_plan.x0, _rect_plan.y0), false);
	ClearPlans();
}

/* Levelling copies the anchor tile's height, so the anchor corner is passed
 * as the reference tile rather than the normalised rectangle origin. */
static void CommitTerraformPlan()
{
	if (!_rect_plan.valid) return;
	int ax = Clamp<int>((int)std::floor(_drag_ax), 1, Map::SizeX() - 2);
	int ay = Clamp<int>((int)std::floor(_drag_ay), 1, Map::SizeY() - 2);
	int ex = _rect_plan.x0 == ax ? _rect_plan.x1 : _rect_plan.x0;
	int ey = _rect_plan.y0 == ay ? _rect_plan.y1 : _rect_plan.y0;
	TileIndex anchor = TileXY(ax, ay);
	TileIndex end = TileXY(ex, ey);
	LevelMode lm = _drag_remove ? LM_LOWER : (anchor == end ? LM_RAISE : LM_LEVEL);
	Command<CMD_LEVEL_LAND>::Post(end, anchor, false, lm);
	ClearPlans();
}

static Axis DragAxis(double wx, double wy, TileIndex tile)
{
	double dx = wx - _drag_ax;
	double dy = wy - _drag_ay;
	if (std::abs(dx) < 0.25 && std::abs(dy) < 0.25) {
		RoadBits rb = IsTileType(tile, MP_ROAD) && IsNormalRoad(tile) ? GetRoadBits(tile, RTT_ROAD) : ROAD_NONE;
		if ((rb & ROAD_Y) != ROAD_NONE && (rb & ROAD_X) == ROAD_NONE) return AXIS_Y;
		return AXIS_X;
	}
	return std::abs(dx) >= std::abs(dy) ? AXIS_X : AXIS_Y;
}

static DiagDirection DragDir(double wx, double wy)
{
	double dx = wx - _drag_ax;
	double dy = wy - _drag_ay;
	if (std::abs(dx) >= std::abs(dy)) return dx >= 0 ? DIAGDIR_SW : DIAGDIR_NE;
	return dy >= 0 ? DIAGDIR_SE : DIAGDIR_NW;
}

/* Same sub-track pick as GenericPlaceSignals: on paired straight pieces the
 * fractional click position decides which half gets the signal. */
static Track PickSignalTrack(TileIndex tile)
{
	if (!IsPlainRailTile(tile)) return INVALID_TRACK;
	TrackBits trackbits = GetTrackBits(tile);
	double fx = _drag_ax - std::floor(_drag_ax);
	double fy = _drag_ay - std::floor(_drag_ay);
	if (trackbits & TRACK_BIT_VERT) trackbits = (fx <= fy) ? TRACK_BIT_RIGHT : TRACK_BIT_LEFT;
	if (trackbits & TRACK_BIT_HORZ) trackbits = (fx + fy <= 1.0) ? TRACK_BIT_UPPER : TRACK_BIT_LOWER;
	return FindFirstTrack(trackbits);
}

static void CommitPointTool(double wx, double wy)
{
	int tx = Clamp<int>((int)std::floor(_drag_ax), 1, Map::SizeX() - 2);
	int ty = Clamp<int>((int)std::floor(_drag_ay), 1, Map::SizeY() - 2);
	TileIndex tile = TileXY(tx, ty);

	switch (_tool) {
		case MiniTool::BusStop:
		case MiniTool::TruckStop: {
			RoadStopType st = _tool == MiniTool::BusStop ? RoadStopType::Bus : RoadStopType::Truck;
			if (_drag_remove) {
				Command<CMD_REMOVE_ROAD_STOP>::Post(tile, 1, 1, st, false);
			} else {
				DiagDirection ddir = AxisToDiagDir(DragAxis(wx, wy, tile));
				Command<CMD_BUILD_ROAD_STOP>::Post(tile, 1, 1, st, true, ddir, PickRoadType(), ROADSTOP_CLASS_DFLT, 0, StationID::Invalid(), false);
			}
			break;
		}

		case MiniTool::TrainDepot:
			if (_drag_remove) {
				Command<CMD_LANDSCAPE_CLEAR>::Post(tile);
			} else {
				Command<CMD_BUILD_TRAIN_DEPOT>::Post(tile, PickRailType(), DragDir(wx, wy));
			}
			break;

		case MiniTool::RoadDepot:
			if (_drag_remove) {
				Command<CMD_LANDSCAPE_CLEAR>::Post(tile);
			} else {
				Command<CMD_BUILD_ROAD_DEPOT>::Post(tile, PickRoadType(), DragDir(wx, wy));
			}
			break;

		case MiniTool::Signal: {
			Track track = PickSignalTrack(tile);
			if (track == INVALID_TRACK) break;
			if (_drag_remove) {
				Command<CMD_REMOVE_SINGLE_SIGNAL>::Post(tile, track);
			} else {
				SignalVariant sigvar = TimerGameCalendar::year < _settings_client.gui.semaphore_build_before ? SIG_SEMAPHORE : SIG_ELECTRIC;
				Command<CMD_BUILD_SINGLE_SIGNAL>::Post(tile, track, _settings_client.gui.default_signal_type, sigvar, false, false, false, SIGTYPE_PBS, SIGTYPE_LAST, 0, 0);
			}
			break;
		}

		default:
			break;
	}
}

static void DrawPointToolPlan(int ppt)
{
	uint32_t c = _drag_remove ? COL_BP_RM : COL_BP;
	int tx = Clamp<int>((int)std::floor(_drag_ax), 1, Map::SizeX() - 2);
	int ty = Clamp<int>((int)std::floor(_drag_ay), 1, Map::SizeY() - 2);
	int x0 = PxX(tx);
	int y0 = PxY(ty);
	int x1 = PxX(tx + 1) - 1;
	int y1 = PxY(ty + 1) - 1;
	BlendRect(x0, y0, x1, y1, c, 90);
	if (_drag_remove) return;

	double wx = WorldX(_cursor.pos.x);
	double wy = WorldY(_cursor.pos.y);
	if (_tool == MiniTool::Signal) {
		Track track = PickSignalTrack(TileXY(tx, ty));
		if (track != INVALID_TRACK) DrawTrackPiece(track, x0, y0, x1, y1, std::max(2, ppt / 5), c);
	} else if (_tool == MiniTool::BusStop || _tool == MiniTool::TruckStop) {
		DrawAxisBand(DragAxis(wx, wy, TileXY(tx, ty)), x0, y0, x1, y1, std::max(2, ppt / 3), c);
	} else {
		DiagDirection d = DragDir(wx, wy);
		int cx = (x0 + x1) / 2;
		int cy = (y0 + y1) / 2;
		ThickLine(cx, cy, cx + _diag_dx[d] * (ppt / 2), cy + _diag_dy[d] * (ppt / 2), std::max(2, ppt / 5), c);
	}
}

static void CommitRoadPlan()
{
	if (_road_plan.start == INVALID_TILE) return;
	if (_drag_remove) {
		Command<CMD_REMOVE_LONG_ROAD>::Post(_road_plan.end, _road_plan.start, PickRoadType(), _road_plan.axis, false, false);
	} else {
		Command<CMD_BUILD_LONG_ROAD>::Post(_road_plan.end, _road_plan.start, PickRoadType(), _road_plan.axis, DRD_NONE, false, false, false);
	}
	ClearPlans();
}

static BridgeType PickBridgeType(uint len)
{
	BridgeType best = 0;
	uint best_speed = 0;
	for (BridgeType bt = 0; bt < MAX_BRIDGES; bt++) {
		if (!CheckBridgeAvailability(bt, len).Succeeded()) continue;
		if (_bridge[bt].speed > best_speed) {
			best = bt;
			best_speed = _bridge[bt].speed;
		}
	}
	return best;
}

/* Drag spans a bridge; a plain click on a sloped tile digs a tunnel instead.
 * Removal clears the head tile, which takes the whole crossing with it. */
static void CommitBridgePlan()
{
	if (_road_plan.start == INVALID_TILE) return;
	bool rail = _tool == MiniTool::RailBridge;
	TransportType tt = rail ? TRANSPORT_RAIL : TRANSPORT_ROAD;
	uint8_t rrt = rail ? (uint8_t)PickRailType() : (uint8_t)PickRoadType();
	if (_drag_remove) {
		Command<CMD_LANDSCAPE_CLEAR>::Post(_road_plan.start);
	} else if (_road_plan.start == _road_plan.end) {
		Command<CMD_BUILD_TUNNEL>::Post(_road_plan.start, tt, rrt);
	} else {
		uint len = std::max(Delta(TileX(_road_plan.start), TileX(_road_plan.end)), Delta(TileY(_road_plan.start), TileY(_road_plan.end))) - 1;
		Command<CMD_BUILD_BRIDGE>::Post(_road_plan.end, _road_plan.start, tt, PickBridgeType(len), rrt);
	}
	ClearPlans();
}

static void DrawCursor()
{
	int x = _cursor.pos.x;
	int y = _cursor.pos.y;
	FillRect(x - 9, y - 1, x + 9, y + 1, COL_INK);
	FillRect(x - 1, y - 9, x + 1, y + 9, COL_INK);
	FillRect(x - 8, y, x + 8, y, COL_PAPER);
	FillRect(x, y - 8, x, y + 8, COL_PAPER);
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
	int lh = GetCharacterHeight(FS_NORMAL);
	DrawScreenText(6 * s, 6 * s, line);

	if (_pause_mode.Any()) DrawScreenTextCentred(_fbw / 2, 6 * s, "PAUSED");

	if (const Vehicle *v = Vehicle::GetIfValid(_sel_vehicle); v != nullptr) {
		static const std::string_view kinds[4] = {"TRAIN", "ROAD", "SHIP", "PLANE"};
		std::string info = fmt::format("{} {}  SPD {}", kinds[v->type], v->unitnumber, v->GetDisplaySpeed());
		if (v->vehstatus.Test(VehState::Stopped)) info += "  STOPPED";
		uint cap = 0, stored = 0;
		CargoType ct = INVALID_CARGO;
		for (const Vehicle *u = v; u != nullptr; u = u->Next()) {
			if (u->cargo_cap == 0) continue;
			cap += u->cargo_cap;
			stored += u->cargo.StoredCount();
			if (!IsValidCargoType(ct)) ct = u->cargo_type;
		}
		if (cap > 0 && IsValidCargoType(ct)) {
			uint32_t l = CargoSpec::Get(ct)->label.base();
			char lab[4] = {(char)(l >> 24), (char)(l >> 16), (char)(l >> 8), (char)l};
			info += fmt::format("  {} {}/{}", std::string_view(lab, 4), stored, cap);
		}
		info += fmt::format("  ORDERS {}  PROFIT {}", v->GetNumOrders(), FormatMoney(v->GetDisplayProfitThisYear()));
		DrawScreenText(6 * s, 6 * s + lh + 2, info);
	}

	std::string_view hint;
	std::string_view hint2;
	switch (_tool) {
		case MiniTool::Rail: hint = "RAIL: DRAG BUILD / CTRL DRAG REMOVE / RMB CANCEL"; break;
		case MiniTool::Road: hint = "ROAD: DRAG BUILD / CTRL DRAG REMOVE / RMB CANCEL"; break;
		case MiniTool::Station: hint = "STATION: DRAG AREA / CTRL DRAG REMOVE / RMB CANCEL"; break;
		case MiniTool::BusStop: hint = "BUS STOP: DRAG SETS AXIS / CTRL CLICK REMOVE / RMB CANCEL"; break;
		case MiniTool::TruckStop: hint = "TRUCK STOP: DRAG SETS AXIS / CTRL CLICK REMOVE / RMB CANCEL"; break;
		case MiniTool::TrainDepot: hint = "TRAIN DEPOT: DRAG SETS EXIT / CTRL CLICK REMOVE / RMB CANCEL"; break;
		case MiniTool::RoadDepot: hint = "ROAD DEPOT: DRAG SETS EXIT / CTRL CLICK REMOVE / RMB CANCEL"; break;
		case MiniTool::Demolish: hint = "CLEAR: DRAG AREA / RMB CANCEL"; break;
		case MiniTool::Signal: hint = "SIGNAL: CLICK BUILD OR CYCLE / CTRL CLICK REMOVE / RMB CANCEL"; break;
		case MiniTool::RailBridge: hint = "RAIL BRIDGE: DRAG SPAN / CLICK SLOPE TUNNEL / CTRL CLICK REMOVE / RMB CANCEL"; break;
		case MiniTool::RoadBridge: hint = "ROAD BRIDGE: DRAG SPAN / CLICK SLOPE TUNNEL / CTRL CLICK REMOVE / RMB CANCEL"; break;
		case MiniTool::Terraform: hint = "TERRAIN: DRAG LEVEL / CLICK RAISE / CTRL LOWER / RMB CANCEL"; break;
		default:
			if (Vehicle::GetIfValid(_sel_vehicle) != nullptr) {
				hint = _follow
					? "FOLLOWING  CLICK STATION ORDER  O DROP ORDER  P START STOP  H UNFOLLOW  ESC DESELECT"
					: "CLICK STATION ORDER / CTRL FULL LOAD  O DROP ORDER  P START STOP  H FOLLOW  ESC DESELECT";
			} else {
				hint = "Z TERRAIN  X CLEAR  N BUY AT DEPOT / CTRL WAGON OR FREIGHT  SPACE PAUSE  F9 EXIT";
				hint2 = "R RAIL  E ROAD  T STATION  B BUS  G TRUCK  F/V DEPOT  L SIGNAL  U/I BRIDGE";
			}
			break;
	}
	DrawScreenText(6 * s, _fbh - lh - 6 * s, hint);
	if (!hint2.empty()) DrawScreenText(6 * s, _fbh - 2 * lh - 6 * s - 2, hint2);
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
	DrawLabels();
	DrawHud();
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
	if (_follow) return;
	_glide = false;
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
	_glide = false;
	_sel_vehicle = VehicleID::Invalid();
	_follow = false;
	ClearPlans();
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

bool MiniUiHidesWindow(WindowClass wc)
{
	return wc == WC_MAIN_WINDOW || wc == WC_MAIN_TOOLBAR || wc == WC_STATUS_BAR;
}

bool MiniUiHidesMouseCursor()
{
	if (!_mini_active) return false;
	Window *w = FindWindowFromPt(_cursor.pos.x, _cursor.pos.y);
	return w == nullptr || MiniUiHidesWindow(w->window_class);
}

void MiniUiScrollTo(int x, int y)
{
	if (!_mini_active) return;
	_follow = false;
	_zoom_anchored = false;
	_glide = true;
	_glide_x = x / (double)TILE_SIZE;
	_glide_y = y / (double)TILE_SIZE;
	_dest_ppt = _ms.jump_ppt;
}

bool MiniUiHandleMouseEvents(bool native_capture)
{
	if (!_mini_active) return false;

	if (!_dragging && !_middle_button_down) {
		if (native_capture) return false;
		Window *w = FindWindowFromPt(_cursor.pos.x, _cursor.pos.y);
		if (w != nullptr && !MiniUiHidesWindow(w->window_class)) return false;
	}

	if (_middle_button_down && (_cursor.delta.x != 0 || _cursor.delta.y != 0)) {
		_zoom_anchored = false;
		_follow = false;
		_glide = false;
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
		if (_tool == MiniTool::None) {
			if (!TryAppendOrder(_cursor.pos.x, _cursor.pos.y) && !HandleLabelClick(_cursor.pos.x, _cursor.pos.y)) {
				SelectVehicleAt(_cursor.pos.x, _cursor.pos.y);
			}
		} else {
			_dragging = true;
			_drag_remove = _ctrl_pressed;
			_drag_ax = WorldX(_cursor.pos.x);
			_drag_ay = WorldY(_cursor.pos.y);
			if (_tool == MiniTool::Rail) UpdateRailPlan(_drag_ax, _drag_ay);
			if (_tool == MiniTool::Road || IsBridgeTool(_tool)) UpdateRoadPlan(_drag_ax, _drag_ay);
			if (IsRectTool(_tool)) UpdateRectPlan(_drag_ax, _drag_ay, RectPlanLimit());
		}
	}

	if (!_left_button_down && _prev_left && _dragging) {
		_dragging = false;
		if (_tool == MiniTool::Rail) CommitRailPlan();
		if (_tool == MiniTool::Road) CommitRoadPlan();
		if (IsBridgeTool(_tool)) CommitBridgePlan();
		if (_tool == MiniTool::Station) CommitStationPlan();
		if (_tool == MiniTool::Demolish) CommitDemolishPlan();
		if (_tool == MiniTool::Terraform) CommitTerraformPlan();
		if (IsPointTool(_tool)) CommitPointTool(WorldX(_cursor.pos.x), WorldY(_cursor.pos.y));
	}
	_prev_left = _left_button_down;

	if (_right_button_clicked) {
		_right_button_clicked = false;
		if (_dragging) {
			_dragging = false;
			ClearPlans();
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
				ClearPlans();
			} else if (_tool != MiniTool::None) {
				_tool = MiniTool::None;
			} else if (_sel_vehicle != VehicleID::Invalid()) {
				_sel_vehicle = VehicleID::Invalid();
				_follow = false;
			} else {
				Deactivate();
			}
			break;

		case 'R':
			_tool = _tool == MiniTool::Rail ? MiniTool::None : MiniTool::Rail;
			break;

		case 'E':
			_tool = _tool == MiniTool::Road ? MiniTool::None : MiniTool::Road;
			break;

		case 'T':
			_tool = _tool == MiniTool::Station ? MiniTool::None : MiniTool::Station;
			break;

		case 'B':
			_tool = _tool == MiniTool::BusStop ? MiniTool::None : MiniTool::BusStop;
			break;

		case 'G':
			_tool = _tool == MiniTool::TruckStop ? MiniTool::None : MiniTool::TruckStop;
			break;

		case 'F':
			_tool = _tool == MiniTool::TrainDepot ? MiniTool::None : MiniTool::TrainDepot;
			break;

		case 'V':
			_tool = _tool == MiniTool::RoadDepot ? MiniTool::None : MiniTool::RoadDepot;
			break;

		case 'X':
			_tool = _tool == MiniTool::Demolish ? MiniTool::None : MiniTool::Demolish;
			break;

		case 'L':
			_tool = _tool == MiniTool::Signal ? MiniTool::None : MiniTool::Signal;
			break;

		case 'U':
			_tool = _tool == MiniTool::RailBridge ? MiniTool::None : MiniTool::RailBridge;
			break;

		case 'I':
			_tool = _tool == MiniTool::RoadBridge ? MiniTool::None : MiniTool::RoadBridge;
			break;

		case 'Z':
			_tool = _tool == MiniTool::Terraform ? MiniTool::None : MiniTool::Terraform;
			break;

		case 'P':
			if (const Vehicle *v = Vehicle::GetIfValid(_sel_vehicle); v != nullptr) {
				Command<CMD_START_STOP_VEHICLE>::Post(v->tile, _sel_vehicle, false);
			}
			break;

		case 'H':
			_follow = !_follow && Vehicle::GetIfValid(_sel_vehicle) != nullptr;
			if (_follow) _glide = false;
			break;

		case 'O':
			if (const Vehicle *v = Vehicle::GetIfValid(_sel_vehicle); v != nullptr && v->GetNumOrders() > 0) {
				Command<CMD_DELETE_ORDER>::Post(v->tile, v->index, (VehicleOrderID)(v->GetNumOrders() - 1));
			}
			break;

		case 'N':
			BuyAtDepot(_ctrl_pressed);
			break;

		case WKC_SPACE:
			Command<CMD_PAUSE>::Post(PauseMode::Normal, !_pause_mode.Test(PauseMode::Normal));
			break;

		default:
			break;
	}
	return true;
}

void MiniUiFrame(uint delta_ms)
{
	if (!_mini_active) return;
	if (_game_mode != GM_NORMAL && _game_mode != GM_EDITOR) {
		Deactivate();
		return;
	}

	/* Native windows request vehicle following on the main viewport. */
	static VehicleID last_native_follow = VehicleID::Invalid();
	VehicleID native_follow = GetMainWindow()->viewport->follow_vehicle;
	if (native_follow != last_native_follow) {
		last_native_follow = native_follow;
		if (native_follow != VehicleID::Invalid()) {
			_sel_vehicle = native_follow;
			_follow = true;
			_glide = false;
		}
	}

	if (_fbw != _screen.width || _fbh != _screen.height) {
		_fbw = _screen.width;
		_fbh = _screen.height;
		_fb.assign((size_t)_fbw * _fbh, COL_VOID);
	}
	if (_fbw <= 0 || _fbh <= 0) return;

	/* WASD and arrows arrive via _dirkeys; pan speed is constant in screen space. */
	if (_dirkeys != 0) {
		_zoom_anchored = false;
		_follow = false;
		_glide = false;
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
			_follow = false;
			_glide = false;
			_cam_x += ex;
			_cam_y += ey;
			ClampCamera();
		}
	}

	if (_follow) {
		const Vehicle *fv = Vehicle::GetIfValid(_sel_vehicle);
		if (fv == nullptr) {
			_follow = false;
		} else {
			_cam_x = fv->x_pos / (double)TILE_SIZE;
			_cam_y = fv->y_pos / (double)TILE_SIZE;
			ClampCamera();
		}
	}

	if (_glide) {
		double f = 1.0 - std::exp(delta_ms / -_ms.glide_ms);
		_cam_x += (_glide_x - _cam_x) * f;
		_cam_y += (_glide_y - _cam_y) * f;
		ClampCamera();
		if (std::abs(_glide_x - _cam_x) * _cam_ppt < 0.5 && std::abs(_glide_y - _cam_y) * _cam_ppt < 0.5) {
			_cam_x = _glide_x;
			_cam_y = _glide_y;
			ClampCamera();
			_glide = false;
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
	ComputeZoomDetail(ppt);

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
		if (_tool == MiniTool::Rail) {
			UpdateRailPlan(WorldX(_cursor.pos.x), WorldY(_cursor.pos.y));
			DrawRailPlan(ppt);
		} else if (_tool == MiniTool::Road || IsBridgeTool(_tool)) {
			UpdateRoadPlan(WorldX(_cursor.pos.x), WorldY(_cursor.pos.y));
			DrawRoadPlan(ppt);
		} else if (IsRectTool(_tool)) {
			UpdateRectPlan(WorldX(_cursor.pos.x), WorldY(_cursor.pos.y), RectPlanLimit());
			DrawRectPlan(ppt);
		} else if (IsPointTool(_tool)) {
			DrawPointToolPlan(ppt);
		}
	} else if (_tool != MiniTool::None) {
		int htx = (int)std::floor(WorldX(_cursor.pos.x));
		int hty = (int)std::floor(WorldY(_cursor.pos.y));
		if (htx >= 0 && hty >= 0 && htx < (int)Map::SizeX() && hty < (int)Map::SizeY()) {
			uint32_t c = _tool == MiniTool::Demolish ? COL_BP_RM : COL_BP;
			BlendRect(PxX(htx), PxY(hty), PxX(htx + 1) - 1, PxY(hty + 1) - 1, c, 70);
		}
	}

	DrawOrderRoute();
	DrawVehicles(ppt);
	DrawSelectionRing(ppt);
	DrawCursor();
	Present();
	MarkWholeScreenDirty();
}
