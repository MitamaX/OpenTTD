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
#include "company_gui.h"
#include "elrail_func.h"
#include "engine_base.h"
#include "core/backup_type.hpp"
#include "core/math_func.hpp"
#include "core/utf8.hpp"
#include <unordered_map>
#include "fileio_func.h"
#include "gfx_func.h"
#include "graph_gui.h"
#include "ground_vehicle.hpp"
#include "gui.h"
#include "industry.h"
#include "ini_type.h"
#include "landscape.h"
#include "landscape_cmd.h"
#include "league_gui.h"
#include "mini_atlas.h"
#include "misc_cmd.h"
#include "network/network.h"
#include "network/network_type.h"
#include "newgrf_roadstop.h"
#include "newgrf_station.h"
#include "news_gui.h"
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
#include "string_func.h"
#include "strings_func.h"
#include "station_map.h"
#include "terraform_cmd.h"
#include "town.h"
#include "tile_map.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_tick.h"
#include "tunnelbridge_cmd.h"
#include "tunnelbridge_map.h"
#include "vehicle_base.h"
#include "vehicle_cmd.h"
#include "vehicle_func.h"
#include "vehicle_gui.h"
#include "video/video_driver.hpp"
#include "water_map.h"
#include "window_func.h"
#include "window_gui.h"

#include "table/strings.h"

#include "safeguards.h"

static bool _mini_active = false;

/* Mini UI frame size in pixels; drawing goes through the raylib command
 * buffer, so this only mirrors the screen dimensions. */
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
	RailTunnel,
	RoadTunnel,
	Terraform,
};

static bool IsRectTool(MiniTool t)
{
	return t == MiniTool::Station || t == MiniTool::Demolish || t == MiniTool::Terraform;
}

static bool IsPointTool(MiniTool t)
{
	return t == MiniTool::BusStop || t == MiniTool::TruckStop || t == MiniTool::TrainDepot || t == MiniTool::RoadDepot || t == MiniTool::Signal || t == MiniTool::RailTunnel || t == MiniTool::RoadTunnel;
}

static bool IsDirPointTool(MiniTool t)
{
	return t == MiniTool::BusStop || t == MiniTool::TruckStop || t == MiniTool::TrainDepot || t == MiniTool::RoadDepot;
}

enum class MiniLayer : uint8_t {
	None,
	Rail,
	Road,
};

static MiniLayer ToolLayer(MiniTool t)
{
	switch (t) {
		case MiniTool::Rail:
		case MiniTool::Station:
		case MiniTool::TrainDepot:
		case MiniTool::Signal:
		case MiniTool::RailTunnel:
			return MiniLayer::Rail;
		case MiniTool::Road:
		case MiniTool::BusStop:
		case MiniTool::TruckStop:
		case MiniTool::RoadDepot:
		case MiniTool::RoadTunnel:
			return MiniLayer::Road;
		default:
			return MiniLayer::None;
	}
}

static MiniTool _tool = MiniTool::None;

/* ONI-style overlay: an explicit toggle that swaps the info layer without
 * leaving the screen. Picking a build tool auto-engages its layer; that
 * auto choice reverts when the tool is dropped, a manual toggle sticks. */
static MiniLayer _overlay = MiniLayer::None;
static bool _overlay_auto = false;
static MiniLayer _last_tool_layer = MiniLayer::None;
static MiniLayer _filter_layer = MiniLayer::None;
static DiagDirection _point_dir = DIAGDIR_SE;
static bool _dragging = false;
static bool _drag_remove = false;
static double _drag_ax, _drag_ay;

static VehicleID _sel_vehicle = VehicleID::Invalid();
static bool _follow = false;

static bool _prev_left = false;

struct MiniRailPlan {
	std::vector<TileIndex> path;
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
	int start_active = 1;
	double pan_speed = 1600.0;
	double pan_speed_fast = 4000.0;
	double pan_smooth_ms = 60.0;
	double zoom_step = 1.25;
	double zoom_smooth_ms = 80.0;
	int hud_scale = 2;
	int menu_panel_rows = 4;
	int grid_alpha = 32;
	int contour_alpha = 120;
	int relief_strength = 22;
	int filter_alpha = 150;
	int edge_scroll = 0;
	int edge_margin = 24;
	double edge_scroll_speed = 1600.0;
	double drag_pan_multiplier = 2.0;
	double jump_ppt = 32.0;
	double glide_ms = 250.0;
};

static MiniSettings _ms;

static bool _zoom_anchored = false;
static int _zoom_sx, _zoom_sy;
static double _zoom_wx, _zoom_wy;

static bool _glide = false;
static double _glide_x, _glide_y;

static double _pan_vx, _pan_vy;

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

	ReadIniNumber(group, "start_active", _ms.start_active);
	ReadIniNumber(group, "pan_speed", _ms.pan_speed);
	ReadIniNumber(group, "pan_speed_fast", _ms.pan_speed_fast);
	ReadIniNumber(group, "pan_smooth_ms", _ms.pan_smooth_ms);
	ReadIniNumber(group, "zoom_step", _ms.zoom_step);
	ReadIniNumber(group, "zoom_smooth_ms", _ms.zoom_smooth_ms);
	ReadIniNumber(group, "hud_scale", _ms.hud_scale);
	ReadIniNumber(group, "menu_panel_rows", _ms.menu_panel_rows);
	ReadIniNumber(group, "grid_alpha", _ms.grid_alpha);
	ReadIniNumber(group, "contour_alpha", _ms.contour_alpha);
	ReadIniNumber(group, "relief_strength", _ms.relief_strength);
	ReadIniNumber(group, "filter_alpha", _ms.filter_alpha);
	ReadIniNumber(group, "edge_scroll", _ms.edge_scroll);
	ReadIniNumber(group, "edge_margin", _ms.edge_margin);
	ReadIniNumber(group, "edge_scroll_speed", _ms.edge_scroll_speed);
	ReadIniNumber(group, "drag_pan_multiplier", _ms.drag_pan_multiplier);
	ReadIniNumber(group, "jump_ppt", _ms.jump_ppt);
	ReadIniNumber(group, "glide_ms", _ms.glide_ms);

	_ms.pan_speed = Clamp(_ms.pan_speed, 100.0, 10000.0);
	_ms.pan_speed_fast = Clamp(_ms.pan_speed_fast, 100.0, 20000.0);
	_ms.pan_smooth_ms = Clamp(_ms.pan_smooth_ms, 1.0, 500.0);
	_ms.zoom_step = Clamp(_ms.zoom_step, 1.05, 2.0);
	_ms.zoom_smooth_ms = Clamp(_ms.zoom_smooth_ms, 1.0, 500.0);
	_ms.hud_scale = Clamp(_ms.hud_scale, 1, 4);
	_ms.menu_panel_rows = Clamp(_ms.menu_panel_rows, 1, 4);
	_ms.grid_alpha = Clamp(_ms.grid_alpha, 0, 255);
	_ms.contour_alpha = Clamp(_ms.contour_alpha, 0, 255);
	_ms.relief_strength = Clamp(_ms.relief_strength, 0, 60);
	_ms.filter_alpha = Clamp(_ms.filter_alpha, 0, 230);
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

/* Screen chrome follows the reference HUD: warm dark panels with a thin
 * darker edge, teal active state, cyan accent, warm off-white text. */
static const uint32_t COL_CH_PANEL = 0xFF262624U;
static const uint32_t COL_CH_EDGE = 0xFF191916U;
static const uint32_t COL_CH_TILE = 0xFF2E2E2BU;
static const uint32_t COL_CH_ACTIVE = 0xFF3D5A5CU;
static const uint32_t COL_CH_TEXT = 0xFFC5C0B2U;
static const uint32_t COL_CH_DIM = 0xFF6E6A5EU;
static const uint32_t COL_CH_ACCENT = 0xFF8FE0E8U;

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

/* Transposed projection: map X runs down the screen and map Y runs right,
 * matching the native isometric orientation's handedness. */
static double ScrBaseX() { return _fbw * 0.5 - _cam_y * _cam_ppt; }
static double ScrBaseY() { return _fbh * 0.5 - _cam_x * _cam_ppt; }

static int ScrX(double ty) { return (int)std::lround(ty * _cam_ppt + ScrBaseX()); }
static int ScrY(double tx) { return (int)std::lround(tx * _cam_ppt + ScrBaseY()); }

static double MapXAt(int sy) { return (sy - ScrBaseY()) / _cam_ppt; }
static double MapYAt(int sx) { return (sx - ScrBaseX()) / _cam_ppt; }

/* Overlay mode draws the base map as darkened greyscale: luminance is kept
 * so terrain still reads, while repainted layer content gets full colour.
 * filter_alpha sets how far the background sinks. */
static bool _grey_map = false;

static uint32_t GreyMap(uint32_t c)
{
	uint lum = (77 * ((c >> 16) & 0xFFU) + 151 * ((c >> 8) & 0xFFU) + 28 * (c & 0xFFU)) >> 8;
	uint g = 12 + lum * (255 - (uint)_ms.filter_alpha) / 255;
	return (c & 0xFF000000U) | (g << 16) | (g << 8) | g;
}

static uint32_t MapCol(uint32_t c)
{
	return _grey_map ? GreyMap(c) : c;
}

static void FillRect(int x0, int y0, int x1, int y1, uint32_t c)
{
	if (x1 < x0 || y1 < y0) return;
	RlwCmdRect(x0, y0, x1, y1, MapCol(c));
}

static void BlendRect(int x0, int y0, int x1, int y1, uint32_t c, uint alpha)
{
	if (x1 < x0 || y1 < y0) return;
	RlwCmdRect(x0, y0, x1, y1, (MapCol(c) & 0x00FFFFFFU) | ((uint32_t)Clamp<uint>(alpha, 0, 255) << 24));
}

static void ThickLine(int x0, int y0, int x1, int y1, int width, uint32_t c)
{
	RlwCmdLine(x0, y0, x1, y1, std::max(width, 1), MapCol(c));
}

/* Shape fills prefer an atlas quad so the silhouettes are already on the
 * sprite pipeline; sizes too small to sample cleanly and frames without an
 * atlas fall back to the geometric primitives. */
static void FillCircle(int cx, int cy, int r, uint32_t c)
{
	r = std::max(r, 1);
	uint32_t col = MapCol(c);
	if (r >= 2 && MiniAtlasQuad(MiniSprite::Disc, cx - r, cy - r, cx + r, cy + r, col)) return;
	RlwCmdCircle(cx, cy, r, col);
}

static void FillDiamond(int cx, int cy, int r, uint32_t c)
{
	r = std::max(r, 1);
	uint32_t col = MapCol(c);
	if (r >= 2 && MiniAtlasQuad(MiniSprite::Diamond, cx - r, cy - r, cx + r, cy + r, col)) return;
	RlwCmdDiamond(cx, cy, r, col);
}

static void FillTriangle(int cx, int cy, int r, uint32_t c)
{
	r = std::max(r, 1);
	uint32_t col = MapCol(c);
	if (r >= 2 && MiniAtlasQuad(MiniSprite::Triangle, cx - r, cy - r, cx + r, cy + r, col)) return;
	RlwCmdTriangle(cx, cy, r, col);
}

/* Rotated silhouette: ships and aircraft point along their heading. Without
 * an atlas the shape falls back unrotated. */
static void FillShapeRot(MiniSprite s, int cx, int cy, int r, int angle, uint32_t c)
{
	r = std::max(r, 1);
	uint32_t col = MapCol(c);
	if (r >= 2 && MiniAtlasQuadRot(s, cx, cy, r, angle, col)) return;
	switch (s) {
		case MiniSprite::Triangle:
		case MiniSprite::Aircraft:
			RlwCmdTriangle(cx, cy, r, col);
			break;
		case MiniSprite::Diamond:
		case MiniSprite::Ship:
			RlwCmdDiamond(cx, cy, r, col);
			break;
		default:
			RlwCmdCircle(cx, cy, r, col);
			break;
	}
}

/* Strings are laid out by the native font code into a small offscreen buffer
 * once, cached as a white-on-transparent texture and drawn as a tinted quad,
 * so any TrueType fallback font covers non-Latin names. */
struct MiniTextEntry {
	int tex;
	int w, h;
	uint64_t last_use;
};

static std::unordered_map<std::string, MiniTextEntry> _text_cache;
static uint64_t _mini_frame = 0;

static const MiniTextEntry *TextTexture(std::string_view text)
{
	if (text.empty()) return nullptr;
	auto it = _text_cache.find(std::string(text));
	if (it == _text_cache.end()) {
		Dimension dim = GetStringBoundingBox(text);
		int w = (int)dim.width;
		int h = (int)dim.height;
		if (w <= 0 || h <= 0) return nullptr;

		std::vector<uint32_t> buf((size_t)w * h, 0xFF000000U);
		DrawPixelInfo dpi;
		dpi.dst_ptr = buf.data();
		dpi.left = 0;
		dpi.top = 0;
		dpi.width = w;
		dpi.height = h;
		dpi.pitch = w;
		dpi.zoom = ZoomLevel::Min;
		{
			AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);
			AutoRestoreBackup anim_backup(_screen_disable_anim, true);
			DrawString(0, w - 1, 0, text, TC_WHITE, SA_LEFT | SA_FORCE);
		}
		for (uint32_t &px : buf) {
			uint32_t a = std::max({(px >> 16) & 0xFF, (px >> 8) & 0xFF, px & 0xFF});
			px = (a << 24) | 0x00FFFFFFU;
		}
		it = _text_cache.emplace(std::string(text), MiniTextEntry{RlwCreateTexture(buf.data(), w, h), w, h, 0}).first;
	}
	it->second.last_use = _mini_frame;
	return &it->second;
}

static void PruneTextCache()
{
	if ((_mini_frame & 0xFF) != 0) return;
	for (auto it = _text_cache.begin(); it != _text_cache.end();) {
		if (_mini_frame - it->second.last_use > 600) {
			RlwFreeTexture(it->second.tex);
			it = _text_cache.erase(it);
		} else {
			++it;
		}
	}
}

static constexpr uint32_t TextTint(TextColour colour)
{
	return colour == TC_BLACK ? 0xFF14181CU : 0xFFE6E1D3U;
}

static void DrawScreenText(int x, int y, std::string_view text, TextColour colour = TC_WHITE)
{
	const MiniTextEntry *e = TextTexture(text);
	if (e != nullptr) RlwCmdTexQuad(e->tex, x, y, TextTint(colour));
}

static void ScreenFillRect(int x0, int y0, int x1, int y1, uint32_t c)
{
	FillRect(x0, y0, x1, y1, c);
}

static void ChromePanel(int x0, int y0, int x1, int y1)
{
	int r = 2 * _ms.hud_scale;
	RlwCmdRoundRect(x0, y0, x1, y1, r, COL_CH_EDGE);
	RlwCmdRoundRect(x0 + 1, y0 + 1, x1 - 1, y1 - 1, r, COL_CH_PANEL);
}

static void DrawHudText(int x, int y, std::string_view text, int min_w = 0)
{
	int pad = 4;
	int w = std::max<int>(GetStringBoundingBox(text).width, min_w);
	int lh = GetCharacterHeight(FS_NORMAL);
	RlwCmdRoundRect(x - pad, y - pad, x + w + pad, y + lh + pad - 1, pad, (COL_CH_PANEL & 0x00FFFFFFU) | 0xF0000000U);
	DrawScreenText(x, y, text);
}

static void DrawHudTextCentred(int cx, int y, std::string_view text)
{
	DrawHudText(cx - GetStringBoundingBox(text).width / 2, y, text);
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

static MiniSprite GroundSlot(TileIndex tile)
{
	if (GetTileType(tile) == MP_CLEAR) {
		switch (GetClearGround(tile)) {
			case CLEAR_FIELDS: return MiniSprite::Field;
			case CLEAR_ROCKS: return MiniSprite::Rock;
			case CLEAR_SNOW: return MiniSprite::Snow;
			case CLEAR_DESERT: return MiniSprite::Desert;
			default: break;
		}
	}
	return MiniSprite::Grass;
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

static uint32_t GroundOverviewColour(TileIndex tile, Slope s, int hbase)
{
	double avg = (GetSlopeZInCorner(s, CORNER_N) + GetSlopeZInCorner(s, CORNER_W) + GetSlopeZInCorner(s, CORNER_E) + GetSlopeZInCorner(s, CORNER_S)) / 4.0;
	if (IsSpecialGround(tile)) return Mix(GroundColour(tile, hbase), COL_SHADOW, std::min(255, (int)(_ms.relief_strength * avg)));
	return RampLerp(hbase + avg);
}

/* Sloped ground is one gradient quad: the corner colours sample the height
 * ramp at the corner levels and the GPU interpolates between them, so the
 * top of a slope lands on exactly the colour of the next level and
 * gradients run tile to tile. */
static void DrawGround(TileIndex tile, int x0, int y0, int x1, int y1, int ppt)
{
	auto [s, hbase] = GetTileSlopeZ(tile);

	/* Ground art is a luminance texture tinted by the ramp colour, so height
	 * bands and hillshading survive the swap to real tiles. The overview tier
	 * stays on flat colours: per-tile quads are slow at that tile count and
	 * their seams read as a grid. */
	MiniSprite slot = GroundSlot(tile);
	if (ppt >= 8 && MiniAtlasHasArt(slot)) {
		uint32_t c = s == SLOPE_FLAT ? GroundColour(tile, hbase) : GroundOverviewColour(tile, s, hbase);
		if (MiniAtlasQuad(slot, x0, y0, x1, y1, MapCol(c))) return;
	}

	if (s == SLOPE_FLAT) {
		FillRect(x0, y0, x1, y1, GroundColour(tile, hbase));
		return;
	}

	if (ppt < 8) {
		FillRect(x0, y0, x1, y1, GroundOverviewColour(tile, s, hbase));
		return;
	}

	bool special = IsSpecialGround(tile);
	uint32_t flat = GroundColour(tile, hbase);
	auto corner = [&](Corner cn) {
		int z = GetSlopeZInCorner(s, cn);
		return MapCol(special ? Mix(flat, COL_SHADOW, std::min(255, _ms.relief_strength * z)) : RampLerp(hbase + z));
	};
	RlwCmdGradientRect(x0, y0, x1, y1, corner(CORNER_N), corner(CORNER_E), corner(CORNER_W), corner(CORNER_S));
}

static void DrawTrackPiece(Track t, int x0, int y0, int x1, int y1, int width, uint32_t c)
{
	int cx = (x0 + x1) / 2;
	int cy = (y0 + y1) / 2;
	switch (t) {
		case TRACK_X: FillRect(cx - width / 2, y0, cx - width / 2 + width - 1, y1, c); break;
		case TRACK_Y: FillRect(x0, cy - width / 2, x1, cy - width / 2 + width - 1, c); break;
		case TRACK_UPPER: ThickLine(x0, cy, cx, y0, width, c); break;
		case TRACK_LOWER: ThickLine(cx, y1, x1, cy, width, c); break;
		case TRACK_LEFT: ThickLine(x0, cy, cx, y1, width, c); break;
		case TRACK_RIGHT: ThickLine(cx, y0, x1, cy, width, c); break;
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
	if (bits & ROAD_NW) FillRect(x0, cy - lo, cx, cy - lo + width - 1, c);
	if (bits & ROAD_SE) FillRect(cx, cy - lo, x1, cy - lo + width - 1, c);
	if (bits & ROAD_NE) FillRect(cx - lo, y0, cx - lo + width - 1, cy, c);
	if (bits & ROAD_SW) FillRect(cx - lo, cy, cx - lo + width - 1, y1, c);
}

struct ZoomDetail {
	bool tree_dots;
	bool block_borders;
	bool signals;
	bool oneway;
	bool cargo_dots;
	bool vehicle_shapes;
	bool station_names;
	bool all_town_names;
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
	_zd.all_town_names = ppt >= 8;
}

static const int _diag_dx[4] = {0, 1, 0, -1};
static const int _diag_dy[4] = {-1, 0, 1, 0};

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
			FillCircle(px, py, r + 1, COL_INK);
			FillCircle(px, py, r, c);
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
			int py = cy + dir * (i - s / 2);
			FillRect(cx - w, py, cx + w, py, COL_PAPER);
		} else {
			int px = cx + dir * (i - s / 2);
			FillRect(px, cy - w, px, cy + w, COL_PAPER);
		}
	}
}

static void DrawBlock(MiniSprite s, int x0, int y0, int x1, int y1, int ppt, uint32_t fill, uint32_t border)
{
	if (ppt >= 8 && MiniAtlasHasArt(s) && MiniAtlasQuad(s, x0, y0, x1, y1, MapCol(fill))) return;
	if (!_zd.block_borders) {
		FillRect(x0, y0, x1, y1, fill);
		return;
	}
	int inset = std::max(1, ppt / 10);
	int b = std::max(1, ppt / 10);
	FillRect(x0 + inset, y0 + inset, x1 - inset, y1 - inset, border);
	FillRect(x0 + inset + b, y0 + inset + b, x1 - inset - b, y1 - inset - b, fill);
}

/* Dark block with a bright tick pointing out of the exit side. Depot art is
 * authored exit-up and rotates to the real exit instead of the tick. */
static void DrawDepot(int x0, int y0, int x1, int y1, int ppt, DiagDirection exit)
{
	if (ppt >= 8 && MiniAtlasHasArt(MiniSprite::Depot)) {
		if (MiniAtlasQuadRot(MiniSprite::Depot, (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0 + 1) / 2, exit * 90, MapCol(COL_DEPOT))) return;
	}
	DrawBlock(MiniSprite::Depot, x0, y0, x1, y1, ppt, COL_DEPOT, COL_INK);
	if (!_zd.block_borders) return;
	int cx = (x0 + x1) / 2;
	int cy = (y0 + y1) / 2;
	int w = std::max(2, ppt / 5);
	ThickLine(cx, cy, cx + _diag_dx[exit] * (ppt / 2), cy + _diag_dy[exit] * (ppt / 2), w, COL_PAPER);
}

static void DrawWater(int x0, int y0, int x1, int y1, int ppt)
{
	if (ppt >= 8 && MiniAtlasHasArt(MiniSprite::Water) && MiniAtlasQuad(MiniSprite::Water, x0, y0, x1, y1, MapCol(COL_WATER))) return;
	FillRect(x0, y0, x1, y1, COL_WATER);
}

static void DrawAxisBand(Axis axis, int x0, int y0, int x1, int y1, int width, uint32_t c)
{
	int cx = (x0 + x1) / 2;
	int cy = (y0 + y1) / 2;
	int lo = width / 2;
	if (axis == AXIS_X) {
		FillRect(cx - lo, y0, cx - lo + width - 1, y1, c);
	} else {
		FillRect(x0, cy - lo, x1, cy - lo + width - 1, c);
	}
}

static void DrawTile(TileIndex tile, int tx, int ty, int ppt)
{
	int x0 = ScrX(ty);
	int y0 = ScrY(tx);
	int x1 = ScrX(ty + 1) - 1;
	int y1 = ScrY(tx + 1) - 1;
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
			DrawWater(x0, y0, x1, y1, ppt);
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
				FillShapeRot(MiniSprite::Tree, cx, cy, r, 0, COL_TREE);
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
			DrawBlock(MiniSprite::House, x0, y0, x1, y1, ppt, COL_HOUSE, COL_HOUSE_B);
			break;

		case MP_INDUSTRY:
			DrawGround(tile, x0, y0, x1, y1, ppt);
			DrawBlock(MiniSprite::Industry, x0, y0, x1, y1, ppt, COL_IND, COL_IND_B);
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
				DrawWater(x0, y0, x1, y1, ppt);
				water_tile = true;
			} else {
				DrawGround(tile, x0, y0, x1, y1, ppt);
			}
			DrawBlock(MiniSprite::Station, x0, y0, x1, y1, ppt, fill, border);
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
			DrawBlock(MiniSprite::Object, x0, y0, x1, y1, ppt, COL_OBJ, COL_OBJ_B);
			break;

		case MP_TUNNELBRIDGE: {
			DrawGround(tile, x0, y0, x1, y1, ppt);
			Axis axis = DiagDirToAxis(GetTunnelBridgeDirection(tile));
			if (IsTunnel(tile)) {
				DrawBlock(MiniSprite::Tunnel, x0, y0, x1, y1, ppt, COL_TUNNEL, COL_RAIL);
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
		if (tx + 1 < (int)Map::SizeX() && TileHeight(TileXY(tx + 1, ty)) != h) BlendRect(x0, y1 - cw + 1, x1, y1, COL_SHADOW, _ms.contour_alpha);
		if (ty + 1 < (int)Map::SizeY() && TileHeight(TileXY(tx, ty + 1)) != h) BlendRect(x1 - cw + 1, y0, x1, y1, COL_SHADOW, _ms.contour_alpha);
	}
}

/* Second pass for the overlay: the base map went down as dark greyscale and
 * the active layer's content is repainted in a bright accent so it carries
 * the frame. Rail reads as paper-white lines, road as catenary-yellow. */
static void DrawTileLayer(TileIndex tile, int tx, int ty, int ppt, MiniLayer layer)
{
	int x0 = ScrX(ty);
	int y0 = ScrY(tx);
	int x1 = ScrX(ty + 1) - 1;
	int y1 = ScrY(tx + 1) - 1;
	if (x1 < 0 || y1 < 0 || x0 >= _fbw || y0 >= _fbh) return;

	int rail_w = std::max(1, ppt / 6);
	int road_w = std::max(2, ppt / 3);
	int cat_w = rail_w >= 2 ? std::max(1, rail_w / 3) : 0;
	bool rail = layer == MiniLayer::Rail;
	uint32_t accent = rail ? COL_PAPER : COL_CATENARY;

	auto depot = [&](DiagDirection exit) {
		if (ppt >= 8 && MiniAtlasHasArt(MiniSprite::Depot)) {
			if (MiniAtlasQuadRot(MiniSprite::Depot, (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0 + 1) / 2, exit * 90, MapCol(accent))) return;
		}
		uint32_t fill = Darken(accent);
		DrawBlock(MiniSprite::Depot, x0, y0, x1, y1, ppt, fill, accent);
		if (!_zd.block_borders) return;
		uint lum = (77 * ((fill >> 16) & 0xFFU) + 151 * ((fill >> 8) & 0xFFU) + 28 * (fill & 0xFFU)) >> 8;
		int cx = (x0 + x1) / 2;
		int cy = (y0 + y1) / 2;
		int w = std::max(2, ppt / 5);
		ThickLine(cx, cy, cx + _diag_dx[exit] * (ppt / 2), cy + _diag_dy[exit] * (ppt / 2), w, lum >= 140 ? COL_INK : COL_PAPER);
	};

	switch (GetTileType(tile)) {
		case MP_RAILWAY:
			if (!rail) break;
			if (IsRailDepot(tile)) {
				depot(GetRailDepotDirection(tile));
			} else {
				TrackBits bits = GetTrackBits(tile);
				DrawTrackBitsPx(bits, x0, y0, x1, y1, rail_w, accent);
				if (cat_w > 0 && HasRailCatenary(GetRailType(tile))) {
					DrawTrackBitsPx(bits, x0, y0, x1, y1, cat_w, COL_CATENARY);
				}
				if (HasSignals(tile)) DrawSignals(tile, x0, y0, x1, y1, ppt);
			}
			break;

		case MP_ROAD:
			if (IsLevelCrossing(tile)) {
				if (rail) {
					DrawTrackBitsPx(GetCrossingRailBits(tile), x0, y0, x1, y1, rail_w, accent);
				} else {
					DrawAxisBand(GetCrossingRoadAxis(tile), x0, y0, x1, y1, road_w, accent);
				}
			} else if (!rail) {
				if (IsRoadDepot(tile)) {
					depot(GetRoadDepotDirection(tile));
				} else {
					RoadBits bits = GetAnyRoadBits(tile, RTT_ROAD, true) | GetAnyRoadBits(tile, RTT_TRAM, true);
					DrawRoadBitsPx(bits, x0, y0, x1, y1, road_w, accent);
					if (IsNormalRoad(tile)) DrawOneWay(tile, x0, y0, x1, y1, ppt);
				}
			}
			break;

		case MP_STATION:
			switch (GetStationType(tile)) {
				case StationType::Rail:
				case StationType::RailWaypoint:
					if (!rail) break;
					DrawBlock(MiniSprite::Station, x0, y0, x1, y1, ppt, COL_ST_RAIL, COL_ST_RAIL_B);
					DrawAxisBand(GetRailStationAxis(tile), x0, y0, x1, y1, rail_w, accent);
					if (cat_w > 0 && HasRailCatenary(GetRailType(tile))) {
						DrawAxisBand(GetRailStationAxis(tile), x0, y0, x1, y1, cat_w, COL_CATENARY);
					}
					break;
				case StationType::Truck:
				case StationType::Bus:
				case StationType::RoadWaypoint:
					if (rail) break;
					DrawBlock(MiniSprite::Station, x0, y0, x1, y1, ppt, COL_ST_ROAD, COL_ST_ROAD_B);
					if (IsDriveThroughStopTile(tile)) {
						DrawAxisBand(GetDriveThroughStopAxis(tile), x0, y0, x1, y1, road_w, accent);
					}
					break;
				default:
					break;
			}
			break;

		case MP_TUNNELBRIDGE: {
			TransportType tt = GetTunnelBridgeTransportType(tile);
			if (rail ? tt != TRANSPORT_RAIL : tt != TRANSPORT_ROAD) break;
			Axis axis = DiagDirToAxis(GetTunnelBridgeDirection(tile));
			if (IsTunnel(tile)) {
				DrawBlock(MiniSprite::Tunnel, x0, y0, x1, y1, ppt, COL_TUNNEL, accent);
			} else {
				DrawAxisBand(axis, x0, y0, x1, y1, road_w, accent);
				if (cat_w > 0 && rail && HasRailCatenary(GetRailType(tile))) {
					DrawAxisBand(axis, x0, y0, x1, y1, cat_w, COL_CATENARY);
				}
			}
			break;
		}

		default:
			break;
	}

	if (IsBridgeAbove(tile)) {
		TransportType tt = GetTunnelBridgeTransportType(GetSouthernBridgeEnd(tile));
		if (rail ? tt == TRANSPORT_RAIL : tt == TRANSPORT_ROAD) {
			DrawAxisBand(GetBridgeAxis(tile), x0, y0, x1, y1, road_w, accent);
		}
	}
}

/* A tile whose whole footprint is one solid colour can join a horizontal run
 * with equal neighbours; one rect per run keeps the command count far below
 * one per tile on open terrain and water. Tree tiles merge their ground too
 * and only defer the dot on top. Ground with art still merges: the run draws
 * as one repeat-wrapped quad instead of a rect, keyed by the art slot. */
static bool TileRunColour(TileIndex tile, int tx, int ty, int ppt, uint32_t &c, bool &tree_dot, MiniSprite &art)
{
	tree_dot = false;
	art = MiniSprite::End;
	if (IsBridgeAbove(tile)) return false;
	switch (GetTileType(tile)) {
		case MP_VOID:
			c = COL_VOID;
			return true;

		case MP_WATER:
			if (IsShipDepot(tile)) return false;
			c = COL_WATER;
			if (ppt >= 8 && MiniAtlasHasArt(MiniSprite::Water)) art = MiniSprite::Water;
			return true;

		case MP_TREES:
			tree_dot = _zd.tree_dots;
			[[fallthrough]];
		case MP_CLEAR: {
			auto [s, hbase] = GetTileSlopeZ(tile);
			if (s == SLOPE_FLAT) {
				c = GroundColour(tile, hbase);
				if (ppt >= 8) {
					MiniSprite g = GroundSlot(tile);
					if (MiniAtlasHasArt(g)) art = g;
				}
			} else if (ppt < 8) {
				c = GroundOverviewColour(tile, s, hbase);
			} else {
				return false;
			}
			uint h = TileHeight(tile);
			if (tx + 1 < (int)Map::SizeX() && TileHeight(TileXY(tx + 1, ty)) != h) return false;
			if (ty + 1 < (int)Map::SizeY() && TileHeight(TileXY(tx, ty + 1)) != h) return false;
			return true;
		}

		default:
			return false;
	}
}

static const int8_t _dir_dx[8] = {-1, 0, 1, 1, 1, 0, -1, -1};
static const int8_t _dir_dy[8] = {-1, -1, -1, 0, 1, 1, 1, 0};

/* Screen-space heading in degrees clockwise from up, per Direction. */
static const int16_t _dir_angle[8] = {-45, 0, 45, 90, 135, 180, -135, -90};

/* Vehicles only move on game ticks while drawing runs at render rate, so
 * raw positions stutter. Each frame interpolates between a unit's previous
 * and current tick position; the fraction comes from a smoothed measure of
 * the real tick interval, which also absorbs fast forward. */
struct MiniVehSnap {
	int32_t px, py;
	int32_t cx, cy;
	uint64_t tick;
};

static std::unordered_map<uint32_t, MiniVehSnap> _veh_snap;
static uint64_t _lerp_tick = 0;
static double _lerp_since = 0.0;
static double _lerp_interval = 30.0;
static double _lerp_alpha = 1.0;

static void UpdateLerpClock(uint delta_ms)
{
	_lerp_since += delta_ms;
	uint64_t t = TimerGameTick::counter;
	if (t != _lerp_tick) {
		double per = _lerp_since / (double)(t - _lerp_tick);
		if (per >= 5.0 && per <= 200.0) _lerp_interval = _lerp_interval * 0.7 + per * 0.3;
		_lerp_tick = t;
		_lerp_since = 0.0;
	}
	_lerp_alpha = std::min(_lerp_since / _lerp_interval, 1.0);
	if ((_mini_frame & 0xFF) == 0) {
		std::erase_if(_veh_snap, [](const auto &kv) { return kv.second.tick + 64 < _lerp_tick; });
	}
}

/* Returns the display position in tile units. Entries older than one tick
 * and jumps wider than two tiles snap instead of streaking. */
static std::pair<double, double> LerpVehWorld(const Vehicle *v)
{
	MiniVehSnap &e = _veh_snap[v->index.base()];
	if (e.tick != _lerp_tick) {
		if (e.tick + 1 == _lerp_tick) {
			e.px = e.cx;
			e.py = e.cy;
		} else {
			e.px = v->x_pos;
			e.py = v->y_pos;
		}
		e.cx = v->x_pos;
		e.cy = v->y_pos;
		e.tick = _lerp_tick;
	}
	if (std::abs(e.cx - e.px) > 32 || std::abs(e.cy - e.py) > 32) {
		e.px = e.cx;
		e.py = e.cy;
	}
	return {(e.px + (e.cx - e.px) * _lerp_alpha) / TILE_SIZE, (e.py + (e.cy - e.py) * _lerp_alpha) / TILE_SIZE};
}

static uint32_t CargoRgb(CargoType ct)
{
	Colour c = _cur_palette.palette[CargoSpec::Get(ct)->legend_colour.p];
	return 0xFF000000U | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

/* Silhouette tells the vehicle type apart: square train, round road
 * vehicle, diamond ship, triangle aircraft. The centre dot is the unit's
 * cargo in its legend colour. */
static bool VehicleInLayer(VehicleType vt)
{
	switch (_filter_layer) {
		case MiniLayer::Rail: return vt == VEH_TRAIN;
		case MiniLayer::Road: return vt == VEH_ROAD;
		default: return true;
	}
}

/* Shapes without a clear nose carry a paper dot on the leading edge, the
 * same accent as the train head dot. */
static void DrawHeadingDot(int cx, int cy, int r, Direction dir)
{
	if (r < 3) return;
	int d = dir;
	double inv = (_dir_dx[d] != 0 && _dir_dy[d] != 0) ? 0.70710678 : 1.0;
	int px = cx + (int)std::lround(_dir_dx[d] * inv * 0.62 * r);
	int py = cy + (int)std::lround(_dir_dy[d] * inv * 0.62 * r);
	FillCircle(px, py, std::max(1, (r + 1) / 3), COL_PAPER);
}

/* A consist draws as one polyline through its unit centres. The game spaces
 * units in path steps, and a cardinal step moves both map axes, so raw
 * centres sit sqrt(2) apart on diagonals; the native isometric projection
 * cancels that but a top-down view shows it as a stretched train. Units are
 * therefore re-laid along their own polyline at true unit lengths from the
 * head, which keeps the drawn length constant on any mix of track. Ink
 * underlays the whole line before the colour pass, so joints stay clean. */
static void DrawTrainConsist(const Vehicle *head, int ppt)
{
	static std::vector<std::pair<double, double>> raw;
	static std::vector<double> arc;
	static std::vector<double> want;
	static std::vector<std::pair<int, int>> pts;
	raw.clear();
	arc.clear();
	want.clear();
	pts.clear();

	const Vehicle *tail = head;
	double s = 0.0;
	double prev_len = 0.0;
	for (const Vehicle *u = head; u != nullptr; u = u->Next()) {
		auto [ux, uy] = LerpVehWorld(u);
		raw.emplace_back(uy * _cam_ppt + ScrBaseX(), ux * _cam_ppt + ScrBaseY());
		double len = u->GetGroundVehicleCache()->cached_veh_length * ppt / (double)TILE_SIZE;
		if (!want.empty()) s += (prev_len + len) * 0.5;
		want.push_back(s);
		prev_len = len;
		tail = u;
	}

	arc.resize(raw.size());
	arc[0] = 0.0;
	for (size_t i = 1; i < raw.size(); i++) {
		double dx = raw[i].first - raw[i - 1].first;
		double dy = raw[i].second - raw[i - 1].second;
		arc[i] = arc[i - 1] + std::sqrt(dx * dx + dy * dy);
	}

	size_t seg = 0;
	for (size_t i = 0; i < raw.size(); i++) {
		double t = want[i];
		double x, y;
		if (t >= arc.back()) {
			x = raw.back().first;
			y = raw.back().second;
			if (raw.size() >= 2) {
				double dx = x - raw[raw.size() - 2].first;
				double dy = y - raw[raw.size() - 2].second;
				double d = std::sqrt(dx * dx + dy * dy);
				if (d > 0.0) {
					x += dx / d * (t - arc.back());
					y += dy / d * (t - arc.back());
				}
			}
		} else {
			while (seg + 2 < raw.size() && arc[seg + 1] <= t) seg++;
			double span = arc[seg + 1] - arc[seg];
			double f = span > 0.0 ? (t - arc[seg]) / span : 0.0;
			x = raw[seg].first + (raw[seg + 1].first - raw[seg].first) * f;
			y = raw[seg].second + (raw[seg + 1].second - raw[seg].second) * f;
		}
		pts.emplace_back((int)std::lround(x), (int)std::lround(y));
	}

	/* The nose and tail stick out half a unit length past the end centres. */
	auto overhang = [&](const Vehicle *u, int sign) {
		double len = u->GetGroundVehicleCache()->cached_veh_length * ppt / (double)TILE_SIZE;
		double inv = (_dir_dx[u->direction] != 0 && _dir_dy[u->direction] != 0) ? 0.70710678 : 1.0;
		return std::pair<int, int>(
				(int)std::lround(sign * _dir_dx[u->direction] * inv * len * 0.5),
				(int)std::lround(sign * _dir_dy[u->direction] * inv * len * 0.5));
	};
	auto [nx, ny] = overhang(head, 1);
	pts.insert(pts.begin(), {pts.front().first + nx, pts.front().second + ny});
	auto [bx, by] = overhang(tail, -1);
	pts.emplace_back(pts.back().first + bx, pts.back().second + by);

	int w = std::max(2, ppt / 4);
	int m = w + 2;
	int minx = pts[0].first, maxx = minx, miny = pts[0].second, maxy = miny;
	for (auto [x, y] : pts) {
		minx = std::min(minx, x);
		maxx = std::max(maxx, x);
		miny = std::min(miny, y);
		maxy = std::max(maxy, y);
	}
	if (maxx < -m || maxy < -m || minx >= _fbw + m || miny >= _fbh + m) return;

	uint32_t c = Company::IsValidID(head->owner) ? _company_rgb[_company_colours[head->owner]] : COL_OBJ;
	bool dim = !VehicleInLayer(VEH_TRAIN);
	uint32_t ink = COL_INK;
	if (dim) {
		ink = GreyMap(COL_INK);
		c = GreyMap(c);
	}

	for (size_t i = 0; i + 1 < pts.size(); i++) ThickLine(pts[i].first, pts[i].second, pts[i + 1].first, pts[i + 1].second, w + 2, ink);
	for (size_t i = 1; i + 1 < pts.size(); i++) FillCircle(pts[i].first, pts[i].second, (w + 2) / 2, ink);
	for (size_t i = 0; i + 1 < pts.size(); i++) ThickLine(pts[i].first, pts[i].second, pts[i + 1].first, pts[i + 1].second, w, c);
	for (size_t i = 1; i + 1 < pts.size(); i++) FillCircle(pts[i].first, pts[i].second, std::max(1, w / 2), c);

	if (!dim) FillCircle(pts[0].first, pts[0].second, std::max(1, w / 2 - 1), COL_PAPER);

	if (!dim && _zd.cargo_dots) {
		int half = std::max(3, ppt * 2 / 5) / 2;
		int dr = std::max(1, half - 2);
		size_t i = 1;
		for (const Vehicle *u = head; u != nullptr; u = u->Next(), i++) {
			if (u->cargo_cap == 0 || !IsValidCargoType(u->cargo_type)) continue;
			FillCircle(pts[i].first, pts[i].second, dr + 1, COL_INK);
			FillCircle(pts[i].first, pts[i].second, dr, CargoRgb(u->cargo_type));
		}
	}
}

static void DrawVehicles(int ppt)
{
	int half = std::max(3, ppt * 2 / 5) / 2;
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->type > VEH_AIRCRAFT) continue;
		if (v->vehstatus.Test(VehState::Hidden)) continue;
		if (v->type == VEH_AIRCRAFT && !v->IsPrimaryVehicle()) continue;
		if (v->type == VEH_TRAIN && _zd.vehicle_shapes) {
			if (v->IsPrimaryVehicle()) DrawTrainConsist(v, ppt);
			continue;
		}
		int r = (v->type == VEH_SHIP || v->type == VEH_AIRCRAFT) ? half + 2 : half;
		auto [wx, wy] = LerpVehWorld(v);
		int cx = ScrX(wy);
		int cy = ScrY(wx);
		if (cx < -r - 1 || cy < -r - 1 || cx >= _fbw + r + 1 || cy >= _fbh + r + 1) continue;
		uint32_t c = Company::IsValidID(v->owner) ? _company_rgb[_company_colours[v->owner]] : COL_OBJ;
		bool dim = !VehicleInLayer(v->type);
		uint32_t ink = COL_INK;
		if (dim) {
			ink = GreyMap(COL_INK);
			c = GreyMap(c);
		}
		if (!_zd.vehicle_shapes) {
			FillRect(cx - 1, cy - 1, cx + 1, cy + 1, c);
			continue;
		}
		int angle = _dir_angle[v->direction];
		switch (v->type) {
			case VEH_ROAD:
				FillShapeRot(MiniSprite::RoadVeh, cx, cy, r + 1, angle, ink);
				FillShapeRot(MiniSprite::RoadVeh, cx, cy, r, angle, c);
				if (!dim) DrawHeadingDot(cx, cy, r, v->direction);
				break;
			case VEH_SHIP:
				/* The diamond only shows its axis when rotated; the head dot
				 * picks which end leads. */
				FillShapeRot(MiniSprite::Ship, cx, cy, r + 1, angle, ink);
				FillShapeRot(MiniSprite::Ship, cx, cy, r, angle, c);
				if (!dim) DrawHeadingDot(cx, cy, r, v->direction);
				break;
			case VEH_AIRCRAFT:
				FillShapeRot(MiniSprite::Aircraft, cx, cy, r + 1, angle, ink);
				FillShapeRot(MiniSprite::Aircraft, cx, cy, r, angle, c);
				break;
			default:
				break;
		}
		if (!dim && _zd.cargo_dots && v->cargo_cap > 0 && IsValidCargoType(v->cargo_type)) {
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
	int tx = (int)std::floor(MapXAt(_cursor.pos.y));
	int ty = (int)std::floor(MapYAt(_cursor.pos.x));
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
	Command<CMD_BUILD_VEHICLE>::Post(STR_ERROR_CAN_T_BUY_TRAIN + vt, tile, eid, true, INVALID_CARGO, INVALID_CLIENT_ID);
}

/* Clicking a compatible station with a vehicle selected appends a go-to
 * order, mirroring the defaults of the order window's goto click. */
static bool TryAppendOrder(int sx, int sy)
{
	const Vehicle *v = Vehicle::GetIfValid(_sel_vehicle);
	if (v == nullptr || !v->IsPrimaryVehicle() || v->owner != _local_company) return false;

	int tx = (int)std::floor(MapXAt(sy));
	int ty = (int)std::floor(MapYAt(sx));
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
	Command<CMD_INSERT_ORDER>::Post(STR_ERROR_CAN_T_INSERT_NEW_ORDER, v->tile, v->index, (VehicleOrderID)v->GetNumOrders(), order);
	return true;
}

/* Any unit of a consist selects its head, so the info line always
 * describes the whole vehicle. */
static const Vehicle *SelectVehicleAt(int sx, int sy)
{
	const Vehicle *best = nullptr;
	int best_d2 = 15 * 15;
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->type > VEH_AIRCRAFT) continue;
		if (v->vehstatus.Test(VehState::Hidden)) continue;
		int dx = ScrX(v->y_pos / (double)TILE_SIZE) - sx;
		int dy = ScrY(v->x_pos / (double)TILE_SIZE) - sy;
		int d2 = dx * dx + dy * dy;
		if (d2 < best_d2) {
			best_d2 = d2;
			best = v;
		}
	}
	if (best == nullptr) {
		_sel_vehicle = VehicleID::Invalid();
		return nullptr;
	}
	_sel_vehicle = best->First()->index;
	return best->First();
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
				stops.emplace_back(ScrX(TileY(st->xy) + 0.5), ScrY(TileX(st->xy) + 0.5));
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
		auto [wx, wy] = LerpVehWorld(v);
		int vx = ScrX(wy);
		int vy = ScrY(wx);
		ThickLine(vx, vy, stops[cur_stop].first, stops[cur_stop].second, 2, COL_PAPER);
	}
}

static void DrawSelectionRing(int ppt)
{
	const Vehicle *v = Vehicle::GetIfValid(_sel_vehicle);
	if (v == nullptr) return;
	auto [wx, wy] = LerpVehWorld(v);
	int cx = ScrX(wy);
	int cy = ScrY(wx);
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

static TextColour PlateTextColour(uint32_t c)
{
	uint lum = (77 * ((c >> 16) & 0xFFU) + 151 * ((c >> 8) & 0xFFU) + 28 * (c & 0xFFU)) >> 8;
	return lum >= 140 ? TC_BLACK : TC_WHITE;
}

/* Flat mini-style plate; drawn in mini UI screen space because the native
 * sign kdtree lives in viewport coordinates. */
static Rect DrawLabelPlate(int cx, int cy, std::string_view str, uint32_t fill, bool transparent, TextColour tc)
{
	int pad = 3;
	const MiniTextEntry *e = TextTexture(str);
	int tw = e != nullptr ? e->w : (int)GetStringBoundingBox(str).width;
	int w = tw + 2 * pad;
	int h = GetCharacterHeight(FS_NORMAL) + 2 * pad;
	Rect r = {cx - w / 2, cy - h - 3, cx - w / 2 + w - 1, cy - 4};
	if (transparent) {
		RlwCmdRoundRect(r.left, r.top, r.right, r.bottom, pad, (fill & 0x00FFFFFFU) | 0xAA000000U);
	} else {
		RlwCmdRoundRect(r.left, r.top, r.right, r.bottom, pad, COL_CH_EDGE);
		RlwCmdRoundRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, pad, fill);
	}
	if (e != nullptr) RlwCmdTexQuad(e->tex, r.left + pad, r.top + pad, TextTint(tc));
	return r;
}

static void DrawLabels()
{
	_town_label_hits.clear();
	_station_label_hits.clear();
	int margin = 300;
	int limit = GetCharacterHeight(FS_NORMAL) + 20;
	for (const Town *t : Town::Iterate()) {
		if (!_zd.all_town_names && !t->larger_town) continue;
		int cx = ScrX(TileY(t->xy) + 0.5);
		int cy = ScrY(TileX(t->xy) + 0.5);
		if (cx < -margin || cy < 0 || cx >= _fbw + margin || cy >= _fbh + limit) continue;
		std::string str = GetString(t->larger_town ? STR_VIEWPORT_TOWN_CITY_POP : STR_VIEWPORT_TOWN_POP, t->index, t->cache.population);
		Rect r = DrawLabelPlate(cx, cy, str, COL_CH_PANEL, true, TC_WHITE);
		_town_label_hits.emplace_back(r, t->index);
	}
	if (!_zd.station_names) return;
	for (const Station *st : Station::Iterate()) {
		int cx = ScrX(TileY(st->xy) + 0.5);
		int cy = ScrY(TileX(st->xy) + 0.5);
		if (cx < -margin || cy < 0 || cx >= _fbw + margin || cy >= _fbh + limit) continue;
		std::string str = GetString(STR_VIEWPORT_STATION, st->index, st->facilities);
		uint32_t plate = (st->owner == OWNER_NONE || !st->IsInUse()) ? COL_OBJ : _company_rgb[_company_colours[st->owner]];
		Rect r = DrawLabelPlate(cx, cy, str, plate, false, PlateTextColour(plate));
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
/* Pipe-style placement: the drag lays a free-form path that follows the
 * cursor tile by tile and turns where the cursor turns; stepping back onto
 * the previous tile undoes the last step. Pieces derive from the pairs of
 * tile edges the path crosses. Edge bits: 1 = -x, 2 = +x, 4 = -y, 8 = +y. */
static int StepBit(TileIndex from, TileIndex to)
{
	if ((int)TileX(to) < (int)TileX(from)) return 1;
	if ((int)TileX(to) > (int)TileX(from)) return 2;
	if ((int)TileY(to) < (int)TileY(from)) return 4;
	return 8;
}

static int OppositeBit(int b)
{
	switch (b) {
		case 1: return 2;
		case 2: return 1;
		case 4: return 8;
		default: return 4;
	}
}

static Track EdgePairTrack(int mask)
{
	switch (mask) {
		case 1 | 2: return TRACK_X;
		case 4 | 8: return TRACK_Y;
		case 1 | 4: return TRACK_UPPER;
		case 2 | 8: return TRACK_LOWER;
		case 2 | 4: return TRACK_LEFT;
		case 1 | 8: return TRACK_RIGHT;
		default: return INVALID_TRACK;
	}
}

static void RailPathPieces()
{
	_plan.pieces.clear();
	size_t n = _plan.path.size();
	if (n < 2) return;
	for (size_t i = 0; i < n; i++) {
		int in = i > 0 ? OppositeBit(StepBit(_plan.path[i - 1], _plan.path[i])) : 0;
		int out = i + 1 < n ? StepBit(_plan.path[i], _plan.path[i + 1]) : 0;
		if (in == 0) in = OppositeBit(out);
		if (out == 0) out = OppositeBit(in);
		Track t = EdgePairTrack(in | out);
		if (t != INVALID_TRACK) _plan.pieces.emplace_back(_plan.path[i], t);
	}
}

static void UpdateRailPlan(double wx, double wy)
{
	int tx = Clamp<int>((int)std::floor(wx), 0, Map::SizeX() - 2);
	int ty = Clamp<int>((int)std::floor(wy), 0, Map::SizeY() - 2);

	if (_plan.path.empty()) {
		int ax = Clamp<int>((int)std::floor(_drag_ax), 0, Map::SizeX() - 2);
		int ay = Clamp<int>((int)std::floor(_drag_ay), 0, Map::SizeY() - 2);
		_plan.path.push_back(TileXY(ax, ay));
	}

	while (_plan.path.size() < 1024) {
		TileIndex cur = _plan.path.back();
		int cx = (int)TileX(cur), cy = (int)TileY(cur);
		int dx = tx - cx, dy = ty - cy;
		if (dx == 0 && dy == 0) break;
		int nx = cx, ny = cy;
		if (std::abs(dx) >= std::abs(dy)) nx += dx > 0 ? 1 : -1; else ny += dy > 0 ? 1 : -1;
		TileIndex next = TileXY(nx, ny);
		if (_plan.path.size() >= 2 && next == _plan.path[_plan.path.size() - 2]) {
			_plan.path.pop_back();
		} else {
			_plan.path.push_back(next);
		}
	}

	RailPathPieces();
}

static void DrawRailPlan(int ppt)
{
	uint32_t c = _drag_remove ? COL_BP_RM : COL_BP;
	int w = std::max(2, ppt / 5);
	for (const auto &[tile, t] : _plan.pieces) {
		int tx = TileX(tile);
		int ty = TileY(tile);
		int x0 = ScrX(ty);
		int y0 = ScrY(tx);
		int x1 = ScrX(ty + 1) - 1;
		int y1 = ScrY(tx + 1) - 1;
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
	_plan.path.clear();
	_road_plan.tiles.clear();
	_road_plan.start = INVALID_TILE;
	_rect_plan.valid = false;
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

/* A straight drag bridges water automatically: each water run becomes a
 * bridge between its neighbouring land tiles, which turn into ramps, so
 * the land spans stop short of them. */
struct MiniSpans {
	bool ok = false;
	std::vector<std::pair<int, int>> land;
	std::vector<std::pair<int, int>> bridges;
};

static MiniSpans SplitWaterSpans(std::span<const TileIndex> ts)
{
	MiniSpans out;
	int n = (int)ts.size();
	std::vector<uint8_t> head(n, 0);
	for (int i = 0; i < n; i++) {
		if (!IsTileType(ts[i], MP_WATER)) continue;
		int j = i;
		while (j + 1 < n && IsTileType(ts[j + 1], MP_WATER)) j++;
		if (i == 0 || j == n - 1) return out;
		head[i - 1] = head[j + 1] = 1;
		out.bridges.emplace_back(i - 1, j + 1);
		i = j;
	}
	for (int i = 0; i < n; i++) {
		if (IsTileType(ts[i], MP_WATER) || head[i] != 0) continue;
		int j = i;
		while (j + 1 < n && !IsTileType(ts[j + 1], MP_WATER) && head[j + 1] == 0) j++;
		out.land.emplace_back(i, j);
		i = j;
	}
	out.ok = true;
	return out;
}

static void CommitRailPlan()
{
	if (_plan.pieces.empty()) {
		ClearPlans();
		return;
	}

	if (_drag_remove) {
		for (const auto &[tile, t] : _plan.pieces) {
			Command<CMD_REMOVE_RAILROAD_TRACK>::Post(STR_ERROR_CAN_T_REMOVE_RAILROAD_TRACK, tile, tile, t);
		}
		ClearPlans();
		return;
	}

	/* Maximal straight runs go through the range command so the water
	 * auto-bridge logic still applies; corner pieces commit tile by tile. */
	RailType rt = PickRailType();
	size_t i = 0;
	while (i < _plan.pieces.size()) {
		auto [tile, t] = _plan.pieces[i];
		size_t j = i;
		if (t == TRACK_X || t == TRACK_Y) {
			int dir = 0;
			while (j + 1 < _plan.pieces.size() && _plan.pieces[j + 1].second == t) {
				int step = StepBit(_plan.pieces[j].first, _plan.pieces[j + 1].first);
				if (dir != 0 && step != dir) break;
				dir = step;
				j++;
			}
		}
		if (j > i) {
			std::vector<TileIndex> tiles;
			for (size_t k = i; k <= j; k++) tiles.push_back(_plan.pieces[k].first);
			MiniSpans spans = SplitWaterSpans(tiles);
			if (!spans.ok) {
				Command<CMD_BUILD_RAILROAD_TRACK>::Post(STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK, tiles.back(), tiles.front(), rt, t, true, false);
			} else {
				for (auto [a, b] : spans.bridges) {
					Command<CMD_BUILD_BRIDGE>::Post(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE, tiles[b], tiles[a], TRANSPORT_RAIL, PickBridgeType((uint)(b - a - 1)), (uint8_t)rt);
				}
				for (auto [a, b] : spans.land) {
					Command<CMD_BUILD_RAILROAD_TRACK>::Post(STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK, tiles[b], tiles[a], rt, t, true, false);
				}
			}
		} else {
			Command<CMD_BUILD_RAILROAD_TRACK>::Post(STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK, tile, tile, rt, t, true, false);
		}
		i = j + 1;
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
		DrawAxisBand(_road_plan.axis, ScrX(ty), ScrY(tx), ScrX(ty + 1) - 1, ScrY(tx + 1) - 1, w, c);
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
	int px0 = ScrX(_rect_plan.y0);
	int py0 = ScrY(_rect_plan.x0);
	int px1 = ScrX(_rect_plan.y1 + 1) - 1;
	int py1 = ScrY(_rect_plan.x1 + 1) - 1;
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
		Command<CMD_REMOVE_FROM_RAIL_STATION>::Post(STR_ERROR_CAN_T_REMOVE_PART_OF_STATION, org, TileXY(_rect_plan.x1, _rect_plan.y1), true);
	} else {
		Axis axis = w >= h ? AXIS_X : AXIS_Y;
		uint8_t plat_len = (uint8_t)(axis == AXIS_X ? w : h);
		uint8_t numtracks = (uint8_t)(axis == AXIS_X ? h : w);
		Command<CMD_BUILD_RAIL_STATION>::Post(STR_ERROR_CAN_T_BUILD_RAILROAD_STATION, org, PickRailType(), axis, numtracks, plat_len, STAT_CLASS_DFLT, 0, StationID::Invalid(), false);
	}
	ClearPlans();
}

static void CommitDemolishPlan()
{
	if (!_rect_plan.valid) return;
	Command<CMD_CLEAR_AREA>::Post(STR_ERROR_CAN_T_CLEAR_THIS_AREA, TileXY(_rect_plan.x1, _rect_plan.y1), TileXY(_rect_plan.x0, _rect_plan.y0), false);
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
	Command<CMD_LEVEL_LAND>::Post(STR_ERROR_CAN_T_LEVEL_LAND_HERE, end, anchor, false, lm);
	ClearPlans();
}

/* Same sub-track pick as GenericPlaceSignals: on paired straight pieces the
 * fractional click position decides which half gets the signal. */
static Track PickSignalTrack(TileIndex tile, double wx, double wy)
{
	if (!IsPlainRailTile(tile)) return INVALID_TRACK;
	TrackBits trackbits = GetTrackBits(tile);
	double fx = wx - std::floor(wx);
	double fy = wy - std::floor(wy);
	if (trackbits & TRACK_BIT_VERT) trackbits = (fx <= fy) ? TRACK_BIT_RIGHT : TRACK_BIT_LEFT;
	if (trackbits & TRACK_BIT_HORZ) trackbits = (fx + fy <= 1.0) ? TRACK_BIT_UPPER : TRACK_BIT_LOWER;
	return FindFirstTrack(trackbits);
}

/* Point tools place on click: the blueprint floats on the hover tile and
 * Q/E spin _point_dir, so no drag gesture is involved. */
static void CommitPointTool()
{
	int tx = Clamp<int>((int)std::floor(_drag_ax), 1, Map::SizeX() - 2);
	int ty = Clamp<int>((int)std::floor(_drag_ay), 1, Map::SizeY() - 2);
	TileIndex tile = TileXY(tx, ty);

	switch (_tool) {
		case MiniTool::BusStop:
		case MiniTool::TruckStop: {
			bool bus = _tool == MiniTool::BusStop;
			RoadStopType st = bus ? RoadStopType::Bus : RoadStopType::Truck;
			if (_drag_remove) {
				Command<CMD_REMOVE_ROAD_STOP>::Post(bus ? STR_ERROR_CAN_T_REMOVE_BUS_STATION : STR_ERROR_CAN_T_REMOVE_TRUCK_STATION, tile, 1, 1, st, false);
			} else {
				DiagDirection ddir = AxisToDiagDir(DiagDirToAxis(_point_dir));
				Command<CMD_BUILD_ROAD_STOP>::Post(bus ? STR_ERROR_CAN_T_BUILD_BUS_STATION : STR_ERROR_CAN_T_BUILD_TRUCK_STATION, tile, 1, 1, st, true, ddir, PickRoadType(), ROADSTOP_CLASS_DFLT, 0, StationID::Invalid(), false);
			}
			break;
		}

		case MiniTool::TrainDepot:
			if (_drag_remove) {
				Command<CMD_LANDSCAPE_CLEAR>::Post(STR_ERROR_CAN_T_CLEAR_THIS_AREA, tile);
			} else {
				Command<CMD_BUILD_TRAIN_DEPOT>::Post(STR_ERROR_CAN_T_BUILD_TRAIN_DEPOT, tile, PickRailType(), _point_dir);
			}
			break;

		case MiniTool::RoadDepot:
			if (_drag_remove) {
				Command<CMD_LANDSCAPE_CLEAR>::Post(STR_ERROR_CAN_T_CLEAR_THIS_AREA, tile);
			} else {
				Command<CMD_BUILD_ROAD_DEPOT>::Post(STR_ERROR_CAN_T_BUILD_ROAD_DEPOT, tile, PickRoadType(), _point_dir);
			}
			break;

		case MiniTool::RailTunnel:
		case MiniTool::RoadTunnel: {
			bool rail = _tool == MiniTool::RailTunnel;
			if (_drag_remove) {
				Command<CMD_LANDSCAPE_CLEAR>::Post(STR_ERROR_CAN_T_CLEAR_THIS_AREA, tile);
			} else {
				Command<CMD_BUILD_TUNNEL>::Post(STR_ERROR_CAN_T_BUILD_TUNNEL_HERE, tile, rail ? TRANSPORT_RAIL : TRANSPORT_ROAD, rail ? (uint8_t)PickRailType() : (uint8_t)PickRoadType());
			}
			break;
		}

		case MiniTool::Signal: {
			Track track = PickSignalTrack(tile, _drag_ax, _drag_ay);
			if (track == INVALID_TRACK) break;
			if (_drag_remove) {
				Command<CMD_REMOVE_SINGLE_SIGNAL>::Post(STR_ERROR_CAN_T_REMOVE_SIGNALS_FROM, tile, track);
			} else {
				SignalVariant sigvar = TimerGameCalendar::year < _settings_client.gui.semaphore_build_before ? SIG_SEMAPHORE : SIG_ELECTRIC;
				Command<CMD_BUILD_SINGLE_SIGNAL>::Post(STR_ERROR_CAN_T_BUILD_SIGNALS_HERE, tile, track, _settings_client.gui.default_signal_type, sigvar, false, false, false, SIGTYPE_PBS, SIGTYPE_LAST, 0, 0);
			}
			break;
		}

		default:
			break;
	}
}

static void DrawPointToolPlan(int ppt)
{
	uint32_t c = _ctrl_pressed ? COL_BP_RM : COL_BP;
	double wx = MapXAt(_cursor.pos.y);
	double wy = MapYAt(_cursor.pos.x);
	int tx = Clamp<int>((int)std::floor(wx), 1, Map::SizeX() - 2);
	int ty = Clamp<int>((int)std::floor(wy), 1, Map::SizeY() - 2);
	int x0 = ScrX(ty);
	int y0 = ScrY(tx);
	int x1 = ScrX(ty + 1) - 1;
	int y1 = ScrY(tx + 1) - 1;
	BlendRect(x0, y0, x1, y1, c, 90);
	if (_ctrl_pressed) return;

	if (_tool == MiniTool::Signal) {
		Track track = PickSignalTrack(TileXY(tx, ty), wx, wy);
		if (track != INVALID_TRACK) DrawTrackPiece(track, x0, y0, x1, y1, std::max(2, ppt / 5), c);
	} else if (_tool == MiniTool::BusStop || _tool == MiniTool::TruckStop) {
		DrawAxisBand(DiagDirToAxis(_point_dir), x0, y0, x1, y1, std::max(2, ppt / 3), c);
	} else if (_tool == MiniTool::RailTunnel || _tool == MiniTool::RoadTunnel) {
		DiagDirection d = GetInclinedSlopeDirection(GetTileSlope(TileXY(tx, ty)));
		if (d != INVALID_DIAGDIR) {
			int cx = (x0 + x1) / 2;
			int cy = (y0 + y1) / 2;
			ThickLine(cx, cy, cx + _diag_dx[d] * (ppt / 2), cy + _diag_dy[d] * (ppt / 2), std::max(2, ppt / 5), c);
		}
	} else if (IsDirPointTool(_tool)) {
		int cx = (x0 + x1) / 2;
		int cy = (y0 + y1) / 2;
		ThickLine(cx, cy, cx + _diag_dx[_point_dir] * (ppt / 2), cy + _diag_dy[_point_dir] * (ppt / 2), std::max(2, ppt / 5), c);
	}
}

static void CommitRoadPlan()
{
	if (_road_plan.start == INVALID_TILE) return;
	if (_drag_remove) {
		Command<CMD_REMOVE_LONG_ROAD>::Post(STR_ERROR_CAN_T_REMOVE_ROAD_FROM, _road_plan.end, _road_plan.start, PickRoadType(), _road_plan.axis, false, false);
		ClearPlans();
		return;
	}

	MiniSpans spans = SplitWaterSpans(_road_plan.tiles);
	if (!spans.ok) {
		Command<CMD_BUILD_LONG_ROAD>::Post(STR_ERROR_CAN_T_BUILD_ROAD_HERE, _road_plan.end, _road_plan.start, PickRoadType(), _road_plan.axis, DRD_NONE, false, false, false);
	} else {
		const std::vector<TileIndex> &ts = _road_plan.tiles;
		for (auto [a, b] : spans.bridges) {
			Command<CMD_BUILD_BRIDGE>::Post(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE, ts[b], ts[a], TRANSPORT_ROAD, PickBridgeType((uint)(b - a - 1)), (uint8_t)PickRoadType());
		}
		for (auto [a, b] : spans.land) {
			Command<CMD_BUILD_LONG_ROAD>::Post(STR_ERROR_CAN_T_BUILD_ROAD_HERE, ts[b], ts[a], PickRoadType(), _road_plan.axis, DRD_NONE, false, false, false);
		}
	}
	ClearPlans();
}

/* Bottom-left build menu: a category bar with one panel of square icon tiles
 * above it, three per row. Drawn in screen space after Present(), so hit
 * rects live in screen pixels. */
struct MiniMenuItem {
	StringID str;
	std::string_view fallback;
	MiniTool tool;
};

struct MiniMenuCategory {
	StringID str;
	std::string_view fallback;
	MiniTool icon;
	std::span<const MiniMenuItem> items;
};

/* Labels come from the official language files so translations apply; the
 * fallback covers tools with no concise official string. */
static std::string MenuLabel(StringID str, std::string_view fallback)
{
	return str == INVALID_STRING_ID ? std::string(fallback) : StrMakeValid(GetString(str), {});
}

static const MiniMenuItem _menu_rail_items[] = {
	{STR_LAI_RAIL_DESCRIPTION_TRACK, "TRACK", MiniTool::Rail},
	{STR_LAI_STATION_DESCRIPTION_RAILROAD_STATION, "STATION", MiniTool::Station},
	{STR_COMPANY_INFRASTRUCTURE_VIEW_SIGNALS, "SIGNAL", MiniTool::Signal},
	{STR_LAI_RAIL_DESCRIPTION_TRAIN_DEPOT, "DEPOT", MiniTool::TrainDepot},
	{STR_LAI_TUNNEL_DESCRIPTION_RAILROAD, "TUNNEL", MiniTool::RailTunnel},
};

static const MiniMenuItem _menu_road_items[] = {
	{STR_LAI_ROAD_DESCRIPTION_ROAD, "ROAD", MiniTool::Road},
	{STR_LAI_STATION_DESCRIPTION_BUS_STATION, "BUS", MiniTool::BusStop},
	{STR_LAI_STATION_DESCRIPTION_TRUCK_LOADING_AREA, "TRUCK", MiniTool::TruckStop},
	{STR_LAI_ROAD_DESCRIPTION_ROAD_VEHICLE_DEPOT, "DEPOT", MiniTool::RoadDepot},
	{STR_LAI_TUNNEL_DESCRIPTION_ROAD, "TUNNEL", MiniTool::RoadTunnel},
};

/* Area-command tools live apart from construction: the bottom-right corner
 * is the command corner in the reference layout. */
static const MiniMenuItem _cmd_items[] = {
	{INVALID_STRING_ID, "LEVEL", MiniTool::Terraform},
	{INVALID_STRING_ID, "CLEAR", MiniTool::Demolish},
};

static const MiniMenuCategory _menu_cats[] = {
	{STR_RAIL_NAME_RAILROAD, "RAIL", MiniTool::Rail, _menu_rail_items},
	{STR_ROAD_NAME_ROAD, "ROAD", MiniTool::Road, _menu_road_items},
};

static int _menu_open = -1;
static int _menu_scroll = 0;
static Rect _menu_panel_rect;
static std::vector<std::pair<Rect, int>> _menu_cat_hits;
static std::vector<std::pair<Rect, MiniTool>> _menu_item_hits;

static bool InRect(const Rect &r, int x, int y)
{
	return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

/* Reference button cycle: dark tile, teal when active, cyan edge on hover. */
static void ChromeTile(const Rect &r, bool active)
{
	bool hover = _cursor.in_window && InRect(r, _cursor.pos.x, _cursor.pos.y);
	int rad = 2 * _ms.hud_scale;
	int b = hover ? 2 : 1;
	RlwCmdRoundRect(r.left, r.top, r.right, r.bottom, rad, hover ? COL_CH_ACCENT : COL_CH_EDGE);
	RlwCmdRoundRect(r.left + b, r.top + b, r.right - b, r.bottom - b, rad, active ? COL_CH_ACTIVE : COL_CH_TILE);
}

static int MenuTileSide()
{
	int lh = GetCharacterHeight(FS_NORMAL);
	int tw = 0;
	for (const MiniMenuCategory &c : _menu_cats) {
		tw = std::max<int>(tw, GetStringBoundingBox(MenuLabel(c.str, c.fallback)).width);
		for (const MiniMenuItem &it : c.items) tw = std::max<int>(tw, GetStringBoundingBox(MenuLabel(it.str, it.fallback)).width);
	}
	for (const MiniMenuItem &it : _cmd_items) tw = std::max<int>(tw, GetStringBoundingBox(MenuLabel(it.str, it.fallback)).width);
	return std::max(tw + 10, 3 * lh);
}

static std::string ToolLabel(MiniTool tool)
{
	for (const MiniMenuCategory &c : _menu_cats) {
		for (const MiniMenuItem &it : c.items) {
			if (it.tool == tool) return MenuLabel(it.str, it.fallback);
		}
	}
	for (const MiniMenuItem &it : _cmd_items) {
		if (it.tool == tool) return MenuLabel(it.str, it.fallback);
	}
	return std::string();
}

static void ScreenThickLine(int x0, int y0, int x1, int y1, int width, uint32_t c)
{
	ThickLine(x0, y0, x1, y1, width, c);
}

static void ScreenFillCircle(int cx, int cy, int r, uint32_t c)
{
	FillCircle(cx, cy, r, c);
}

/* Tile icons reuse the map's colour language so the menu previews what the
 * tool paints on the terrain. */
static void DrawToolIcon(MiniTool tool, int cx, int cy, int is)
{
	int h = is / 2;
	int t = std::max(2, is / 5);
	switch (tool) {
		case MiniTool::Rail:
			ScreenThickLine(cx - h, cy + h, cx + h, cy - h, t, COL_PAPER);
			break;
		case MiniTool::Road:
			ScreenFillRect(cx - h, cy - is / 4, cx + h, cy + is / 4, COL_ROAD);
			for (int i = -1; i <= 1; i++) ScreenFillRect(cx + i * (is / 3) - 1, cy - 1, cx + i * (is / 3) + 1, cy + 1, COL_PAPER);
			break;
		case MiniTool::Station:
			ScreenFillRect(cx - h, cy - h, cx + h, cy + h, COL_ST_RAIL_B);
			ScreenFillRect(cx - h + 2, cy - h + 2, cx + h - 2, cy + h - 2, COL_ST_RAIL);
			break;
		case MiniTool::BusStop:
			ScreenFillRect(cx - h, cy - h, cx + h, cy + h, COL_ST_ROAD_B);
			ScreenFillRect(cx - h + 2, cy - h + 2, cx + h - 2, cy + h - 2, COL_ST_ROAD);
			ScreenFillCircle(cx, cy, t, COL_PAPER);
			break;
		case MiniTool::TruckStop:
			ScreenFillRect(cx - h, cy - h, cx + h, cy + h, COL_ST_ROAD_B);
			ScreenFillRect(cx - h + 2, cy - h + 2, cx + h - 2, cy + h - 2, COL_ST_ROAD);
			ScreenFillRect(cx - t, cy - t, cx + t, cy + t, COL_PAPER);
			break;
		case MiniTool::TrainDepot:
		case MiniTool::RoadDepot:
			ScreenFillRect(cx - h, cy - h, cx + h, cy + h, COL_ROAD);
			ScreenFillRect(cx + h - 2, cy - is / 4, cx + h, cy + is / 4, COL_PAPER);
			break;
		case MiniTool::Signal:
			ScreenFillCircle(cx - is / 4, cy + is / 4, t, COL_STOP);
			ScreenFillCircle(cx + is / 4, cy - is / 4, t, COL_GO);
			break;
		case MiniTool::RailTunnel:
		case MiniTool::RoadTunnel:
			ScreenFillRect(cx - h, cy - h, cx + h, cy + h, tool == MiniTool::RailTunnel ? COL_PAPER : COL_BRIDGE);
			ScreenFillRect(cx - h + 2, cy - h + 2, cx + h - 2, cy + h - 2, COL_TUNNEL);
			break;
		case MiniTool::Terraform:
			ScreenFillRect(cx - h, cy + is / 6, cx + h, cy + h, _height_ramp[3]);
			ScreenFillRect(cx - h + is / 5, cy - is / 6, cx + h - is / 5, cy + is / 6, _height_ramp[6]);
			ScreenFillRect(cx - h + 2 * is / 5, cy - h, cx + h - 2 * is / 5, cy - is / 6, _height_ramp[9]);
			break;
		case MiniTool::Demolish:
			ScreenThickLine(cx - h, cy - h, cx + h, cy + h, t, COL_STOP);
			ScreenThickLine(cx - h, cy + h, cx + h, cy - h, t, COL_STOP);
			break;
		default:
			break;
	}
}

static void DrawMenuTile(const Rect &r, StringID str, std::string_view fallback, MiniTool icon, bool active)
{
	ChromeTile(r, active);
	int lh = GetCharacterHeight(FS_NORMAL);
	int cx = (r.left + r.right) / 2;
	int icon_h = r.bottom - r.top + 1 - lh - 9;
	DrawToolIcon(icon, cx, r.top + 3 + icon_h / 2, icon_h * 2 / 3);
	if (const MiniTextEntry *e = TextTexture(MenuLabel(str, fallback)); e != nullptr) {
		RlwCmdTexQuad(e->tex, cx - e->w / 2, r.bottom - lh - 3, active ? COL_CH_ACCENT : COL_CH_TEXT);
	}
}

static void DrawBuildMenu()
{
	_menu_cat_hits.clear();
	_menu_item_hits.clear();

	int s = _ms.hud_scale;
	int gap = 2 * s;
	int margin = 6 * s;
	int pp = 4 * s;
	int tile = MenuTileSide();

	int bar_top = _fbh - margin - tile;

	if (_menu_open >= 0) {
		const MiniMenuCategory &cat = _menu_cats[_menu_open];
		int n = (int)cat.items.size();
		int cols = 3;
		int rows = (n + cols - 1) / cols;
		int vis = _ms.menu_panel_rows;
		_menu_scroll = Clamp(_menu_scroll, 0, std::max(0, rows - vis));
		int pw = cols * tile + (cols - 1) * gap + 2 * pp;
		int ph = vis * tile + (vis - 1) * gap + 2 * pp;
		int py = bar_top - gap - ph;
		_menu_panel_rect = {margin, py, margin + pw - 1, py + ph - 1};
		ChromePanel(margin, py, margin + pw - 1, py + ph - 1);
		for (int i = 0; i < n; i++) {
			int row = i / cols - _menu_scroll;
			if (row < 0 || row >= vis) continue;
			const MiniMenuItem &it = cat.items[i];
			int ix = margin + pp + (i % cols) * (tile + gap);
			int iy = py + pp + row * (tile + gap);
			Rect ir = {ix, iy, ix + tile - 1, iy + tile - 1};
			DrawMenuTile(ir, it.str, it.fallback, it.tool, _tool == it.tool);
			_menu_item_hits.emplace_back(ir, it.tool);
		}
		if (rows > vis) {
			int track_top = py + pp;
			int track_h = ph - 2 * pp;
			int bx1 = margin + pw - 1 - s;
			int bx0 = std::max(margin, bx1 - s + 1);
			int ty0 = track_top + track_h * _menu_scroll / rows;
			int ty1 = track_top + track_h * (_menu_scroll + vis) / rows - 1;
			ScreenFillRect(bx0, ty0, bx1, ty1, COL_CH_DIM);
		}
	}

	for (int c = 0; c < (int)std::size(_menu_cats); c++) {
		const MiniMenuCategory &cat = _menu_cats[c];
		int x = margin + c * (tile + gap);
		Rect r = {x, bar_top, x + tile - 1, bar_top + tile - 1};
		DrawMenuTile(r, cat.str, cat.fallback, cat.icon, _menu_open == c);
		_menu_cat_hits.emplace_back(r, c);
	}
}

static bool HandleMenuClick(int x, int y)
{
	for (const auto &[r, c] : _menu_cat_hits) {
		if (InRect(r, x, y)) {
			_menu_open = _menu_open == c ? -1 : c;
			_menu_scroll = 0;
			return true;
		}
	}
	for (const auto &[r, t] : _menu_item_hits) {
		if (InRect(r, x, y)) {
			_tool = _tool == t ? MiniTool::None : t;
			return true;
		}
	}
	return false;
}

static std::vector<std::pair<Rect, MiniTool>> _cmd_hits;

static void DrawCmdBar()
{
	_cmd_hits.clear();

	int s = _ms.hud_scale;
	int gap = 2 * s;
	int margin = 6 * s;
	int tile = MenuTileSide();

	int n = (int)std::size(_cmd_items);
	int x0 = _fbw - margin - n * tile - (n - 1) * gap;
	int y = _fbh - margin - tile;
	for (int i = 0; i < n; i++) {
		const MiniMenuItem &it = _cmd_items[i];
		int x = x0 + i * (tile + gap);
		Rect r = {x, y, x + tile - 1, y + tile - 1};
		DrawMenuTile(r, it.str, it.fallback, it.tool, _tool == it.tool);
		_cmd_hits.emplace_back(r, it.tool);
	}
}

static bool HandleCmdClick(int x, int y)
{
	for (const auto &[r, t] : _cmd_hits) {
		if (InRect(r, x, y)) {
			_tool = _tool == t ? MiniTool::None : t;
			return true;
		}
	}
	return false;
}

/* Top-right window bar: category tiles whose panels open native status
 * windows. Same tile language as the build menu. */
enum class MiniWin : uint8_t {
	Finances,
	CompanyInfo,
	Goals,
	League,
	Graph,
	Stations,
	Trains,
	RoadVehicles,
	Ships,
	Aircraft,
	News,
	Towns,
	Industries,
	Subsidies,
};

struct MiniWinItem {
	StringID str;
	std::string_view fallback;
	MiniWin win;
};

struct MiniWinCategory {
	StringID str;
	std::string_view fallback;
	MiniWin icon;
	std::span<const MiniWinItem> items;
};

static const MiniWinItem _win_company_items[] = {
	{INVALID_STRING_ID, "FINANCES", MiniWin::Finances},
	{INVALID_STRING_ID, "INFO", MiniWin::CompanyInfo},
	{INVALID_STRING_ID, "GOALS", MiniWin::Goals},
	{STR_GRAPH_MENU_COMPANY_LEAGUE_TABLE, "LEAGUE", MiniWin::League},
	{STR_GRAPH_MENU_OPERATING_PROFIT_GRAPH, "GRAPH", MiniWin::Graph},
};

static const MiniWinItem _win_vehicle_items[] = {
	{INVALID_STRING_ID, "STATIONS", MiniWin::Stations},
	{STR_REPLACE_VEHICLE_TRAIN, "TRAIN", MiniWin::Trains},
	{STR_REPLACE_VEHICLE_ROAD_VEHICLE, "ROAD", MiniWin::RoadVehicles},
	{STR_REPLACE_VEHICLE_SHIP, "SHIP", MiniWin::Ships},
	{STR_REPLACE_VEHICLE_AIRCRAFT, "AIRCRAFT", MiniWin::Aircraft},
};

static const MiniWinItem _win_world_items[] = {
	{STR_NEWS_MENU_MESSAGE_HISTORY_MENU, "NEWS", MiniWin::News},
	{STR_TOWN_MENU_TOWN_DIRECTORY, "TOWNS", MiniWin::Towns},
	{STR_INDUSTRY_MENU_INDUSTRY_DIRECTORY, "INDUSTRY", MiniWin::Industries},
	{STR_SUBSIDIES_MENU_SUBSIDIES, "SUBSIDY", MiniWin::Subsidies},
};

static const MiniWinCategory _win_cats[] = {
	{STR_CONFIG_SETTING_COMPANY, "COMPANY", MiniWin::Finances, _win_company_items},
	{STR_CONFIG_SETTING_VEHICLES, "VEHICLES", MiniWin::Trains, _win_vehicle_items},
	{STR_CONFIG_SETTING_ENVIRONMENT, "WORLD", MiniWin::Towns, _win_world_items},
};

static int _win_open = -1;
static int _win_bar_bottom = 0;
static Rect _win_panel_rect;
static std::vector<std::pair<Rect, int>> _win_cat_hits;
static std::vector<std::pair<Rect, MiniWin>> _win_item_hits;
static std::vector<std::pair<Rect, MiniLayer>> _ovl_hits;

static int WinTileSide()
{
	int lh = GetCharacterHeight(FS_NORMAL);
	int tw = 0;
	for (const MiniWinCategory &c : _win_cats) {
		tw = std::max<int>(tw, GetStringBoundingBox(MenuLabel(c.str, c.fallback)).width);
		for (const MiniWinItem &it : c.items) tw = std::max<int>(tw, GetStringBoundingBox(MenuLabel(it.str, it.fallback)).width);
	}
	return std::max(tw + 10, 3 * lh);
}

static void DrawWinIcon(MiniWin win, int cx, int cy, int is)
{
	int h = is / 2;
	int t = std::max(2, is / 5);
	uint32_t cc = Company::IsValidID(_local_company) ? _company_rgb[_company_colours[_local_company]] : COL_OBJ;
	switch (win) {
		case MiniWin::Finances:
			ScreenFillCircle(cx, cy, h, COL_ST_BUOY);
			ScreenFillCircle(cx, cy, std::max(1, h - t), Darken(COL_ST_BUOY));
			break;
		case MiniWin::CompanyInfo:
			ScreenFillRect(cx - h, cy - h, cx - h + 1, cy + h, COL_PAPER);
			ScreenFillRect(cx - h + 2, cy - h, cx + h, cy, cc);
			break;
		case MiniWin::Goals:
			ScreenFillCircle(cx, cy, h, COL_STOP);
			ScreenFillCircle(cx, cy, std::max(2, h - t), COL_PAPER);
			ScreenFillCircle(cx, cy, std::max(1, h - 2 * t), COL_STOP);
			break;
		case MiniWin::League:
			ScreenFillRect(cx - h, cy, cx - h / 3 - 1, cy + h, COL_OBJ);
			ScreenFillRect(cx - h / 3 + 1, cy - h, cx + h / 3 - 1, cy + h, COL_ST_BUOY);
			ScreenFillRect(cx + h / 3 + 1, cy - h / 3, cx + h, cy + h, COL_OBJ);
			break;
		case MiniWin::Graph:
			ScreenThickLine(cx - h, cy + h, cx - h / 4, cy, t, COL_GO);
			ScreenThickLine(cx - h / 4, cy, cx + h / 4, cy + h / 3, t, COL_GO);
			ScreenThickLine(cx + h / 4, cy + h / 3, cx + h, cy - h, t, COL_GO);
			break;
		case MiniWin::Stations:
			ScreenFillRect(cx - h, cy - h, cx + h, cy + h, COL_ST_RAIL_B);
			ScreenFillRect(cx - h + 2, cy - h + 2, cx + h - 2, cy + h - 2, COL_ST_RAIL);
			break;
		case MiniWin::Trains:
			ScreenThickLine(cx - h, cy, cx + h - t, cy, t + 1, cc);
			ScreenFillCircle(cx + h - t, cy, t, COL_PAPER);
			break;
		case MiniWin::RoadVehicles:
			ScreenFillCircle(cx, cy, h, COL_INK);
			ScreenFillCircle(cx, cy, h - 1, cc);
			break;
		case MiniWin::Ships:
			FillDiamond(cx, cy, h, COL_INK);
			FillDiamond(cx, cy, h - 1, cc);
			break;
		case MiniWin::Aircraft:
			FillTriangle(cx, cy, h, COL_INK);
			FillTriangle(cx, cy, h - 1, cc);
			break;
		case MiniWin::News:
			ScreenFillRect(cx - h, cy - h, cx + h, cy + h, COL_PAPER);
			for (int i = -1; i <= 1; i++) ScreenFillRect(cx - h + 2, cy + i * (is / 4) - 1, cx + h - 2, cy + i * (is / 4), COL_INK);
			break;
		case MiniWin::Towns:
			ScreenFillRect(cx - h, cy - is / 6, cx - 1, cy + h, COL_HOUSE_B);
			ScreenFillRect(cx - h + 1, cy - is / 6 + 1, cx - 2, cy + h - 1, COL_HOUSE);
			ScreenFillRect(cx + 1, cy - h, cx + h, cy + h, COL_HOUSE_B);
			ScreenFillRect(cx + 2, cy - h + 1, cx + h - 1, cy + h - 1, COL_HOUSE);
			break;
		case MiniWin::Industries:
			ScreenFillRect(cx - h, cy - is / 6, cx + h, cy + h, COL_IND_B);
			ScreenFillRect(cx - h + 2, cy - is / 6 + 2, cx + h - 2, cy + h - 2, COL_IND);
			ScreenFillRect(cx + h / 4, cy - h, cx + h / 4 + t, cy - is / 6, COL_IND_B);
			break;
		case MiniWin::Subsidies:
			ScreenFillCircle(cx - h + t, cy + h - t, t, COL_HOUSE);
			ScreenFillCircle(cx + h - t, cy - h + t, t, COL_IND);
			ScreenThickLine(cx - h + 2 * t, cy + h - 2 * t, cx + h - 2 * t, cy - h + 2 * t, std::max(2, t - 1), COL_PAPER);
			break;
	}
}

static void DrawWinTile(const Rect &r, StringID str, std::string_view fallback, MiniWin icon, bool active)
{
	ChromeTile(r, active);
	int lh = GetCharacterHeight(FS_NORMAL);
	int cx = (r.left + r.right) / 2;
	int icon_h = r.bottom - r.top + 1 - lh - 9;
	DrawWinIcon(icon, cx, r.top + 3 + icon_h / 2, icon_h * 2 / 3);
	if (const MiniTextEntry *e = TextTexture(MenuLabel(str, fallback)); e != nullptr) {
		RlwCmdTexQuad(e->tex, cx - e->w / 2, r.bottom - lh - 3, active ? COL_CH_ACCENT : COL_CH_TEXT);
	}
}

static void OpenMiniWindow(MiniWin win)
{
	bool company = Company::IsValidID(_local_company);
	switch (win) {
		case MiniWin::Finances: if (company) ShowCompanyFinances(_local_company); break;
		case MiniWin::CompanyInfo: if (company) ShowCompany(_local_company); break;
		case MiniWin::Goals: if (company) ShowGoalsList(_local_company); break;
		case MiniWin::League: ShowPerformanceLeagueTable(); break;
		case MiniWin::Graph: ShowOperatingProfitGraph(); break;
		case MiniWin::Stations: if (company) ShowCompanyStations(_local_company); break;
		case MiniWin::Trains: if (company) ShowVehicleListWindow(_local_company, VEH_TRAIN); break;
		case MiniWin::RoadVehicles: if (company) ShowVehicleListWindow(_local_company, VEH_ROAD); break;
		case MiniWin::Ships: if (company) ShowVehicleListWindow(_local_company, VEH_SHIP); break;
		case MiniWin::Aircraft: if (company) ShowVehicleListWindow(_local_company, VEH_AIRCRAFT); break;
		case MiniWin::News: ShowMessageHistory(); break;
		case MiniWin::Towns: ShowTownDirectory(); break;
		case MiniWin::Industries: ShowIndustryDirectory(); break;
		case MiniWin::Subsidies: ShowSubsidiesList(); break;
	}
}

static void DrawOverlayGlyph(MiniLayer layer, int cx, int cy, int os, bool active)
{
	uint32_t g = active ? COL_CH_ACCENT : COL_CH_TEXT;
	uint32_t bg = active ? COL_CH_ACTIVE : COL_CH_TILE;
	int gl = os / 2 - 2;
	if (layer == MiniLayer::Rail) {
		ScreenThickLine(cx - gl, cy, cx + gl, cy, std::max(2, os / 8), g);
		int th = std::max(2, os / 6);
		for (int i = -1; i <= 1; i++) {
			int px = cx + i * (gl - 1);
			ScreenFillRect(px, cy - th, px + 1, cy + th, g);
		}
	} else {
		int bh = std::max(2, os / 5);
		ScreenFillRect(cx - gl, cy - bh, cx + gl, cy + bh, g);
		for (int i = -1; i <= 1; i++) {
			int px = cx + i * (gl - 1);
			ScreenFillRect(px - 1, cy, px + 1, cy, bg);
		}
	}
}

static void DrawWinBar()
{
	_win_cat_hits.clear();
	_win_item_hits.clear();
	_ovl_hits.clear();

	int s = _ms.hud_scale;
	int gap = 2 * s;
	int margin = 6 * s;
	int pp = 4 * s;
	int tile = WinTileSide();

	int ncats = (int)std::size(_win_cats);
	int bar_left = _fbw - margin - ncats * tile - (ncats - 1) * gap;
	_win_bar_bottom = margin + tile - 1;

	for (int c = 0; c < ncats; c++) {
		const MiniWinCategory &cat = _win_cats[c];
		int x = bar_left + c * (tile + gap);
		Rect r = {x, margin, x + tile - 1, margin + tile - 1};
		DrawWinTile(r, cat.str, cat.fallback, cat.icon, _win_open == c);
		_win_cat_hits.emplace_back(r, c);
	}

	/* Second row under the window bar: the overlay strip. Compact icon
	 * toggles, same click cycle as the reference: re-click clears, another
	 * button switches. */
	if (_ms.filter_alpha > 0) {
		static const MiniLayer layers[] = {MiniLayer::Rail, MiniLayer::Road};
		int os = 12 * s;
		int op = 2 * s;
		int n = (int)std::size(layers);
		int sw = n * os + (n - 1) * gap + 2 * op;
		int sh = os + 2 * op;
		int sx = _fbw - margin - sw;
		int sy = _win_bar_bottom + 1 + gap;
		ChromePanel(sx, sy, sx + sw - 1, sy + sh - 1);
		for (int i = 0; i < n; i++) {
			int x = sx + op + i * (os + gap);
			Rect r = {x, sy + op, x + os - 1, sy + op + os - 1};
			bool active = _overlay == layers[i];
			ChromeTile(r, active);
			DrawOverlayGlyph(layers[i], (r.left + r.right) / 2, (r.top + r.bottom) / 2, os, active);
			_ovl_hits.emplace_back(r, layers[i]);
		}
		_win_bar_bottom = sy + sh - 1;
	}

	if (_win_open >= 0) {
		const MiniWinCategory &cat = _win_cats[_win_open];
		int n = (int)cat.items.size();
		int cols = 3;
		int rows = (n + cols - 1) / cols;
		int pw = cols * tile + (cols - 1) * gap + 2 * pp;
		int ph = rows * tile + (rows - 1) * gap + 2 * pp;
		int px = _fbw - margin - pw;
		int py = _win_bar_bottom + 1 + gap;
		_win_panel_rect = {px, py, px + pw - 1, py + ph - 1};
		ChromePanel(px, py, px + pw - 1, py + ph - 1);
		for (int i = 0; i < n; i++) {
			const MiniWinItem &it = cat.items[i];
			int ix = px + pp + (i % cols) * (tile + gap);
			int iy = py + pp + (i / cols) * (tile + gap);
			Rect ir = {ix, iy, ix + tile - 1, iy + tile - 1};
			DrawWinTile(ir, it.str, it.fallback, it.win, false);
			_win_item_hits.emplace_back(ir, it.win);
		}
	}
}

static bool HandleWinClick(int x, int y)
{
	for (const auto &[r, l] : _ovl_hits) {
		if (InRect(r, x, y)) {
			_overlay = _overlay == l ? MiniLayer::None : l;
			_overlay_auto = false;
			return true;
		}
	}
	for (const auto &[r, c] : _win_cat_hits) {
		if (InRect(r, x, y)) {
			_win_open = _win_open == c ? -1 : c;
			return true;
		}
	}
	if (_win_open >= 0) {
		for (const auto &[r, w] : _win_item_hits) {
			if (InRect(r, x, y)) {
				OpenMiniWindow(w);
				_win_open = -1;
				return true;
			}
		}
		if (InRect(_win_panel_rect, x, y)) return true;
	}
	return false;
}

/* Top-left status corner in the reference layout: a panel docked flush to
 * the corner carrying the year gauge, company identity, money and the time
 * controls, with the selected-vehicle readout hanging below it. */
static std::vector<std::pair<Rect, int>> _speed_hits;
static int _colony_bottom = 0;
static const uint32_t COL_CH_GAUGE = 0xFFE8B94DU;

static void DrawPlayTriangle(int x0, int cy, int w, int hh, uint32_t c)
{
	for (int i = 0; i < w; i++) {
		int h = hh * (w - i) / w;
		if (h <= 0) break;
		ScreenFillRect(x0 + i, cy - h, x0 + i, cy + h, c);
	}
}

static void DrawColonyPanel()
{
	_speed_hits.clear();

	int s = _ms.hud_scale;
	int lh = GetCharacterHeight(FS_NORMAL);
	int pad = 5 * s;
	int gap = 2 * s;
	int ring = lh;

	const Company *c = Company::GetIfValid(_local_company);
	std::string name = c != nullptr ? StrMakeValid(GetString(STR_COMPANY_NAME, c->index), {}) : std::string();
	std::string date = GetString(STR_JUST_DATE_LONG, TimerGameCalendar::date);
	std::string funds;
	if (c != nullptr) {
		uint veh = 0;
		for (int t = 0; t < VEH_COMPANY_END; t++) veh += c->group_all[t].num_vehicle;
		funds = fmt::format("{}   VEH {}", GetString(STR_JUST_CURRENCY_LONG, c->money), veh);
	}

	int bw = 12 * s, bh = 10 * s;
	int text_x = pad + 2 * ring + 4 * s;
	int w = std::max({text_x + std::max<int>(GetStringBoundingBox(name).width, GetStringBoundingBox(date).width),
			pad + (int)GetStringBoundingBox(funds).width,
			pad + 3 * bw + 2 * gap}) + pad;
	/* Text widths shift every tick; quantised and monotonic width keeps the
	 * panel from breathing. */
	w = (w + 8 * s - 1) / (8 * s) * (8 * s);
	static int stable_w = 0;
	w = std::max(w, stable_w);
	stable_w = w;
	int rows_bottom = pad + std::max(2 * ring, 2 * lh + gap);
	int funds_y = rows_bottom + 2 * s;
	int btn_y = funds.empty() ? funds_y : funds_y + lh + 3 * s;
	int h = btn_y + bh + pad;

	int m = 6 * s;
	ChromePanel(m, m, m + w - 1, m + h - 1);

	/* Year gauge: segmented ring, elapsed part in the reference cycle gold. */
	TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::date);
	double frac = (ymd.month + (ymd.day - 1) / 31.0) / 12.0;
	int cx = m + pad + ring, cy = m + pad + ring;
	const int N = 28;
	const double TAU = 6.283185307179586;
	int rr = ring - s;
	for (int i = 0; i < N; i++) {
		double a0 = -TAU / 4 + TAU * i / N;
		double a1 = -TAU / 4 + TAU * (i + 1) / N;
		uint32_t col = ((double)i + 0.5) / N < frac ? COL_CH_GAUGE : COL_CH_TILE;
		RlwCmdLine(cx + (int)std::lround(cos(a0) * rr), cy + (int)std::lround(sin(a0) * rr),
				cx + (int)std::lround(cos(a1) * rr), cy + (int)std::lround(sin(a1) * rr), 2 * s, col);
	}

	if (!name.empty()) DrawScreenText(m + text_x, m + pad, name);
	DrawScreenText(m + text_x, m + pad + lh + gap, date);
	if (!funds.empty()) DrawScreenText(m + pad, m + funds_y, funds);

	int active = _pause_mode.Any() ? 0 : (_game_speed == 100 ? 1 : 2);
	for (int i = 0; i < 3; i++) {
		int bx = m + pad + i * (bw + gap);
		Rect r = {bx, m + btn_y, bx + bw - 1, m + btn_y + bh - 1};
		ChromeTile(r, active == i);
		uint32_t gc = active == i ? COL_CH_ACCENT : (i == 2 && _networking ? COL_CH_DIM : COL_CH_TEXT);
		int gcx = (r.left + r.right) / 2, gcy = (r.top + r.bottom) / 2;
		int gh = std::max(2, (bh - 4 * s) / 2);
		switch (i) {
			case 0:
				ScreenFillRect(gcx - 2 * s, gcy - gh, gcx - s, gcy + gh, gc);
				ScreenFillRect(gcx + s, gcy - gh, gcx + 2 * s, gcy + gh, gc);
				break;
			case 1:
				DrawPlayTriangle(gcx - (gh + 2 * s) / 2, gcy, gh + 2 * s, gh, gc);
				break;
			case 2:
				DrawPlayTriangle(gcx - (gh + s), gcy, gh + s, gh, gc);
				DrawPlayTriangle(gcx, gcy, gh + s, gh, gc);
				break;
		}
		_speed_hits.emplace_back(r, i);
	}
	_colony_bottom = m + h;
}

static bool HandleSpeedClick(int x, int y)
{
	for (const auto &[r, i] : _speed_hits) {
		if (!InRect(r, x, y)) continue;
		switch (i) {
			case 0:
				Command<CMD_PAUSE>::Post(PauseMode::Normal, !_pause_mode.Test(PauseMode::Normal));
				break;
			case 1:
				if (_pause_mode.Test(PauseMode::Normal)) Command<CMD_PAUSE>::Post(PauseMode::Normal, false);
				ChangeGameSpeed(false);
				break;
			case 2:
				if (_networking) break;
				if (_pause_mode.Test(PauseMode::Normal)) Command<CMD_PAUSE>::Post(PauseMode::Normal, false);
				ChangeGameSpeed(true);
				break;
		}
		return true;
	}
	return false;
}

/* Left-edge status stream like the reference: no event cards, only live
 * aggregated problem states. A row shows the category and count, hovering
 * lists the affected vehicles, clicking cycles the camera through them. */
enum class MiniStatus : uint8_t {
	Crashed,
	Lost,
	Broken,
	NoOrders,
	OldAge,
	Unprofitable,
	End,
};

static const int MINI_STATUS_COUNT = (int)MiniStatus::End;
static std::vector<VehicleID> _status_veh[MINI_STATUS_COUNT];
static std::vector<std::pair<Rect, int>> _status_rows;
static uint32_t _status_cursor[MINI_STATUS_COUNT];
static std::unordered_map<std::string, std::string> _news_trunc;

static std::string StatusLabel(int st)
{
	switch ((MiniStatus)st) {
		case MiniStatus::Crashed: return MenuLabel(STR_VEHICLE_STATUS_CRASHED, "CRASHED");
		case MiniStatus::Lost: return MenuLabel(INVALID_STRING_ID, "LOST");
		case MiniStatus::Broken: return MenuLabel(STR_VEHICLE_STATUS_BROKEN_DOWN, "BROKEN DOWN");
		case MiniStatus::NoOrders: return MenuLabel(INVALID_STRING_ID, "NO ORDERS");
		case MiniStatus::OldAge: return MenuLabel(INVALID_STRING_ID, "OLD AGE");
		default: return MenuLabel(INVALID_STRING_ID, "IN THE RED");
	}
}

static void ScanStatuses()
{
	for (auto &l : _status_veh) l.clear();
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->type > VEH_AIRCRAFT || !v->IsPrimaryVehicle() || v->owner != _local_company) continue;
		if (v->vehstatus.Test(VehState::Crashed)) {
			_status_veh[(int)MiniStatus::Crashed].push_back(v->index);
			continue;
		}
		if (v->vehicle_flags.Test(VehicleFlag::PathfinderLost)) _status_veh[(int)MiniStatus::Lost].push_back(v->index);
		if (v->type != VEH_AIRCRAFT && v->breakdown_ctr == 1) _status_veh[(int)MiniStatus::Broken].push_back(v->index);
		if (v->GetNumOrders() == 0 && !v->vehstatus.Test(VehState::Stopped)) _status_veh[(int)MiniStatus::NoOrders].push_back(v->index);
		if (v->age > v->max_age) _status_veh[(int)MiniStatus::OldAge].push_back(v->index);
		if (v->economy_age >= VEHICLE_PROFIT_MIN_AGE && v->GetDisplayProfitLastYear() < 0) _status_veh[(int)MiniStatus::Unprofitable].push_back(v->index);
	}
}

static std::string_view TruncateText(const std::string &text, int maxw)
{
	if ((int)GetStringBoundingBox(text).width <= maxw) return text;
	auto it = _news_trunc.find(text);
	if (it == _news_trunc.end()) {
		if (_news_trunc.size() > 128) _news_trunc.clear();
		std::string best;
		Utf8View view(text);
		for (auto vit = view.begin(); vit != view.end();) {
			++vit;
			std::string cand = text.substr(0, vit.GetByteOffset()) + "...";
			if ((int)GetStringBoundingBox(cand).width > maxw) break;
			best = std::move(cand);
		}
		it = _news_trunc.emplace(text, std::move(best)).first;
	}
	return it->second;
}

static void DrawStatusStream()
{
	ScanStatuses();
	_status_rows.clear();

	int s = _ms.hud_scale;
	int lh = GetCharacterHeight(FS_NORMAL);
	int margin = 6 * s;
	int bar_w = 3 * s;
	int card_w = 84 * s;
	int maxw = card_w - bar_w - 8 * s;
	int y = _colony_bottom + margin;

	int hover = -1;
	Rect hover_r{};
	for (int st = 0; st < MINI_STATUS_COUNT; st++) {
		const auto &list = _status_veh[st];
		if (list.empty()) continue;

		bool crit = st <= (int)MiniStatus::Broken;
		uint32_t bar = crit ? 0xFFE05F4AU : 0xFFE0B64AU;
		uint32_t bg = crit ? 0xFF4A2320U : 0xFF453A1EU;
		uint32_t tcol = crit ? 0xFFF2D9D2U : 0xFFEBD9A8U;

		std::string text = fmt::format("{} ({})", StatusLabel(st), list.size());
		std::string_view t = TruncateText(text, maxw);
		if (t.empty()) continue;
		int ch = lh + 5 * s;
		Rect r = {margin, y, margin + card_w - 1, y + ch - 1};
		ScreenFillRect(r.left, r.top, r.left + bar_w - 1, r.bottom, bar);
		ScreenFillRect(r.left + bar_w, r.top, r.right, r.bottom, bg);
		if (_cursor.in_window && InRect(r, _cursor.pos.x, _cursor.pos.y)) {
			BlendRect(r.left, r.top, r.right, r.bottom, COL_PAPER, 28);
			hover = st;
			hover_r = r;
		}
		if (const MiniTextEntry *e = TextTexture(t); e != nullptr) RlwCmdTexQuad(e->tex, r.left + bar_w + 4 * s, y + (ch - lh) / 2, tcol);
		_status_rows.push_back({r, st});
		y += ch + 2 * s;
	}

	/* Hover panel: the affected vehicles by name, capped at eight. */
	if (hover >= 0) {
		const auto &list = _status_veh[hover];
		static std::vector<std::string> names;
		names.clear();
		int wmax = 0;
		for (size_t i = 0; i < list.size() && i < 8; i++) {
			const Vehicle *v = Vehicle::GetIfValid(list[i]);
			if (v == nullptr) continue;
			names.push_back(StrMakeValid(GetString(STR_VEHICLE_NAME, v->index), {}));
			wmax = std::max(wmax, (int)GetStringBoundingBox(names.back()).width);
		}
		if (list.size() > names.size()) {
			names.push_back(fmt::format("+{}", list.size() - names.size()));
			wmax = std::max(wmax, (int)GetStringBoundingBox(names.back()).width);
		}
		if (!names.empty()) {
			int pad = 4 * s;
			int x0 = hover_r.right + 4 * s;
			int y0 = hover_r.top;
			int w = wmax + 2 * pad;
			int h = (int)names.size() * (lh + 2 * s) + 2 * pad - 2 * s;
			if (y0 + h >= _fbh) y0 = std::max(0, _fbh - h - 1);
			ChromePanel(x0, y0, x0 + w - 1, y0 + h - 1);
			int ty = y0 + pad;
			for (const std::string &n : names) {
				DrawScreenText(x0 + pad, ty, n);
				ty += lh + 2 * s;
			}
		}
	}
}

static bool HandleStatusClick(int x, int y)
{
	for (const auto &[r, st] : _status_rows) {
		if (!InRect(r, x, y)) continue;
		const auto &list = _status_veh[st];
		for (size_t i = 0; i < list.size(); i++) {
			size_t idx = _status_cursor[st] % list.size();
			_status_cursor[st]++;
			const Vehicle *v = Vehicle::GetIfValid(list[idx]);
			if (v == nullptr) continue;
			_sel_vehicle = v->index;
			MiniUiScrollTo(v->x_pos, v->y_pos);
			return true;
		}
		return true;
	}
	return false;
}

/* Right-edge vehicle screen like the reference side screens: info rows, the
 * order list and command tiles for the selected vehicle. Map clicks only
 * select; the window tile still opens the native vehicle window. */
static std::vector<std::pair<Rect, int>> _veh_btn_hits;
static std::vector<std::pair<Rect, int>> _veh_order_hits;
static Rect _veh_panel_rect;
static bool _veh_panel_shown = false;

enum {
	VEH_BTN_STARTSTOP,
	VEH_BTN_DEPOT,
	VEH_BTN_FOLLOW,
	VEH_BTN_NATIVE,
};

static std::string VehOrderLabel(const Order &o)
{
	switch (o.GetType()) {
		case OT_GOTO_STATION: return StrMakeValid(GetString(STR_STATION_NAME, o.GetDestination().ToStationID()), {});
		case OT_GOTO_WAYPOINT: return StrMakeValid(GetString(STR_WAYPOINT_NAME, o.GetDestination().ToStationID()), {});
		case OT_GOTO_DEPOT: return MenuLabel(INVALID_STRING_ID, "DEPOT");
		case OT_CONDITIONAL: return fmt::format("IF > {}", o.GetConditionSkipToOrder() + 1);
		default: return std::string();
	}
}

static void DrawVehPanel()
{
	_veh_btn_hits.clear();
	_veh_order_hits.clear();
	_veh_panel_shown = false;

	const Vehicle *v = Vehicle::GetIfValid(_sel_vehicle);
	if (v == nullptr || !v->IsPrimaryVehicle()) return;

	int s = _ms.hud_scale;
	int lh = GetCharacterHeight(FS_NORMAL);
	int pad = 5 * s;
	int gap = 2 * s;
	int w = 110 * s;
	int maxw = w - 2 * pad;
	int row_h = lh + gap;

	std::string title = StrMakeValid(GetString(STR_VEHICLE_NAME, v->index), {});

	/* Info rows: text, tint and an optional legend dot colour. */
	static std::vector<std::tuple<std::string, uint32_t, uint32_t>> rows;
	rows.clear();
	rows.emplace_back(fmt::format("{} {}", StrMakeValid(GetString((StringID)(STR_REPLACE_VEHICLE_TRAIN + v->type)), {}), v->unitnumber), COL_CH_DIM, 0);
	if (v->vehstatus.Test(VehState::Crashed)) {
		rows.emplace_back(StrMakeValid(GetString(STR_VEHICLE_STATUS_CRASHED), {}), 0xFFE05F4AU, 0);
	} else if (v->vehstatus.Test(VehState::Stopped)) {
		rows.emplace_back(StrMakeValid(GetString(STR_VEHICLE_STATUS_STOPPED), {}), 0xFFE05F4AU, 0);
	} else {
		rows.emplace_back(fmt::format("SPD {} / {}", v->GetDisplaySpeed(), v->GetDisplayMaxSpeed()), COL_CH_TEXT, 0);
	}
	rows.emplace_back(fmt::format("PROFIT {} / {}", GetString(STR_JUST_CURRENCY_LONG, v->GetDisplayProfitThisYear()), GetString(STR_JUST_CURRENCY_LONG, v->GetDisplayProfitLastYear())), COL_CH_TEXT, 0);
	rows.emplace_back(fmt::format("AGE {}Y / {}Y   REL {}%", v->age.base() / 366, v->max_age.base() / 366, v->reliability * 100 >> 16), COL_CH_TEXT, 0);

	/* Cargo totals across the consist, one row per cargo kind. */
	static std::vector<std::tuple<CargoType, uint, uint>> cargo;
	cargo.clear();
	for (const Vehicle *u = v; u != nullptr; u = u->Next()) {
		if (u->cargo_cap == 0 || !IsValidCargoType(u->cargo_type)) continue;
		auto it = std::find_if(cargo.begin(), cargo.end(), [&](const auto &e) { return std::get<0>(e) == u->cargo_type; });
		if (it == cargo.end()) it = cargo.emplace(cargo.end(), u->cargo_type, 0, 0);
		std::get<1>(*it) += u->cargo_cap;
		std::get<2>(*it) += u->cargo.StoredCount();
	}
	for (auto &[ct, cap, stored] : cargo) {
		uint32_t l = CargoSpec::Get(ct)->label.base();
		char lab[4] = {(char)(l >> 24), (char)(l >> 16), (char)(l >> 8), (char)l};
		rows.emplace_back(fmt::format("{} {}/{}", std::string_view(lab, 4), stored, cap), COL_CH_TEXT, CargoRgb(ct));
	}

	/* Order rows keep their real order index for the skip command. */
	static std::vector<std::tuple<int, std::string, bool>> orders;
	orders.clear();
	int oi = 0;
	for (const Order &o : v->Orders()) {
		std::string label = VehOrderLabel(o);
		if (!label.empty()) orders.emplace_back(oi, std::move(label), oi == v->cur_real_order_index);
		oi++;
	}

	int bw = 12 * s, bh = 10 * s;
	int x1 = _fbw - 1 - 6 * s;
	int x0 = x1 - w + 1;
	int y0 = _win_bar_bottom + 6 * s;

	int fixed = pad + lh + 3 * s + (int)rows.size() * row_h + 3 * s + (orders.empty() ? 0 : lh + gap) + 4 * s + bh + pad;
	int limit = _fbh - MenuTileSide() - 18 * s;
	int max_orders = std::max(0, (limit - y0 - fixed) / row_h);
	bool more = (int)orders.size() > max_orders;
	int shown = more ? std::max(0, max_orders - 1) : (int)orders.size();

	int h = fixed + (shown + (more ? 1 : 0)) * row_h;
	ChromePanel(x0, y0, x0 + w - 1, y0 + h - 1);
	_veh_panel_rect = {x0, y0, x0 + w - 1, y0 + h - 1};
	_veh_panel_shown = true;

	int y = y0 + pad;
	DrawScreenText(x0 + pad, y, TruncateText(title, maxw));
	y += lh + s;
	ScreenFillRect(x0 + pad, y, x0 + w - 1 - pad, y + s - 1, COL_CH_ACCENT);
	y += 2 * s;

	for (const auto &[text, tint, dot] : rows) {
		int tx = x0 + pad;
		if (dot != 0) {
			int r = std::max(2, lh / 4);
			FillCircle(tx + r, y + lh / 2, r, dot);
			tx += 2 * r + 3 * s;
		}
		if (const MiniTextEntry *e = TextTexture(TruncateText(text, maxw - (tx - x0 - pad))); e != nullptr) RlwCmdTexQuad(e->tex, tx, y, tint);
		y += row_h;
	}
	y += 3 * s;

	if (!orders.empty()) {
		if (const MiniTextEntry *e = TextTexture(MenuLabel(INVALID_STRING_ID, "ORDERS")); e != nullptr) RlwCmdTexQuad(e->tex, x0 + pad, y, COL_CH_DIM);
		y += lh + gap;
		for (int n = 0; n < shown; n++) {
			const auto &[idx, label, cur] = orders[n];
			Rect r = {x0 + pad, y, x0 + w - 1 - pad, y + row_h - 1};
			if (_cursor.in_window && InRect(r, _cursor.pos.x, _cursor.pos.y)) BlendRect(r.left, r.top, r.right, r.bottom, COL_PAPER, 28);
			if (cur) ScreenFillRect(r.left, r.top, r.left + 2 * s - 1, r.bottom, COL_CH_ACCENT);
			std::string line = fmt::format("{}. {}", idx + 1, label);
			if (const MiniTextEntry *e = TextTexture(TruncateText(line, maxw - 4 * s)); e != nullptr) RlwCmdTexQuad(e->tex, r.left + 4 * s, y, cur ? COL_CH_ACCENT : COL_CH_TEXT);
			_veh_order_hits.push_back({r, idx});
			y += row_h;
		}
		if (more) {
			if (const MiniTextEntry *e = TextTexture(fmt::format("+{}", orders.size() - shown)); e != nullptr) RlwCmdTexQuad(e->tex, x0 + pad + 4 * s, y, COL_CH_DIM);
			y += row_h;
		}
	}
	y += 4 * s;

	bool stopped = v->vehstatus.Test(VehState::Stopped);
	for (int i = 0; i < 4; i++) {
		int bx = x0 + pad + i * (bw + gap);
		Rect r = {bx, y, bx + bw - 1, y + bh - 1};
		ChromeTile(r, i == VEH_BTN_FOLLOW && _follow);
		uint32_t gc = (i == VEH_BTN_FOLLOW && _follow) ? COL_CH_ACCENT : COL_CH_TEXT;
		int gcx = (r.left + r.right) / 2, gcy = (r.top + r.bottom) / 2;
		int gh = std::max(2, (bh - 4 * s) / 2);
		switch (i) {
			case VEH_BTN_STARTSTOP:
				if (stopped) {
					DrawPlayTriangle(gcx - (gh + 2 * s) / 2, gcy, gh + 2 * s, gh, gc);
				} else {
					ScreenFillRect(gcx - gh + 1, gcy - gh + 1, gcx + gh - 1, gcy + gh - 1, gc);
				}
				break;
			case VEH_BTN_DEPOT:
				for (int j = 0; j < gh; j++) ScreenFillRect(gcx - (gh - j), gcy - gh + j, gcx + (gh - j), gcy - gh + j, gc);
				ScreenFillRect(gcx - gh, gcy + 1, gcx + gh, gcy + std::max(1, s), gc);
				break;
			case VEH_BTN_FOLLOW:
				ScreenFillRect(gcx - s, gcy - s, gcx + s, gcy + s, gc);
				ScreenFillRect(gcx - gh - s, gcy, gcx - gh + s, gcy, gc);
				ScreenFillRect(gcx + gh - s, gcy, gcx + gh + s, gcy, gc);
				ScreenFillRect(gcx, gcy - gh - s, gcx, gcy - gh + s, gc);
				ScreenFillRect(gcx, gcy + gh - s, gcx, gcy + gh + s, gc);
				break;
			case VEH_BTN_NATIVE:
				ScreenFillRect(gcx - gh, gcy - gh, gcx + gh, gcy - gh + std::max(1, s), gc);
				ScreenFillRect(gcx - gh, gcy - gh, gcx - gh + std::max(1, s) - 1, gcy + gh, gc);
				ScreenFillRect(gcx + gh - std::max(1, s) + 1, gcy - gh, gcx + gh, gcy + gh, gc);
				ScreenFillRect(gcx - gh, gcy + gh - std::max(1, s) + 1, gcx + gh, gcy + gh, gc);
				break;
		}
		_veh_btn_hits.push_back({r, i});
	}
}

static bool HandleVehPanelClick(int x, int y)
{
	if (!_veh_panel_shown || !InRect(_veh_panel_rect, x, y)) return false;
	const Vehicle *v = Vehicle::GetIfValid(_sel_vehicle);
	if (v == nullptr) return true;

	for (const auto &[r, i] : _veh_btn_hits) {
		if (!InRect(r, x, y)) continue;
		switch (i) {
			case VEH_BTN_STARTSTOP:
				Command<CMD_START_STOP_VEHICLE>::Post(STR_ERROR_CAN_T_STOP_START_TRAIN + v->type, v->tile, v->index, false);
				break;
			case VEH_BTN_DEPOT:
				Command<CMD_SEND_VEHICLE_TO_DEPOT>::Post(GetCmdSendToDepotMsg(v), v->index, _ctrl_pressed ? DepotCommandFlag::Service : DepotCommandFlags{}, {});
				break;
			case VEH_BTN_FOLLOW:
				_follow = !_follow;
				if (_follow) {
					_glide = false;
					_zoom_anchored = false;
					_dest_ppt = MAX_PPT;
				}
				break;
			case VEH_BTN_NATIVE:
				ShowVehicleViewWindow(v);
				break;
		}
		return true;
	}
	for (const auto &[r, idx] : _veh_order_hits) {
		if (!InRect(r, x, y)) continue;
		Command<CMD_SKIP_TO_ORDER>::Post(STR_ERROR_CAN_T_SKIP_TO_ORDER, v->tile, v->index, (VehicleOrderID)idx);
		return true;
	}
	return true;
}

static void DrawHud()
{
	int s = _ms.hud_scale;
	int lh = GetCharacterHeight(FS_NORMAL);

	if (_pause_mode.Any()) DrawHudTextCentred(_fbw / 2, 6 * s, StrMakeValid(GetString(STR_STATUSBAR_PAUSED), {}));

	if (_tool != MiniTool::None) {
		/* The active tool announces itself beside the cursor: official name on
		 * top, action and rotation hints below, live size while dragging. */
		std::string_view hint;
		switch (_tool) {
			case MiniTool::Rail: hint = "DRAG PATH / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Road: hint = "DRAG LINE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Station: hint = "DRAG AREA / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::BusStop:
			case MiniTool::TruckStop: hint = "Q E ROTATE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::TrainDepot:
			case MiniTool::RoadDepot: hint = "Q E ROTATE EXIT / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Demolish: hint = "DRAG AREA / RMB CANCEL"; break;
			case MiniTool::Signal: hint = "CLICK BUILD OR CYCLE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::RailTunnel:
			case MiniTool::RoadTunnel: hint = "CLICK SLOPE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Terraform: hint = "DRAG LEVEL / CLICK RAISE / CTRL LOWER / RMB CANCEL"; break;
			default: break;
		}
		if (_cursor.in_window) {
			std::string title = ToolLabel(_tool);
			if (_dragging) {
				if (_tool == MiniTool::Rail && !_plan.pieces.empty()) title += fmt::format("  {}", _plan.pieces.size());
				if (_tool == MiniTool::Road && !_road_plan.tiles.empty()) title += fmt::format("  {}", _road_plan.tiles.size());
				if (IsRectTool(_tool) && _rect_plan.valid) title += fmt::format("  {}x{}", _rect_plan.x1 - _rect_plan.x0 + 1, _rect_plan.y1 - _rect_plan.y0 + 1);
			}
			int wmax = std::max<int>(GetStringBoundingBox(title).width, GetStringBoundingBox(hint).width);
			int tx = std::min(_cursor.pos.x + 9 * s, _fbw - wmax - 4 * s);
			int ty = std::min(_cursor.pos.y + 11 * s, _fbh - 2 * lh - 8 * s);
			DrawHudText(tx, ty, title);
			DrawHudText(tx, ty + lh + 4, hint);
		}
	} else if (Vehicle::GetIfValid(_sel_vehicle) != nullptr) {
		/* Transient state only: the reference keeps the centre of the screen
		 * clear unless something is selected or being placed. */
		std::string_view hint = _follow
			? "FOLLOWING  CLICK STATION ORDER  O DROP ORDER  P START STOP  H UNFOLLOW  ESC DESELECT"
			: "CLICK STATION ORDER / CTRL FULL LOAD  O DROP ORDER  P START STOP  H FOLLOW  ESC DESELECT";
		DrawHudTextCentred(_fbw / 2, _fbh - MenuTileSide() - 12 * s - lh, hint);
	}
}

static void Present()
{
	DrawLabels();
	DrawHud();
	DrawColonyPanel();
	DrawStatusStream();
	DrawBuildMenu();
	DrawCmdBar();
	DrawWinBar();
	DrawVehPanel();
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
	_zoom_wx = MapXAt(sy);
	_zoom_wy = MapYAt(sx);
	_zoom_anchored = true;
}

static void Deactivate()
{
	_mini_active = false;
	_tool = MiniTool::None;
	_overlay = MiniLayer::None;
	_overlay_auto = false;
	_last_tool_layer = MiniLayer::None;
	_menu_open = -1;
	_win_open = -1;
	_dragging = false;
	_zoom_anchored = false;
	_glide = false;
	_sel_vehicle = VehicleID::Invalid();
	_follow = false;
	_veh_panel_shown = false;
	_veh_snap.clear();
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
	if (VideoDriver::GetInstance()->GetName() != "raylib") return;

	LoadMiniSettings();
	MiniAtlasReload();
	UndrawMouseCursor();
	/* One palette-driven fill resets the 32bpp-anim mapping buffer, so later
	 * direct framebuffer writes are not overwritten by palette animation. */
	{
		AutoRestoreBackup dpi_backup(_cur_dpi, &_screen);
		GfxFillRect(0, 0, _screen.width - 1, _screen.height - 1, PC_BLACK);
	}

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

/* Native windows dock to the top-right corner, stacking downward column by
 * column; when no column has room the newest window covers the corner. */
bool MiniUiWindowPlacement(int width, int height, Point &pt)
{
	if (!_mini_active) return false;

	int right = std::max(0, _screen.width - width);
	if (_veh_panel_shown) right = std::max(0, _veh_panel_rect.left - width);
	int y0 = _win_bar_bottom + 1;
	for (int x = right; x >= 0; x -= width) {
		int y = y0;
		bool moved = true;
		while (moved) {
			moved = false;
			for (const Window *w : Window::Iterate()) {
				if (MiniUiHidesWindow(w->window_class)) continue;
				if (x < w->left + w->width && w->left < x + width && y < w->top + w->height && w->top < y + height) {
					y = w->top + w->height;
					moved = true;
				}
			}
		}
		if (y + height <= _screen.height) {
			pt = {x, y};
			return true;
		}
	}

	pt = {right, 0};
	return true;
}

void MiniUiOverlayRects(std::vector<RlwRectI> &rects)
{
	if (!_mini_active) return;
	for (const Window *w : Window::IterateFromBack()) {
		if (MiniUiHidesWindow(w->window_class)) continue;
		rects.push_back({w->left, w->top, w->width, w->height});
	}
}

void MiniUiScrollTo(int x, int y)
{
	if (!_mini_active) return;
	_follow = false;
	_zoom_anchored = false;
	_glide = true;
	_glide_x = x / (double)TILE_SIZE;
	_glide_y = y / (double)TILE_SIZE;
	_dest_ppt = std::max(_dest_ppt, _ms.jump_ppt);
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
		_pan_vx = 0.0;
		_pan_vy = 0.0;
		_cam_y -= _cursor.delta.x * _ms.drag_pan_multiplier / _cam_ppt;
		_cam_x -= _cursor.delta.y * _ms.drag_pan_multiplier / _cam_ppt;
		ClampCamera();
	}

	if (_cursor.wheel != 0) {
		if (_menu_open >= 0 && InRect(_menu_panel_rect, _cursor.pos.x, _cursor.pos.y)) {
			_menu_scroll += _cursor.wheel > 0 ? 1 : -1;
		} else {
			ZoomAt(_cursor.pos.x, _cursor.pos.y, _cursor.wheel < 0);
		}
		_cursor.wheel = 0;
	}

	if (_left_button_down && !_left_button_clicked) {
		_left_button_clicked = true;
		if (!HandleMenuClick(_cursor.pos.x, _cursor.pos.y) && !HandleCmdClick(_cursor.pos.x, _cursor.pos.y) && !HandleWinClick(_cursor.pos.x, _cursor.pos.y) && !HandleSpeedClick(_cursor.pos.x, _cursor.pos.y) && !HandleStatusClick(_cursor.pos.x, _cursor.pos.y) && !HandleVehPanelClick(_cursor.pos.x, _cursor.pos.y)) {
			if (_tool == MiniTool::None) {
				if (!TryAppendOrder(_cursor.pos.x, _cursor.pos.y) && !HandleLabelClick(_cursor.pos.x, _cursor.pos.y)) {
					SelectVehicleAt(_cursor.pos.x, _cursor.pos.y);
				}
			} else if (IsPointTool(_tool)) {
				_drag_remove = _ctrl_pressed;
				_drag_ax = MapXAt(_cursor.pos.y);
				_drag_ay = MapYAt(_cursor.pos.x);
				CommitPointTool();
			} else {
				_dragging = true;
				_drag_remove = _ctrl_pressed;
				_drag_ax = MapXAt(_cursor.pos.y);
				_drag_ay = MapYAt(_cursor.pos.x);
				if (_tool == MiniTool::Rail) {
					_plan.path.clear();
					UpdateRailPlan(_drag_ax, _drag_ay);
				}
				if (_tool == MiniTool::Road) UpdateRoadPlan(_drag_ax, _drag_ay);
				if (IsRectTool(_tool)) UpdateRectPlan(_drag_ax, _drag_ay, RectPlanLimit());
			}
		}
	}

	if (!_left_button_down && _prev_left && _dragging) {
		_dragging = false;
		if (_tool == MiniTool::Rail) CommitRailPlan();
		if (_tool == MiniTool::Road) CommitRoadPlan();
		if (_tool == MiniTool::Station) CommitStationPlan();
		if (_tool == MiniTool::Demolish) CommitDemolishPlan();
		if (_tool == MiniTool::Terraform) CommitTerraformPlan();
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

		/* Escape only unwinds mini UI state; leaving the mini UI is F9 alone. */
		case WKC_ESC:
			if (_dragging) {
				_dragging = false;
				ClearPlans();
			} else if (_tool != MiniTool::None) {
				_tool = MiniTool::None;
			} else if (_menu_open >= 0 || _win_open >= 0) {
				_menu_open = -1;
				_win_open = -1;
			} else if (_sel_vehicle != VehicleID::Invalid()) {
				_sel_vehicle = VehicleID::Invalid();
				_follow = false;
			}
			break;

		case 'R':
			_tool = _tool == MiniTool::Rail ? MiniTool::None : MiniTool::Rail;
			break;

		case 'E':
			if (IsDirPointTool(_tool)) {
				_point_dir = ChangeDiagDir(_point_dir, DIAGDIRDIFF_90RIGHT);
			} else {
				_tool = _tool == MiniTool::Road ? MiniTool::None : MiniTool::Road;
			}
			break;

		case 'Q':
			if (IsDirPointTool(_tool)) _point_dir = ChangeDiagDir(_point_dir, DIAGDIRDIFF_90LEFT);
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
			_tool = _tool == MiniTool::RailTunnel ? MiniTool::None : MiniTool::RailTunnel;
			break;

		case 'I':
			_tool = _tool == MiniTool::RoadTunnel ? MiniTool::None : MiniTool::RoadTunnel;
			break;

		case 'Z':
			_tool = _tool == MiniTool::Terraform ? MiniTool::None : MiniTool::Terraform;
			break;

		case 'P':
			if (const Vehicle *v = Vehicle::GetIfValid(_sel_vehicle); v != nullptr) {
				Command<CMD_START_STOP_VEHICLE>::Post(STR_ERROR_CAN_T_STOP_START_TRAIN + v->type, v->tile, _sel_vehicle, false);
			}
			break;

		case 'H':
			_follow = !_follow && Vehicle::GetIfValid(_sel_vehicle) != nullptr;
			if (_follow) {
				_glide = false;
				_zoom_anchored = false;
				_dest_ppt = MAX_PPT;
			}
			break;

		case 'O':
			if (const Vehicle *v = Vehicle::GetIfValid(_sel_vehicle); v != nullptr && v->GetNumOrders() > 0) {
				Command<CMD_DELETE_ORDER>::Post(STR_ERROR_CAN_T_DELETE_THIS_ORDER, v->tile, v->index, (VehicleOrderID)(v->GetNumOrders() - 1));
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
	/* Entering a game activates the mini UI unless the config opts out. */
	static GameMode last_mode = GM_MENU;
	if (_game_mode != last_mode) {
		last_mode = _game_mode;
		if (!_mini_active && !_network_dedicated && (_game_mode == GM_NORMAL || _game_mode == GM_EDITOR)) {
			LoadMiniSettings();
			if (_ms.start_active != 0) MiniUiToggle();
		}
	}

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
			_zoom_anchored = false;
			_dest_ppt = MAX_PPT;
		}
	}

	_fbw = _screen.width;
	_fbh = _screen.height;
	if (_fbw <= 0 || _fbh <= 0) return;

	_mini_frame++;
	PruneTextCache();
	MiniAtlasEnsure();
	UpdateLerpClock(delta_ms);
	RlwCmdClear();

	/* WASD and arrows arrive via _dirkeys; pan speed is constant in screen space.
	 * A held axis takes its velocity directly so movement starts instantly; a
	 * released axis decays on its own, so letting go of one diagonal key keeps
	 * the other axis coasting. */
	if (_dirkeys != 0) {
		_zoom_anchored = false;
		_follow = false;
		_glide = false;
	} else if (_follow || _glide || _zoom_anchored) {
		_pan_vx = 0.0;
		_pan_vy = 0.0;
	}
	{
		double speed = _shift_pressed ? _ms.pan_speed_fast : _ms.pan_speed;
		double f = 1.0 - std::exp(delta_ms / -_ms.pan_smooth_ms);
		if (_dirkeys & (1 | 4)) {
			_pan_vx = ((_dirkeys & 4) ? speed : 0.0) - ((_dirkeys & 1) ? speed : 0.0);
		} else {
			_pan_vx -= _pan_vx * f;
			if (std::abs(_pan_vx) < 5.0) _pan_vx = 0.0;
		}
		if (_dirkeys & (2 | 8)) {
			_pan_vy = ((_dirkeys & 8) ? speed : 0.0) - ((_dirkeys & 2) ? speed : 0.0);
		} else {
			_pan_vy -= _pan_vy * f;
			if (std::abs(_pan_vy) < 5.0) _pan_vy = 0.0;
		}
	}
	if (_pan_vx != 0.0 || _pan_vy != 0.0) {
		_cam_y += _pan_vx * delta_ms / 1000.0 / _cam_ppt;
		_cam_x += _pan_vy * delta_ms / 1000.0 / _cam_ppt;
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
			_cam_y += ex;
			_cam_x += ey;
			ClampCamera();
		}
	}

	if (_follow) {
		const Vehicle *fv = Vehicle::GetIfValid(_sel_vehicle);
		if (fv == nullptr) {
			_follow = false;
		} else {
			auto [wx, wy] = LerpVehWorld(fv);
			_cam_x = wx;
			_cam_y = wy;
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
		_cam_x = _zoom_wx - (_zoom_sy - _fbh * 0.5) / _cam_ppt;
		_cam_y = _zoom_wy - (_zoom_sx - _fbw * 0.5) / _cam_ppt;
		ClampCamera();
		if (_cam_ppt == _dest_ppt) _zoom_anchored = false;
	}

	int ppt = std::max(1, (int)std::lround(_cam_ppt));
	ComputeZoomDetail(ppt);

	MiniLayer tool_layer = ToolLayer(_tool);
	if (tool_layer != _last_tool_layer) {
		if (tool_layer != MiniLayer::None) {
			_overlay = tool_layer;
			_overlay_auto = true;
		} else if (_overlay_auto) {
			_overlay = MiniLayer::None;
			_overlay_auto = false;
		}
		_last_tool_layer = tool_layer;
	}
	_filter_layer = _ms.filter_alpha > 0 ? _overlay : MiniLayer::None;

	int tx0 = std::max(0, (int)std::floor(MapXAt(0)));
	int ty0 = std::max(0, (int)std::floor(MapYAt(0)));
	int tx1 = std::min<int>(Map::SizeX() - 1, (int)std::floor(MapXAt(_fbh - 1)));
	int ty1 = std::min<int>(Map::SizeY() - 1, (int)std::floor(MapYAt(_fbw - 1)));

	_grey_map = _filter_layer != MiniLayer::None;
	FillRect(0, 0, _fbw - 1, _fbh - 1, COL_VOID);
	static std::vector<std::pair<int, int>> tree_dots;
	static std::vector<std::pair<int, int>> layer_tiles;
	tree_dots.clear();
	layer_tiles.clear();
	for (int ty = ty0; ty <= ty1; ty++) {
		int run_start = -1;
		uint32_t run_c = 0;
		MiniSprite run_art = MiniSprite::End;
		auto flush = [&](int tx_end) {
			if (run_start < 0) return;
			int x0 = ScrX(ty), y0 = ScrY(run_start), x1 = ScrX(ty + 1) - 1, y1 = ScrY(tx_end) - 1;
			if (run_art == MiniSprite::End || !MiniAtlasTileRun(run_art, x0, y0, x1, y1, tx_end - run_start, MapCol(run_c))) {
				FillRect(x0, y0, x1, y1, run_c);
			}
			run_start = -1;
		};
		for (int tx = tx0; tx <= tx1; tx++) {
			TileIndex tile = TileXY(tx, ty);
			uint32_t c;
			bool tree_dot;
			MiniSprite art;
			if (TileRunColour(tile, tx, ty, ppt, c, tree_dot, art)) {
				if (run_start >= 0 && (c != run_c || art != run_art)) flush(tx);
				if (run_start < 0) {
					run_start = tx;
					run_c = c;
					run_art = art;
				}
				if (tree_dot) tree_dots.emplace_back(tx, ty);
			} else {
				flush(tx);
				DrawTile(tile, tx, ty, ppt);
				if (_filter_layer != MiniLayer::None) layer_tiles.emplace_back(tx, ty);
			}
		}
		flush(tx1 + 1);
	}

	/* Deliberate tile grid at build zooms: merged runs are seamless, so tile
	 * boundaries return as their own faint overlay instead of draw artefacts. */
	if (ppt >= 8 && _ms.grid_alpha > 0) {
		int gx0 = std::max(0, ScrX(ty0));
		int gx1 = std::min(_fbw - 1, ScrX(ty1 + 1) - 1);
		int gy0 = std::max(0, ScrY(tx0));
		int gy1 = std::min(_fbh - 1, ScrY(tx1 + 1) - 1);
		for (int ty = ty0; ty <= ty1 + 1; ty++) {
			int x = ScrX(ty);
			if (x >= 0 && x < _fbw) BlendRect(x, gy0, x, gy1, COL_SHADOW, _ms.grid_alpha);
		}
		for (int tx = tx0; tx <= tx1 + 1; tx++) {
			int y = ScrY(tx);
			if (y >= 0 && y < _fbh) BlendRect(gx0, y, gx1, y, COL_SHADOW, _ms.grid_alpha);
		}
	}

	int tree_r = std::max(1, ppt / 8);
	for (auto [tx, ty] : tree_dots) {
		FillShapeRot(MiniSprite::Tree, (ScrX(ty) + ScrX(ty + 1) - 1) / 2, (ScrY(tx) + ScrY(tx + 1) - 1) / 2, tree_r, 0, COL_TREE);
	}

	_grey_map = false;

	/* Runs only merge bare ground and water, so every tile that can carry
	 * layer content already went through DrawTile and sits in layer_tiles. */
	if (_filter_layer != MiniLayer::None) {
		for (auto [tx, ty] : layer_tiles) {
			DrawTileLayer(TileXY(tx, ty), tx, ty, ppt, _filter_layer);
		}
	}

	if (_dragging) {
		if (_tool == MiniTool::Rail) {
			UpdateRailPlan(MapXAt(_cursor.pos.y), MapYAt(_cursor.pos.x));
			DrawRailPlan(ppt);
		} else if (_tool == MiniTool::Road) {
			UpdateRoadPlan(MapXAt(_cursor.pos.y), MapYAt(_cursor.pos.x));
			DrawRoadPlan(ppt);
		} else if (IsRectTool(_tool)) {
			UpdateRectPlan(MapXAt(_cursor.pos.y), MapYAt(_cursor.pos.x), RectPlanLimit());
			DrawRectPlan(ppt);
		}
	} else if (IsPointTool(_tool)) {
		DrawPointToolPlan(ppt);
	} else if (_tool != MiniTool::None) {
		int htx = (int)std::floor(MapXAt(_cursor.pos.y));
		int hty = (int)std::floor(MapYAt(_cursor.pos.x));
		if (htx >= 0 && hty >= 0 && htx < (int)Map::SizeX() && hty < (int)Map::SizeY()) {
			uint32_t c = _tool == MiniTool::Demolish ? COL_BP_RM : COL_BP;
			BlendRect(ScrX(hty), ScrY(htx), ScrX(hty + 1) - 1, ScrY(htx + 1) - 1, c, 70);
		}
	}

	DrawOrderRoute();
	DrawVehicles(ppt);
	DrawSelectionRing(ppt);
	Present();
}
