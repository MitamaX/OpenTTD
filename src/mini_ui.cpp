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
#include "airport.h"
#include "autoreplace_cmd.h"
#include "autoreplace_func.h"
#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "company_gui.h"
#include "elrail_func.h"
#include "engine_base.h"
#include "core/backup_type.hpp"
#include "core/math_func.hpp"
#include "core/utf8.hpp"
#include "depot_base.h"
#include "depot_cmd.h"
#include "depot_func.h"
#include "depot_map.h"
#include "train_cmd.h"
#include <unordered_map>
#include <unordered_set>
#include "fileio_func.h"
#include "gfx_func.h"
#include "graph_gui.h"
#include "ground_vehicle.hpp"
#include "group.h"
#include "group_cmd.h"
#include "gui.h"
#include "industry.h"
#include "ini_type.h"
#include "landscape.h"
#include "landscape_cmd.h"
#include "league_gui.h"
#include "mini_atlas.h"
#include "economy_func.h"
#include "misc_cmd.h"
#include "network/network.h"
#include "network/network_type.h"
#include "newgrf_airport.h"
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
#include "textbuf_gui.h"
#include "town.h"
#include "town_cmd.h"
#include "tile_map.h"
#include "train.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_economy.h"
#include "timer/timer_game_tick.h"
#include "tunnelbridge_cmd.h"
#include "tunnelbridge_map.h"
#include "vehicle_base.h"
#include "vehicle_cmd.h"
#include "water_cmd.h"
#include "waypoint_cmd.h"
#include "vehicle_func.h"
#include "vehicle_gui.h"
#include "vehiclelist.h"
#include "video/video_driver.hpp"
#include "viewport_func.h"
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
	RailWaypoint,
	BusStop,
	TruckStop,
	TrainDepot,
	RoadDepot,
	ShipDepot,
	Dock,
	Buoy,
	Canal,
	Lock,
	Airport,
	Demolish,
	Signal,
	RailTunnel,
	RoadTunnel,
	Terraform,
};

static bool IsRectTool(MiniTool t)
{
	return t == MiniTool::Station || t == MiniTool::Demolish || t == MiniTool::Terraform || t == MiniTool::Canal;
}

static bool IsPointTool(MiniTool t)
{
	return t == MiniTool::BusStop || t == MiniTool::TruckStop || t == MiniTool::TrainDepot || t == MiniTool::RoadDepot || t == MiniTool::Signal || t == MiniTool::RailTunnel || t == MiniTool::RoadTunnel || t == MiniTool::RailWaypoint || t == MiniTool::ShipDepot || t == MiniTool::Dock || t == MiniTool::Buoy || t == MiniTool::Airport || t == MiniTool::Lock;
}

static bool IsDirPointTool(MiniTool t)
{
	return t == MiniTool::BusStop || t == MiniTool::TruckStop || t == MiniTool::TrainDepot || t == MiniTool::RoadDepot || t == MiniTool::RailWaypoint || t == MiniTool::ShipDepot;
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
		case MiniTool::RailWaypoint:
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

static VehicleID _follow_veh = VehicleID::Invalid();

static VehicleID _order_pick_veh = VehicleID::Invalid();

/* Consist drafts for the fleet window, one per vehicle type. A draft is
 * assembled in the window and produced whole at a depot. */
static std::vector<EngineID> _fleet_draft[4];

/* Deploy job: the draft is produced unit by unit and each new vehicle is
 * attached to the job's own consist head by explicit id, so the result is
 * one connected train regardless of what else sits in the depot. Commands
 * are asynchronous under network play, hence the stepwise states. */
struct FleetDeploy {
	TileIndex depot = INVALID_TILE;
	VehicleType vt = VEH_TRAIN;
	std::vector<EngineID> units;
	size_t next = 0;
	uint8_t stage = 0;
	VehicleID head = VehicleID::Invalid();
	VehicleID fresh = VehicleID::Invalid();
	std::vector<VehicleID> before;
	int waited = 0;
};
static FleetDeploy _deploy;

/* The screen has exactly one input mode: idle, building or following a
 * vehicle. Entering one drops the others. */
static void EnterIdleMode()
{
	_tool = MiniTool::None;
	_follow_veh = VehicleID::Invalid();
	_order_pick_veh = VehicleID::Invalid();
}

static void EnterBuildMode(MiniTool t)
{
	EnterIdleMode();
	_tool = t;
}

static void EnterFollowMode(VehicleID v)
{
	EnterIdleMode();
	_follow_veh = v;
}

static void EnterOrderPickMode(VehicleID v)
{
	EnterIdleMode();
	_order_pick_veh = v;
}

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
	int pad;
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

		/* TrueType glyph bitmaps can overhang the layout box; the padded
		 * canvas keeps those pixels instead of clipping them away. */
		int pad = GetCharacterHeight(FS_NORMAL) / 4 + 1;
		int bw = w + 2 * pad;
		int bh = h + 2 * pad;
		std::vector<uint32_t> buf((size_t)bw * bh, 0xFF000000U);
		DrawPixelInfo dpi;
		dpi.dst_ptr = buf.data();
		dpi.left = 0;
		dpi.top = 0;
		dpi.width = bw;
		dpi.height = bh;
		dpi.pitch = bw;
		dpi.zoom = ZoomLevel::Min;
		{
			AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);
			AutoRestoreBackup anim_backup(_screen_disable_anim, true);
			DrawString(pad, pad + w - 1, pad, text, TC_WHITE, SA_LEFT | SA_FORCE);
		}
		for (uint32_t &px : buf) {
			uint32_t a = std::max({(px >> 16) & 0xFF, (px >> 8) & 0xFF, px & 0xFF});
			px = (a << 24) | 0x00FFFFFFU;
		}
		it = _text_cache.emplace(std::string(text), MiniTextEntry{RlwCreateTexture(buf.data(), bw, bh), w, h, pad, 0}).first;
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

static void DrawTextQuad(const MiniTextEntry *e, int x, int y, uint32_t tint)
{
	RlwCmdTexQuad(e->tex, x - e->pad, y - e->pad, tint);
}

static void DrawScreenText(int x, int y, std::string_view text, TextColour colour = TC_WHITE)
{
	const MiniTextEntry *e = TextTexture(text);
	if (e != nullptr) DrawTextQuad(e, x, y, TextTint(colour));
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

	/* Units inside a depot are hidden one by one; the consist keeps drawing
	 * through its still-visible run. */
	const Vehicle *first = nullptr;
	const Vehicle *tail = nullptr;
	double s = 0.0;
	double prev_len = 0.0;
	for (const Vehicle *u = head; u != nullptr; u = u->Next()) {
		if (u->vehstatus.Test(VehState::Hidden)) continue;
		auto [ux, uy] = LerpVehWorld(u);
		raw.emplace_back(uy * _cam_ppt + ScrBaseX(), ux * _cam_ppt + ScrBaseY());
		double len = u->GetGroundVehicleCache()->cached_veh_length * ppt / (double)TILE_SIZE;
		if (!want.empty()) s += (prev_len + len) * 0.5;
		want.push_back(s);
		prev_len = len;
		if (first == nullptr) first = u;
		tail = u;
	}
	if (raw.empty()) return;

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
	auto [nx, ny] = overhang(first, 1);
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

	if (!dim && first == head) FillCircle(pts[0].first, pts[0].second, std::max(1, w / 2 - 1), COL_PAPER);

	if (!dim && _zd.cargo_dots) {
		int half = std::max(3, ppt * 2 / 5) / 2;
		int dr = std::max(1, half - 2);
		size_t i = 1;
		for (const Vehicle *u = head; u != nullptr; u = u->Next()) {
			if (u->vehstatus.Test(VehState::Hidden)) continue;
			if (u->cargo_cap != 0 && IsValidCargoType(u->cargo_type)) {
				FillCircle(pts[i].first, pts[i].second, dr + 1, COL_INK);
				FillCircle(pts[i].first, pts[i].second, dr, CargoRgb(u->cargo_type));
			}
			i++;
		}
	}
}

static void DrawVehicles(int ppt)
{
	int half = std::max(3, ppt * 2 / 5) / 2;
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->type > VEH_AIRCRAFT) continue;
		if (v->type == VEH_TRAIN && _zd.vehicle_shapes) {
			if (v->IsPrimaryVehicle()) DrawTrainConsist(v, ppt);
			continue;
		}
		if (v->vehstatus.Test(VehState::Hidden)) continue;
		if (v->type == VEH_AIRCRAFT && !v->IsPrimaryVehicle()) continue;
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

/* Any unit of a consist opens its head's window, so the window always
 * describes the whole vehicle. */
static bool OpenVehicleWndAt(int sx, int sy)
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
	if (best != nullptr) ShowVehicleViewWindow(best->First());
	return best != nullptr;
}

static bool OpenDepotWndAt(int sx, int sy)
{
	int tx = (int)std::floor(MapXAt(sy));
	int ty = (int)std::floor(MapYAt(sx));
	if (tx < 0 || ty < 0 || tx >= (int)Map::SizeX() || ty >= (int)Map::SizeY()) return false;
	TileIndex tile = TileXY(tx, ty);
	if (!IsDepotTile(tile)) return false;
	ShowDepotWindow(tile, GetDepotVehicleType(tile));
	return true;
}

/* Stock order-window map picking, trimmed to the plain cases: a click
 * resolves to a depot, waypoint or station order for the picked vehicle. */
static Order OrderFromTile(const Vehicle *v, TileIndex tile)
{
	Order order{};

	if (IsDepotTypeTile(tile, (TransportType)(uint)v->type) && IsTileOwner(tile, _local_company)) {
		order.MakeGoToDepot(GetDepotDestinationIndex(tile),
				OrderDepotTypeFlag::PartOfOrders,
				(_settings_client.gui.new_nonstop && v->IsGroundVehicle()) ? OrderNonStopFlag::NoIntermediate : OrderNonStopFlags{});
		return order;
	}

	if ((IsRailWaypointTile(tile) && v->type == VEH_TRAIN && IsTileOwner(tile, _local_company)) ||
			(IsRoadWaypointTile(tile) && v->type == VEH_ROAD && IsTileOwner(tile, _local_company)) ||
			(IsBuoyTile(tile) && v->type == VEH_SHIP)) {
		order.MakeGoToWaypoint(GetStationIndex(tile));
		if (!IsBuoyTile(tile) && _settings_client.gui.new_nonstop) order.SetNonStopType({OrderNonStopFlag::NoIntermediate, OrderNonStopFlag::NoDestination});
		return order;
	}

	if (IsTileType(tile, MP_STATION) || IsTileType(tile, MP_INDUSTRY)) {
		const Station *st = IsTileType(tile, MP_STATION) ? Station::GetByTile(tile) : Industry::GetByTile(tile)->neutral_station;
		if (st != nullptr && (st->owner == _local_company || st->owner == OWNER_NONE)) {
			StationFacilities facil;
			switch (v->type) {
				case VEH_SHIP:     facil = StationFacility::Dock;    break;
				case VEH_TRAIN:    facil = StationFacility::Train;   break;
				case VEH_AIRCRAFT: facil = StationFacility::Airport; break;
				default:           facil = {StationFacility::BusStop, StationFacility::TruckStop}; break;
			}
			if (st->facilities.Any(facil)) {
				order.MakeGoToStation(st->index);
				if (_settings_client.gui.new_nonstop && v->IsGroundVehicle()) order.SetNonStopType(OrderNonStopFlag::NoIntermediate);
				order.SetStopLocation(v->type == VEH_TRAIN ? (OrderStopLocation)(_settings_client.gui.stop_location) : OrderStopLocation::FarEnd);
				return order;
			}
		}
	}

	order.Free();
	return order;
}

static void OrderPickClick(int sx, int sy)
{
	const Vehicle *v = Vehicle::GetIfValid(_order_pick_veh);
	if (v == nullptr || v->owner != _local_company) {
		EnterIdleMode();
		return;
	}
	int tx = (int)std::floor(MapXAt(sy));
	int ty = (int)std::floor(MapYAt(sx));
	if (tx < 0 || ty < 0 || tx >= (int)Map::SizeX() || ty >= (int)Map::SizeY()) return;
	Order order = OrderFromTile(v, TileXY(tx, ty));
	if (order.IsType(OT_NOTHING)) return;
	Command<CMD_INSERT_ORDER>::Post(STR_ERROR_CAN_T_INSERT_NEW_ORDER, v->tile, v->index, (VehicleOrderID)v->GetNumOrders(), order);
}

/* Stage 0 issues the next build, stage 1 spots the new vehicle among the
 * depot's chains, stage 2 waits for its attach move to apply before the
 * next unit goes out. Single player resolves each stage within a frame. */
static void ProcessFleetDeploy()
{
	if (_deploy.depot == INVALID_TILE) return;
	if (!IsDepotTile(_deploy.depot) || GetDepotVehicleType(_deploy.depot) != _deploy.vt) {
		_deploy = FleetDeploy{};
		return;
	}

	auto depot_ids = [&]() {
		std::vector<VehicleID> ids;
		for (const Vehicle *v : Vehicle::Iterate()) {
			if (v->type == _deploy.vt && v->tile == _deploy.depot) ids.push_back(v->index);
		}
		return ids;
	};

	switch (_deploy.stage) {
		case 0: {
			if (_deploy.next >= _deploy.units.size()) {
				_deploy = FleetDeploy{};
				return;
			}
			const Engine *e = Engine::GetIfValid(_deploy.units[_deploy.next]);
			if (e == nullptr) {
				_deploy.next++;
				return;
			}
			_deploy.before = depot_ids();
			Command<CMD_BUILD_VEHICLE>::Post(GetCmdBuildVehMsg(_deploy.vt), _deploy.depot, e->index, true, INVALID_CARGO, INVALID_CLIENT_ID);
			_deploy.stage = 1;
			_deploy.waited = 0;
			return;
		}

		case 1: {
			for (VehicleID id : depot_ids()) {
				if (std::find(_deploy.before.begin(), _deploy.before.end(), id) != _deploy.before.end()) continue;
				const Vehicle *nv = Vehicle::GetIfValid(id);
				if (nv == nullptr || nv->First() != nv) continue;
				_deploy.fresh = id;
				break;
			}
			if (_deploy.fresh == VehicleID::Invalid()) {
				if (++_deploy.waited > 180) _deploy = FleetDeploy{};
				return;
			}
			if (_deploy.vt != VEH_TRAIN || _deploy.head == VehicleID::Invalid()) {
				_deploy.head = _deploy.fresh;
				_deploy.fresh = VehicleID::Invalid();
				_deploy.next++;
				_deploy.stage = 0;
				return;
			}
			const Vehicle *hv = Vehicle::GetIfValid(_deploy.head);
			if (hv == nullptr) {
				_deploy = FleetDeploy{};
				return;
			}
			Command<CMD_MOVE_RAIL_VEHICLE>::Post(STR_ERROR_CAN_T_MOVE_VEHICLE, _deploy.depot, _deploy.fresh, hv->Last()->index, false);
			_deploy.stage = 2;
			_deploy.waited = 0;
			return;
		}

		case 2: {
			const Vehicle *nv = Vehicle::GetIfValid(_deploy.fresh);
			if (nv == nullptr) {
				_deploy = FleetDeploy{};
				return;
			}
			if (nv->First()->index == _deploy.head) {
				_deploy.fresh = VehicleID::Invalid();
				_deploy.next++;
				_deploy.stage = 0;
				return;
			}
			if (++_deploy.waited > 180) _deploy = FleetDeploy{};
			return;
		}
	}
}

static VehicleID FrontWndVehicle();

/* Route preview for the front window's vehicle: stop-to-stop legs in
 * blueprint blue, the leg from the vehicle to its current destination
 * highlighted. */
static void DrawOrderRoute()
{
	const Vehicle *v = Vehicle::GetIfValid(FrontWndVehicle());
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

static void DrawVehicleRing(int ppt)
{
	const Vehicle *v = Vehicle::GetIfValid(FrontWndVehicle());
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

void ShowIndustryViewWindow(IndustryID industry);

/* Town names always show for navigation; station and industry names join
 * at the infrastructure zoom tier. Labels sit centred above their sign
 * tile. */
static std::vector<std::pair<Rect, TownID>> _town_label_hits;
static std::vector<std::pair<Rect, StationID>> _station_label_hits;
static std::vector<std::pair<Rect, IndustryID>> _industry_label_hits;

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
	if (e != nullptr) DrawTextQuad(e, r.left + pad, r.top + pad, TextTint(tc));
	return r;
}

static void DrawLabels()
{
	_town_label_hits.clear();
	_station_label_hits.clear();
	_industry_label_hits.clear();
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
	/* Oil rigs already carry the plate of their neutral station. */
	for (const Industry *ind : Industry::Iterate()) {
		if (ind->neutral_station != nullptr) continue;
		TileIndex tile = ind->location.GetCenterTile();
		int cx = ScrX(TileY(tile) + 0.5);
		int cy = ScrY(TileX(tile) + 0.5);
		if (cx < -margin || cy < 0 || cx >= _fbw + margin || cy >= _fbh + limit) continue;
		std::string str = GetString(STR_INDUSTRY_NAME, ind->index);
		Rect r = DrawLabelPlate(cx, cy, str, COL_IND, false, PlateTextColour(COL_IND));
		_industry_label_hits.emplace_back(r, ind->index);
	}
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
	for (const auto &[r, id] : _industry_label_hits) {
		if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom) {
			ShowIndustryViewWindow(id);
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

static bool StepBitIsX(int b)
{
	return b == 1 || b == 2;
}

/* Two corner pieces meeting as a switchback turn a train 90 degrees, which
 * rail cannot carry: with the last two steps perpendicular, a new step that
 * reverses the older one is refused and the walk detours instead. */
static bool RailStepAllowed(int d_pp, int d_prev, int d_new)
{
	if (d_pp == 0 || d_prev == 0) return true;
	return StepBitIsX(d_pp) == StepBitIsX(d_prev) || d_new != OppositeBit(d_pp);
}

static TileIndex StepTile(TileIndex t, int b)
{
	int x = (int)TileX(t) + (b == 2) - (b == 1);
	int y = (int)TileY(t) + (b == 8) - (b == 4);
	if (x < 0 || y < 0 || x > (int)Map::SizeX() - 2 || y > (int)Map::SizeY() - 2) return INVALID_TILE;
	return TileXY(x, y);
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

	for (int guard = 0; guard < 4096 && _plan.path.size() < 1024; guard++) {
		TileIndex cur = _plan.path.back();
		int cx = (int)TileX(cur), cy = (int)TileY(cur);
		int dx = tx - cx, dy = ty - cy;
		if (dx == 0 && dy == 0) break;

		size_t n = _plan.path.size();
		int d_prev = n >= 2 ? StepBit(_plan.path[n - 2], _plan.path[n - 1]) : 0;
		int d_pp = n >= 3 ? StepBit(_plan.path[n - 3], _plan.path[n - 2]) : 0;

		int step_x = dx != 0 ? (dx > 0 ? 2 : 1) : 0;
		int step_y = dy != 0 ? (dy > 0 ? 8 : 4) : 0;
		int prim = std::abs(dx) >= std::abs(dy) ? step_x : step_y;
		int sec = prim == step_x ? step_y : step_x;

		TileIndex next = INVALID_TILE;
		bool popped = false;
		for (int cand : {prim, sec, d_prev}) {
			if (cand == 0) continue;
			TileIndex t = StepTile(cur, cand);
			if (t == INVALID_TILE) continue;
			/* Any candidate stepping back onto the previous tile is the undo
			 * gesture; undoing ignores the turn rule. */
			if (n >= 2 && t == _plan.path[n - 2]) {
				_plan.path.pop_back();
				popped = true;
				break;
			}
			if (!RailStepAllowed(d_pp, d_prev, cand)) continue;
			next = t;
			break;
		}
		if (popped) continue;
		if (next == INVALID_TILE) break;
		_plan.path.push_back(next);
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

static void CommitCanalPlan()
{
	if (!_rect_plan.valid) return;
	if (_drag_remove) {
		Command<CMD_CLEAR_AREA>::Post(STR_ERROR_CAN_T_CLEAR_THIS_AREA, TileXY(_rect_plan.x1, _rect_plan.y1), TileXY(_rect_plan.x0, _rect_plan.y0), false);
	} else {
		Command<CMD_BUILD_CANAL>::Post(STR_ERROR_CAN_T_BUILD_CANALS, TileXY(_rect_plan.x1, _rect_plan.y1), TileXY(_rect_plan.x0, _rect_plan.y0), WaterClass::Canal, false);
	}
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

/* No airport picker window: Q/E walk the available airport types and the
 * blueprint previews the footprint, so the choice lives in the tool. */
static uint8_t _airport_type = 0;

static uint8_t PickAirportType()
{
	if (AirportSpec::Get(_airport_type)->IsAvailable()) return _airport_type;
	for (uint8_t i = 0; i < NUM_AIRPORTS; i++) {
		if (AirportSpec::Get(i)->IsAvailable()) return i;
	}
	return _airport_type;
}

static void CycleAirportType(int dir)
{
	for (int i = 1; i <= NUM_AIRPORTS; i++) {
		int t = ((int)PickAirportType() + dir * i) % NUM_AIRPORTS;
		if (t < 0) t += NUM_AIRPORTS;
		if (AirportSpec::Get((uint8_t)t)->IsAvailable()) {
			_airport_type = (uint8_t)t;
			return;
		}
	}
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

		case MiniTool::RailWaypoint:
			if (_drag_remove) {
				Command<CMD_REMOVE_FROM_RAIL_WAYPOINT>::Post(STR_ERROR_CAN_T_REMOVE_RAIL_WAYPOINT, tile, tile, true);
			} else {
				Command<CMD_BUILD_RAIL_WAYPOINT>::Post(STR_ERROR_CAN_T_BUILD_RAIL_WAYPOINT, tile, DiagDirToAxis(_point_dir), 1, 1, STAT_CLASS_WAYP, 0, StationID::Invalid(), false);
			}
			break;

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

		case MiniTool::ShipDepot:
			if (_drag_remove) {
				Command<CMD_LANDSCAPE_CLEAR>::Post(STR_ERROR_CAN_T_CLEAR_THIS_AREA, tile);
			} else {
				Command<CMD_BUILD_SHIP_DEPOT>::Post(STR_ERROR_CAN_T_BUILD_SHIP_DEPOT, tile, DiagDirToAxis(_point_dir));
			}
			break;

		case MiniTool::Dock:
			if (_drag_remove) {
				Command<CMD_LANDSCAPE_CLEAR>::Post(STR_ERROR_CAN_T_CLEAR_THIS_AREA, tile);
			} else {
				Command<CMD_BUILD_DOCK>::Post(STR_ERROR_CAN_T_BUILD_DOCK_HERE, tile, StationID::Invalid(), false);
			}
			break;

		case MiniTool::Buoy:
			if (_drag_remove) {
				Command<CMD_LANDSCAPE_CLEAR>::Post(STR_ERROR_CAN_T_CLEAR_THIS_AREA, tile);
			} else {
				Command<CMD_BUILD_BUOY>::Post(STR_ERROR_CAN_T_POSITION_BUOY_HERE, tile);
			}
			break;

		case MiniTool::Airport:
			if (_drag_remove) {
				Command<CMD_LANDSCAPE_CLEAR>::Post(STR_ERROR_CAN_T_CLEAR_THIS_AREA, tile);
			} else {
				Command<CMD_BUILD_AIRPORT>::Post(STR_ERROR_CAN_T_BUILD_AIRPORT_HERE, tile, PickAirportType(), 0, StationID::Invalid(), false);
			}
			break;

		case MiniTool::Lock:
			if (_drag_remove) {
				Command<CMD_LANDSCAPE_CLEAR>::Post(STR_ERROR_CAN_T_CLEAR_THIS_AREA, tile);
			} else {
				Command<CMD_BUILD_LOCK>::Post(STR_ERROR_CAN_T_BUILD_LOCKS, tile);
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
	} else if (_tool == MiniTool::BusStop || _tool == MiniTool::TruckStop || _tool == MiniTool::RailWaypoint) {
		DrawAxisBand(DiagDirToAxis(_point_dir), x0, y0, x1, y1, std::max(2, ppt / 3), c);
	} else if (_tool == MiniTool::RailTunnel || _tool == MiniTool::RoadTunnel || _tool == MiniTool::Dock || _tool == MiniTool::Lock) {
		DiagDirection d = GetInclinedSlopeDirection(GetTileSlope(TileXY(tx, ty)));
		if (d != INVALID_DIAGDIR) {
			int cx = (x0 + x1) / 2;
			int cy = (y0 + y1) / 2;
			ThickLine(cx, cy, cx + _diag_dx[d] * (ppt / 2), cy + _diag_dy[d] * (ppt / 2), std::max(2, ppt / 5), c);
		}
	} else if (_tool == MiniTool::ShipDepot) {
		/* The depot spans two tiles along its axis; show the real footprint. */
		Axis a = DiagDirToAxis(_point_dir);
		int fx1 = a == AXIS_Y ? ScrX(ty + 2) - 1 : x1;
		int fy1 = a == AXIS_X ? ScrY(tx + 2) - 1 : y1;
		BlendRect(x0, y0, fx1, fy1, c, 60);
	} else if (_tool == MiniTool::Airport) {
		const AirportSpec *as = AirportSpec::Get(PickAirportType());
		BlendRect(x0, y0, ScrX(ty + as->size_y) - 1, ScrY(tx + as->size_x) - 1, c, 60);
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
	{STR_LAI_STATION_DESCRIPTION_WAYPOINT, "WAYPOINT", MiniTool::RailWaypoint},
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

static const MiniMenuItem _menu_water_items[] = {
	{STR_LAI_STATION_DESCRIPTION_SHIP_DOCK, "DOCK", MiniTool::Dock},
	{STR_LAI_WATER_DESCRIPTION_SHIP_DEPOT, "DEPOT", MiniTool::ShipDepot},
	{STR_LAI_STATION_DESCRIPTION_BUOY, "BUOY", MiniTool::Buoy},
	{STR_LAI_WATER_DESCRIPTION_CANAL, "CANAL", MiniTool::Canal},
	{STR_LAI_WATER_DESCRIPTION_LOCK, "LOCK", MiniTool::Lock},
};

static const MiniMenuItem _menu_air_items[] = {
	{STR_LAI_STATION_DESCRIPTION_AIRPORT, "AIRPORT", MiniTool::Airport},
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
	{STR_LAI_WATER_DESCRIPTION_WATER, "WATER", MiniTool::Dock, _menu_water_items},
	{STR_REPLACE_VEHICLE_AIRCRAFT, "AIR", MiniTool::Airport, _menu_air_items},
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
		case MiniTool::RailWaypoint:
			ScreenThickLine(cx - h, cy, cx + h, cy, t, COL_PAPER);
			ScreenFillRect(cx - t, cy - h + 2, cx + t, cy + h - 2, COL_ST_RAIL);
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
		case MiniTool::ShipDepot:
			ScreenFillRect(cx - h, cy - h, cx + h, cy + h, COL_WATER);
			ScreenFillRect(cx + h - 2, cy - is / 4, cx + h, cy + is / 4, COL_PAPER);
			break;
		case MiniTool::Dock:
			ScreenFillRect(cx - h, cy - h, cx + h, cy + h, COL_WATER);
			ScreenFillRect(cx - t, cy - h, cx + t, cy + h, COL_BRIDGE);
			break;
		case MiniTool::Buoy:
			ScreenFillRect(cx - h, cy - h, cx + h, cy + h, COL_WATER);
			ScreenFillCircle(cx, cy, t + 1, COL_STOP);
			break;
		case MiniTool::Airport:
			ScreenFillRect(cx - h, cy - h, cx + h, cy + h, COL_ST_AIR_B);
			ScreenFillRect(cx - h + 2, cy - h + 2, cx + h - 2, cy + h - 2, COL_ST_AIR);
			ScreenFillRect(cx - h + 2, cy - 1, cx + h - 2, cy + 1, COL_PAPER);
			break;
		case MiniTool::Canal:
			ScreenFillRect(cx - h, cy - h, cx + h, cy + h, COL_BRIDGE);
			ScreenFillRect(cx - h, cy - is / 4, cx + h, cy + is / 4, COL_WATER);
			break;
		case MiniTool::Lock:
			ScreenFillRect(cx - h, cy - h, cx + h, cy + h, COL_WATER);
			ScreenFillRect(cx - is / 4 - 1, cy - h, cx - is / 4 + 1, cy + h, COL_PAPER);
			ScreenFillRect(cx + is / 4 - 1, cy - h, cx + is / 4 + 1, cy + h, COL_PAPER);
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
		DrawTextQuad(e, cx - e->w / 2, r.bottom - lh - 3, active ? COL_CH_ACCENT : COL_CH_TEXT);
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
			if (_tool == t) EnterIdleMode(); else EnterBuildMode(t);
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
			if (_tool == t) EnterIdleMode(); else EnterBuildMode(t);
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
	Buy,
	Groups,
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
	{INVALID_STRING_ID, "DEPOT", MiniWin::Buy},
	{INVALID_STRING_ID, "GROUPS", MiniWin::Groups},
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
		case MiniWin::Buy:
			ScreenFillRect(cx - h, cy - h, cx + h, cy + h, COL_DEPOT);
			ScreenFillRect(cx - t, cy - h, cx + t, cy - h + t + 1, COL_PAPER);
			break;
		case MiniWin::Groups:
			ScreenFillRect(cx - h, cy - h, cx + h, cy - h + t, cc);
			ScreenFillRect(cx - h, cy - t / 2, cx + h, cy - t / 2 + t, cc);
			ScreenFillRect(cx - h, cy + h - t, cx + h, cy + h, cc);
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
		DrawTextQuad(e, cx - e->w / 2, r.bottom - lh - 3, active ? COL_CH_ACCENT : COL_CH_TEXT);
	}
}

static void OpenFleetMiniWnd(int vt);
static void OpenFinanceMiniWnd();
static void OpenGroupMiniWnd();

static void OpenMiniWindow(MiniWin win)
{
	bool company = Company::IsValidID(_local_company);
	switch (win) {
		case MiniWin::Finances: if (company) OpenFinanceMiniWnd(); break;
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
		case MiniWin::Buy: if (company) OpenFleetMiniWnd(-1); break;
		case MiniWin::Groups: if (company) OpenGroupMiniWnd(); break;
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
 * controls, with the status rows hanging below it. */
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
	Stuck,
	Broken,
	NoOrders,
	BadOrders,
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
		case MiniStatus::Stuck: return MenuLabel(INVALID_STRING_ID, "STUCK");
		case MiniStatus::Broken: return MenuLabel(STR_VEHICLE_STATUS_BROKEN_DOWN, "BROKEN DOWN");
		case MiniStatus::NoOrders: return MenuLabel(INVALID_STRING_ID, "NO ORDERS");
		case MiniStatus::BadOrders: return MenuLabel(INVALID_STRING_ID, "BAD ORDERS");
		case MiniStatus::OldAge: return MenuLabel(INVALID_STRING_ID, "OLD AGE");
		default: return MenuLabel(INVALID_STRING_ID, "IN THE RED");
	}
}

/* A persistent row needs a hard error: void orders or a station the vehicle
 * cannot use. The softer advice cases of the native order review, too few
 * stations and a duplicate first and last entry, also flag valid schedules
 * like waypoint loops, so they stay with the one-shot native news. */
static bool HasBadOrders(const Vehicle *v)
{
	for (const Order &order : v->Orders()) {
		if (order.IsType(OT_DUMMY)) return true;
		if (order.IsType(OT_GOTO_STATION) && !CanVehicleUseStation(v, Station::Get(order.GetDestination().ToStationID()))) return true;
	}
	return false;
}

/* Waiting at a signal is normal traffic; a stuck train only becomes a status
 * row past the same wait the stuck news uses, and stays one until it moves. */
static std::unordered_set<uint32_t> _stuck_long;

static void ScanStatuses()
{
	for (auto &l : _status_veh) l.clear();
	static std::unordered_set<uint32_t> keep;
	keep.clear();
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->type > VEH_AIRCRAFT || !v->IsPrimaryVehicle() || v->owner != _local_company) continue;
		if (v->vehstatus.Test(VehState::Crashed)) {
			_status_veh[(int)MiniStatus::Crashed].push_back(v->index);
			continue;
		}
		if (v->vehicle_flags.Test(VehicleFlag::PathfinderLost)) _status_veh[(int)MiniStatus::Lost].push_back(v->index);
		if (v->type == VEH_TRAIN) {
			const Train *t = Train::From(v);
			if (t->flags.Test(VehicleRailFlag::Stuck)) {
				uint32_t id = v->index.base();
				if (t->wait_counter >= _settings_game.pf.wait_for_pbs_path * Ticks::DAY_TICKS || _stuck_long.contains(id)) {
					keep.insert(id);
					_status_veh[(int)MiniStatus::Stuck].push_back(v->index);
				}
			}
		}
		if (v->type != VEH_AIRCRAFT && v->breakdown_ctr == 1) _status_veh[(int)MiniStatus::Broken].push_back(v->index);
		if (v->GetNumOrders() == 0 && !v->vehstatus.Test(VehState::Stopped)) _status_veh[(int)MiniStatus::NoOrders].push_back(v->index);
		if (HasBadOrders(v)) _status_veh[(int)MiniStatus::BadOrders].push_back(v->index);
		if (v->age > v->max_age) _status_veh[(int)MiniStatus::OldAge].push_back(v->index);
		/* Last year alone would pin the row until new year even after the route
		 * was fixed; earning anything this year clears it. */
		if (v->economy_age >= VEHICLE_PROFIT_MIN_AGE && v->GetDisplayProfitLastYear() < 0 && v->GetDisplayProfitThisYear() < 0) _status_veh[(int)MiniStatus::Unprofitable].push_back(v->index);
	}
	std::swap(_stuck_long, keep);
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
		if (const MiniTextEntry *e = TextTexture(t); e != nullptr) DrawTextQuad(e, r.left + bar_w + 4 * s, y + (ch - lh) / 2, tcol);
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
			ShowVehicleViewWindow(v->First());
			MiniUiScrollTo(v->x_pos, v->y_pos);
			return true;
		}
		return true;
	}
	return false;
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
			case MiniTool::TruckStop:
			case MiniTool::RailWaypoint: hint = "Q E ROTATE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::TrainDepot:
			case MiniTool::RoadDepot: hint = "Q E ROTATE EXIT / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::ShipDepot: hint = "Q E ROTATE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Dock: hint = "CLICK SHORE SLOPE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Buoy: hint = "CLICK WATER / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Airport: hint = "Q E TYPE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Canal: hint = "DRAG AREA / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Lock: hint = "CLICK SLOPE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Demolish: hint = "DRAG AREA / RMB CANCEL"; break;
			case MiniTool::Signal: hint = "CLICK BUILD OR CYCLE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::RailTunnel:
			case MiniTool::RoadTunnel: hint = "CLICK SLOPE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Terraform: hint = "DRAG LEVEL / CLICK RAISE / CTRL LOWER / RMB CANCEL"; break;
			default: break;
		}
		if (_cursor.in_window) {
			std::string title = ToolLabel(_tool);
			if (_tool == MiniTool::Airport) {
				title += fmt::format("  {}", StrMakeValid(GetString(AirportSpec::Get(PickAirportType())->name), {}));
			}
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
	}
}

/* Mini windows: ONI-structured chrome windows on the GPU layer. A window has
 * a title bar with a rename pen and a close box, a uniform-width tab strip,
 * label-value body rows and square icon commands at the bottom. Every
 * window may embed a live native viewport through a frameless carrier
 * window that is kept aligned with its slot; carriers under higher mini
 * windows are clipped out of the native overlay. */

static constexpr int MW_CARRIER_NUM_BASE = 0x40000;
static const uint32_t COL_CH_RED = 0xFFE05F4AU;
static const uint32_t COL_CH_YELLOW = 0xFFE0B64AU;

enum class MiniWndKind : uint8_t {
	Vehicle,
	Station,
	Town,
	Industry,
	Fleet,
	Finance,
	Group,
};

struct MiniWnd {
	MiniWndKind kind = MiniWndKind::Vehicle;
	VehicleID veh = VehicleID::Invalid();
	StationID st = StationID::Invalid();
	TownID town = TownID::Invalid();
	IndustryID ind = IndustryID::Invalid();
	VehicleID sel = VehicleID::Invalid();
	GroupID sel_grp = ALL_GROUP;
	EngineID sel_eng = EngineID::Invalid();
	int16_t sel_ord = -1;
	int x = 0, y = 0;
	uint8_t tab = 0;
	int scroll = 0;
	int rows = 0;
};

static std::vector<MiniWnd> _wnds;
static int _wnd_drag = -1;
static int _wnd_drag_dx = 0, _wnd_drag_dy = 0;
static std::string _wnd_tooltip;

static VehicleID FrontWndVehicle()
{
	if (_wnds.empty() || _wnds.back().kind != MiniWndKind::Vehicle) return VehicleID::Invalid();
	return _wnds.back().veh;
}

enum {
	MWA_CLOSE,
	MWA_RENAME,
	MWA_TAB_BASE = 10,
	MWA_CMD_BASE = 20,
	MWA_ROW_BASE = 100,
};

struct MiniWndRowAct {
	TileIndex jump = INVALID_TILE;
	VehicleID open_veh = VehicleID::Invalid();
	StationID open_st = StationID::Invalid();
	TownID open_town = TownID::Invalid();
	VehicleID mark = VehicleID::Invalid();
	EngineID buy = EngineID::Invalid();
	TileIndex deploy = INVALID_TILE;
	int draft_del = -1;
	int skip_order = -1;
	bool attach = false;
	bool detach = false;
	CargoType refit = INVALID_CARGO;
	int ord_sel = -1;
	int ord_move = 0;
	bool ord_del = false;
	bool ord_load = false;
	bool ord_unload = false;
	bool ord_add = false;
	GroupID grp_sel = GroupID::Invalid();
	VehicleID grp_add = VehicleID::Invalid();
	VehicleID grp_rm = VehicleID::Invalid();
	EngineID repl_from = EngineID::Invalid();
	EngineID repl_to = EngineID::Invalid();
	bool repl_clear = false;
};

struct MiniWndHit {
	size_t wnd;
	Rect r;
	int act;
};

static std::vector<MiniWndHit> _wnd_hits;
static std::vector<MiniWndRowAct> _wnd_row_acts;

static int WndW() { return std::min(250 * _ms.hud_scale, _fbw - 12 * _ms.hud_scale); }
static int WndTitleH() { return GetCharacterHeight(FS_NORMAL) + 8 * _ms.hud_scale; }
static int WndTabH() { return GetCharacterHeight(FS_NORMAL) + 8 * _ms.hud_scale; }
static int WndRowH() { return GetCharacterHeight(FS_NORMAL) + 5 * _ms.hud_scale; }
static int WndViewH() { return 100 * _ms.hud_scale; }
static int WndCmdS() { return 26 * _ms.hud_scale; }
static int WndPad() { return 6 * _ms.hud_scale; }
static int WndBodyH() { return WndViewH() + WndPad() + 6 * WndRowH(); }
static int WndH() { return WndTitleH() + WndTabH() + WndBodyH() + WndCmdS() + 3 * WndPad(); }

static Rect WndFrameRect(const MiniWnd &mw)
{
	return {mw.x, mw.y, mw.x + WndW() - 1, mw.y + WndH() - 1};
}

static void WndText(int x, int y, int rh, std::string_view text, uint32_t tint)
{
	const MiniTextEntry *e = TextTexture(text);
	if (e != nullptr) DrawTextQuad(e, x, y + (rh - e->h) / 2, tint);
}

static void WndTextRight(int x1, int y, int rh, std::string_view text, uint32_t tint)
{
	const MiniTextEntry *e = TextTexture(text);
	if (e != nullptr) DrawTextQuad(e, x1 - e->w + 1, y + (rh - e->h) / 2, tint);
}

static bool WndHover(const Rect &r)
{
	return _cursor.in_window && InRect(r, _cursor.pos.x, _cursor.pos.y);
}

/* Small chrome icon button; returns its rect for hit registration. */
static Rect WndIconTile(int x, int y, int side, bool active, bool enabled)
{
	Rect r = {x, y, x + side - 1, y + side - 1};
	int rad = 3 * _ms.hud_scale / 2 + 1;
	bool hover = enabled && WndHover(r);
	RlwCmdRoundRect(r.left, r.top, r.right, r.bottom, rad, hover ? COL_CH_ACCENT : COL_CH_EDGE);
	int b = _ms.hud_scale;
	RlwCmdRoundRect(r.left + b, r.top + b, r.right - b, r.bottom - b, rad, active ? COL_CH_ACTIVE : COL_CH_TILE);
	return r;
}

static void WndPenGlyph(const Rect &r, uint32_t c)
{
	int in = (r.right - r.left + 1) / 4;
	Rect b = {r.left + in, r.top + in, r.right - in, r.bottom - in};
	RlwCmdLine(b.left + (b.right - b.left) / 4, b.bottom - (b.bottom - b.top) / 4, b.right, b.top, 2 * _ms.hud_scale, c);
	RlwCmdCircle(b.left, b.bottom, _ms.hud_scale + 1, c);
}

static void WndCloseGlyph(const Rect &r, uint32_t c)
{
	int in = (r.right - r.left + 1) / 3;
	Rect b = {r.left + in, r.top + in, r.right - in, r.bottom - in};
	RlwCmdLine(b.left, b.top, b.right, b.bottom, 2 * _ms.hud_scale, c);
	RlwCmdLine(b.left, b.bottom, b.right, b.top, 2 * _ms.hud_scale, c);
}

/* Kinds get separate number ranges; a vehicle and a station sharing one id
 * must not resolve to the same carrier window. */
static constexpr int MW_CARRIER_KIND_STRIDE = 0x1000000;

static WindowNumber MiniCarrierNum(const MiniWnd &mw)
{
	int id;
	switch (mw.kind) {
		case MiniWndKind::Vehicle: id = (int)mw.veh.base(); break;
		case MiniWndKind::Station: id = (int)mw.st.base(); break;
		case MiniWndKind::Town: id = (int)mw.town.base(); break;
		case MiniWndKind::Fleet: id = 0; break;
		case MiniWndKind::Finance: id = 0; break;
		case MiniWndKind::Group: id = 0; break;
		default: id = (int)mw.ind.base(); break;
	}
	return MW_CARRIER_NUM_BASE + (int)mw.kind * MW_CARRIER_KIND_STRIDE + id;
}

static constexpr NWidgetPart _nested_mini_carrier_widgets[] = {
	NWidget(NWID_VIEWPORT, INVALID_COLOUR, 0), SetResize(1, 1), SetFill(1, 1), SetMinimalSize(64, 48),
};

static WindowDesc _mini_carrier_desc(
	WDP_MANUAL, {}, 0, 0,
	WC_EXTRA_VIEWPORT, WC_NONE,
	{},
	_nested_mini_carrier_widgets
);

/* The group window has no per-window entity id, so the rename target rides
 * in this side channel between the pen click and the query result. */
static GroupID _mini_rename_grp = GroupID::Invalid();

/* Frameless native viewport window aligned with a mini window's view slot;
 * it doubles as the query-string parent so renames land somewhere. */
struct MiniCarrierWindow : Window {
	MiniWndKind carry_kind;
	TileIndex focus_tile = INVALID_TILE;

	MiniCarrierWindow(WindowDesc &desc, WindowNumber num, MiniWndKind kind, std::variant<TileIndex, VehicleID> focus) : Window(desc), carry_kind(kind)
	{
		if (std::holds_alternative<TileIndex>(focus)) this->focus_tile = std::get<TileIndex>(focus);
		this->InitNested(num);
		this->GetWidget<NWidgetViewport>(0)->InitializeViewport(this, focus, ZoomLevel::Viewport);
	}

	/* The viewport scroll target is its top-left corner, so growing the
	 * window from its minimal size would drift the view right and down. */
	void OnResize() override
	{
		if (this->viewport == nullptr) return;
		this->GetWidget<NWidgetViewport>(0)->UpdateViewportCoordinates(this);
		if (this->focus_tile != INVALID_TILE) ScrollWindowToTile(this->focus_tile, this, true);
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value()) return;
		int id = (this->window_number - MW_CARRIER_NUM_BASE) % MW_CARRIER_KIND_STRIDE;
		if (this->carry_kind == MiniWndKind::Group) {
			if (Group::IsValidID(_mini_rename_grp)) {
				Command<CMD_ALTER_GROUP>::Post(STR_ERROR_GROUP_CAN_T_RENAME, AlterGroupMode::Rename, _mini_rename_grp, GroupID::Invalid(), *str);
			}
			return;
		}
		if (this->carry_kind == MiniWndKind::Vehicle) {
			const Vehicle *v = Vehicle::GetIfValid(static_cast<VehicleID>(id));
			if (v != nullptr) Command<CMD_RENAME_VEHICLE>::Post(STR_ERROR_CAN_T_RENAME_TRAIN + v->type, v->index, *str);
		} else if (this->carry_kind == MiniWndKind::Station) {
			StationID st = static_cast<StationID>(id);
			if (Station::IsValidID(st)) Command<CMD_RENAME_STATION>::Post(STR_ERROR_CAN_T_RENAME_STATION, st, *str);
		} else if (this->carry_kind == MiniWndKind::Town) {
			TownID town = static_cast<TownID>(id);
			if (Town::IsValidID(town)) Command<CMD_RENAME_TOWN>::Post(STR_ERROR_CAN_T_RENAME_TOWN, town, *str);
		}
	}
};

static Window *EnsureMiniCarrier(const MiniWnd &mw, int x, int y, int w, int h)
{
	Window *cw = FindWindowById(WC_EXTRA_VIEWPORT, MiniCarrierNum(mw));
	if (cw == nullptr) {
		std::variant<TileIndex, VehicleID> focus;
		if (mw.kind == MiniWndKind::Vehicle) {
			const Vehicle *v = Vehicle::GetIfValid(mw.veh);
			if (v == nullptr) return nullptr;
			focus = v->index;
		} else if (mw.kind == MiniWndKind::Station) {
			const Station *st = Station::GetIfValid(mw.st);
			if (st == nullptr) return nullptr;
			focus = st->rect.IsEmpty() ? st->xy : TileXY((st->rect.left + st->rect.right) / 2, (st->rect.top + st->rect.bottom) / 2);
		} else if (mw.kind == MiniWndKind::Town) {
			const Town *t = Town::GetIfValid(mw.town);
			if (t == nullptr) return nullptr;
			focus = t->xy;
		} else if (mw.kind == MiniWndKind::Group) {
			focus = TileXY(Map::SizeX() / 2, Map::SizeY() / 2);
		} else {
			const Industry *i = Industry::GetIfValid(mw.ind);
			if (i == nullptr) return nullptr;
			focus = i->location.GetCenterTile();
		}
		cw = new MiniCarrierWindow(_mini_carrier_desc, MiniCarrierNum(mw), mw.kind, focus);
	}
	if (cw->width != w || cw->height != h) ResizeWindow(cw, w - cw->width, h - cw->height, false);
	if (cw->left != x || cw->top != y) {
		/* The vacated region must repaint too, or its pixels linger in the
		 * screen buffer and smear through other carriers' overlay rects. */
		cw->SetDirty();
		if (cw->viewport != nullptr) {
			cw->viewport->left += x - cw->left;
			cw->viewport->top += y - cw->top;
		}
		cw->left = x;
		cw->top = y;
		cw->SetDirty();
	}
	return cw;
}

static void CloseMiniCarrier(const MiniWnd &mw)
{
	CloseWindowById(WC_EXTRA_VIEWPORT, MiniCarrierNum(mw));
}

static void CloseMiniWnd(size_t i)
{
	CloseMiniCarrier(_wnds[i]);
	_wnds.erase(_wnds.begin() + (ptrdiff_t)i);
	_wnd_drag = -1;
}

/* The carrier rises with its window so native z-order keeps matching the
 * mini window order where slots overlap. */
static void RaiseMiniWnd(size_t i)
{
	std::rotate(_wnds.begin() + (ptrdiff_t)i, _wnds.begin() + (ptrdiff_t)i + 1, _wnds.end());
	BringWindowToFrontById(WC_EXTRA_VIEWPORT, MiniCarrierNum(_wnds.back()));
}

static void OpenMiniWnd(MiniWndKind kind, VehicleID veh, StationID st, TownID town = TownID::Invalid(), IndustryID ind = IndustryID::Invalid())
{
	for (size_t i = 0; i < _wnds.size(); i++) {
		if (_wnds[i].kind == kind && _wnds[i].veh == veh && _wnds[i].st == st && _wnds[i].town == town && _wnds[i].ind == ind) {
			RaiseMiniWnd(i);
			return;
		}
	}
	int s = _ms.hud_scale;
	MiniWnd mw;
	mw.kind = kind;
	mw.veh = veh;
	mw.st = st;
	mw.town = town;
	mw.ind = ind;
	mw.x = Clamp(_fbw - 6 * s - WndW() - (int)_wnds.size() * 20 * s, 0, std::max(0, _fbw - WndW()));
	mw.y = Clamp(_win_bar_bottom + 6 * s + (int)_wnds.size() * 20 * s, 0, std::max(0, _fbh - WndH()));
	_wnds.push_back(mw);
}

static void CloseAllMiniWnds()
{
	for (const MiniWnd &mw : _wnds) CloseMiniCarrier(mw);
	_wnds.clear();
	_wnd_drag = -1;
}

static void OpenFleetMiniWnd(int vt)
{
	OpenMiniWnd(MiniWndKind::Fleet, VehicleID::Invalid(), StationID::Invalid());
	if (vt >= 0 && _wnds.back().tab != (uint8_t)vt) {
		_wnds.back().tab = (uint8_t)vt;
		_wnds.back().scroll = 0;
	}
}

static void OpenFinanceMiniWnd()
{
	OpenMiniWnd(MiniWndKind::Finance, VehicleID::Invalid(), StationID::Invalid());
}

static void OpenGroupMiniWnd()
{
	OpenMiniWnd(MiniWndKind::Group, VehicleID::Invalid(), StationID::Invalid());
}

/* Body row painter: rows share one scroll window; clickable rows register a
 * hit rect and show a link underline plus a hover wash. */
struct MiniWndBody {
	Rect area;
	int rh;
	int scroll;
	int row = 0;
	size_t wnd;
	bool hot;

	bool RowRect(Rect &out) const
	{
		int y = this->area.top + (this->row - this->scroll) * this->rh;
		if (this->row < this->scroll || y + this->rh - 1 > this->area.bottom) return false;
		out = {this->area.left, y, this->area.right, y + this->rh - 1};
		return true;
	}

	void Plain(std::string_view text, uint32_t tint)
	{
		Rect r;
		if (this->RowRect(r)) WndText(r.left, r.top, this->rh, text, tint);
		this->row++;
	}

	void KV(std::string_view label, std::string_view value, uint32_t vtint)
	{
		Rect r;
		if (this->RowRect(r)) {
			WndText(r.left, r.top, this->rh, label, COL_CH_DIM);
			WndTextRight(r.right, r.top, this->rh, value, vtint);
		}
		this->row++;
	}

	void KVLink(std::string_view label, std::string_view value, uint32_t vtint, const MiniWndRowAct &act, uint32_t ltint = COL_CH_DIM)
	{
		Rect r;
		if (this->RowRect(r)) {
			if (this->hot && WndHover(r)) RlwCmdRect(r.left, r.top, r.right, r.bottom, COL_CH_TILE);
			WndText(r.left, r.top, this->rh, label, ltint);
			WndTextRight(r.right, r.top, this->rh, value, vtint);
			const MiniTextEntry *e = TextTexture(value);
			if (e != nullptr) RlwCmdRect(r.right - e->w + 1, r.bottom - 1, r.right, r.bottom - 1, (vtint & 0x00FFFFFFU) | 0x60000000U);
			_wnd_hits.push_back({this->wnd, r, MWA_ROW_BASE + (int)_wnd_row_acts.size()});
			_wnd_row_acts.push_back(act);
		}
		this->row++;
	}

	void Header(std::string_view text)
	{
		Rect r;
		if (this->RowRect(r)) {
			RlwCmdRect(r.left, r.top + 1, r.right, r.bottom - 1, COL_CH_TILE);
			WndText(r.left + 4 * _ms.hud_scale, r.top, this->rh, text, COL_CH_DIM);
		}
		this->row++;
	}

	void Link(std::string_view text, uint32_t tint, const MiniWndRowAct &act)
	{
		Rect r;
		if (this->RowRect(r)) {
			if (this->hot && WndHover(r)) RlwCmdRect(r.left, r.top, r.right, r.bottom, COL_CH_TILE);
			int ind = 4 * _ms.hud_scale;
			const MiniTextEntry *e = TextTexture(text);
			if (e != nullptr) {
				DrawTextQuad(e, r.left + ind, r.top + (this->rh - e->h) / 2, tint);
				RlwCmdRect(r.left + ind, r.bottom - 1, r.left + ind + e->w - 1, r.bottom - 1, (tint & 0x00FFFFFFU) | 0x60000000U);
			}
			_wnd_hits.push_back({this->wnd, r, MWA_ROW_BASE + (int)_wnd_row_acts.size()});
			_wnd_row_acts.push_back(act);
		}
		this->row++;
	}
};

static std::string WndOfficial(StringID str)
{
	return StrMakeValid(GetString(str), {});
}

static StringID OrderLoadStr(OrderLoadType t)
{
	switch (t) {
		case OrderLoadType::FullLoad: return STR_ORDER_DROP_FULL_LOAD_ALL;
		case OrderLoadType::FullLoadAny: return STR_ORDER_DROP_FULL_LOAD_ANY;
		case OrderLoadType::NoLoad: return STR_ORDER_DROP_NO_LOADING;
		default: return STR_ORDER_DROP_LOAD_IF_POSSIBLE;
	}
}

static StringID OrderUnloadStr(OrderUnloadType t)
{
	switch (t) {
		case OrderUnloadType::Unload: return STR_ORDER_DROP_UNLOAD;
		case OrderUnloadType::Transfer: return STR_ORDER_DROP_TRANSFER;
		case OrderUnloadType::NoUnload: return STR_ORDER_DROP_NO_UNLOADING;
		default: return STR_ORDER_DROP_UNLOAD_IF_ACCEPTED;
	}
}

static void DrawVehicleWndBody(MiniWnd &mw, MiniWndBody &body, const Vehicle *v)
{
	switch (mw.tab) {
		case 0: {
			if (v->vehstatus.Test(VehState::Crashed)) {
				body.Plain(WndOfficial(STR_VEHICLE_STATUS_CRASHED), COL_CH_RED);
			} else if (v->vehstatus.Test(VehState::Stopped)) {
				body.Plain(WndOfficial(STR_VEHICLE_STATUS_STOPPED), COL_CH_RED);
			} else if (v->current_order.IsType(OT_GOTO_STATION)) {
				body.Plain(StrMakeValid(GetString(STR_STATION_NAME, v->current_order.GetDestination().ToStationID()), {}), COL_CH_ACCENT);
			} else if (v->current_order.IsType(OT_GOTO_DEPOT)) {
				body.Plain("차고로 이동 중", COL_CH_ACCENT);
			} else {
				body.Plain("-", COL_CH_DIM);
			}
			body.KV("속도", fmt::format("{} / {}", v->GetDisplaySpeed(), v->GetDisplayMaxSpeed()), COL_CH_TEXT);
			body.Plain(StrMakeValid(GetString(STR_VEHICLE_INFO_RELIABILITY_BREAKDOWNS, v->reliability * 100 >> 16, v->breakdowns_since_last_service), {}), COL_CH_TEXT);
			break;
		}

		case 1: {
			bool any = false;
			static std::vector<std::tuple<CargoType, uint, uint>> cargo;
			cargo.clear();
			for (const Vehicle *u = v; u != nullptr; u = u->Next()) {
				if (u->cargo_cap == 0 || !IsValidCargoType(u->cargo_type)) continue;
				auto it = std::find_if(cargo.begin(), cargo.end(), [&](const auto &e) { return std::get<0>(e) == u->cargo_type; });
				if (it == cargo.end()) it = cargo.emplace(cargo.end(), u->cargo_type, 0, 0);
				std::get<1>(*it) += u->cargo_cap;
				std::get<2>(*it) += u->cargo.StoredCount();
			}
			for (const auto &[ct, cap, stored] : cargo) {
				any = true;
				body.KV(WndOfficial(CargoSpec::Get(ct)->name), fmt::format("{} / {}", stored, cap), COL_CH_TEXT);
			}
			if (!any) body.Plain("적재 화물 없음", COL_CH_DIM);
			/* Refitting needs the whole consist parked in a depot, same as
			 * the stock refit window. */
			if (v->owner == _local_company && v->IsStoppedInDepot()) {
				CargoTypes mask = 0;
				for (const Vehicle *u = v; u != nullptr; u = u->Next()) {
					mask |= u->GetEngine()->info.refit_mask;
				}
				bool any_ref = false;
				for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
					if (!HasBit(mask, cs->Index())) continue;
					if (!any_ref) {
						body.Header("개조");
						any_ref = true;
					}
					bool cur = false;
					for (const Vehicle *u = v; u != nullptr; u = u->Next()) {
						if (u->cargo_cap > 0 && u->cargo_type == cs->Index()) cur = true;
					}
					MiniWndRowAct act;
					act.refit = cs->Index();
					body.Link(fmt::format("{}{}", cur ? "▶ " : "· ", WndOfficial(cs->name)), cur ? COL_CH_ACCENT : COL_CH_TEXT, act);
				}
			}
			break;
		}

		case 2: {
			bool own = v->owner == _local_company;
			int nord = v->GetNumOrders();
			if (mw.sel_ord >= nord) mw.sel_ord = -1;
			if (nord == 0) body.Plain("주문 없음", COL_CH_DIM);
			int oi = 0;
			for (const Order &o : v->Orders()) {
				std::string label;
				switch (o.GetType()) {
					case OT_GOTO_STATION: label = StrMakeValid(GetString(STR_STATION_NAME, o.GetDestination().ToStationID()), {}); break;
					case OT_GOTO_WAYPOINT: label = StrMakeValid(GetString(STR_WAYPOINT_NAME, o.GetDestination().ToStationID()), {}); break;
					case OT_GOTO_DEPOT: label = "차고"; break;
					case OT_CONDITIONAL: label = fmt::format("조건 {}번", o.GetConditionSkipToOrder() + 1); break;
					default: break;
				}
				if (!label.empty()) {
					if (o.IsType(OT_GOTO_STATION)) {
						switch (o.GetLoadType()) {
							case OrderLoadType::FullLoad:
							case OrderLoadType::FullLoadAny: label += " · 만재"; break;
							case OrderLoadType::NoLoad: label += " · 무적재"; break;
							default: break;
						}
						switch (o.GetUnloadType()) {
							case OrderUnloadType::Unload: label += " · 강제 하차"; break;
							case OrderUnloadType::Transfer: label += " · 환승"; break;
							case OrderUnloadType::NoUnload: label += " · 무하차"; break;
							default: break;
						}
					}
					bool cur = oi == v->cur_real_order_index;
					MiniWndRowAct act;
					act.ord_sel = oi;
					body.Link(fmt::format("{}{}. {}", cur ? "▶ " : "", oi + 1, label), mw.sel_ord == oi ? COL_CH_ACCENT : (cur ? COL_CH_YELLOW : COL_CH_TEXT), act);
				}
				oi++;
			}
			if (own && mw.sel_ord >= 0) {
				const Order *so = v->GetOrder((VehicleOrderID)mw.sel_ord);
				if (so != nullptr) {
					body.Header(fmt::format("{}번 주문", mw.sel_ord + 1));
					MiniWndRowAct up;
					up.ord_move = -1;
					body.Link("위로", COL_CH_TEXT, up);
					MiniWndRowAct dn;
					dn.ord_move = 1;
					body.Link("아래로", COL_CH_TEXT, dn);
					MiniWndRowAct sk;
					sk.skip_order = mw.sel_ord;
					body.Link("여기로 건너뛰기", COL_CH_TEXT, sk);
					if (so->IsType(OT_GOTO_STATION)) {
						MiniWndRowAct ld;
						ld.ord_load = true;
						body.KVLink("적재", WndOfficial(OrderLoadStr(so->GetLoadType())), COL_CH_TEXT, ld);
						MiniWndRowAct ul;
						ul.ord_unload = true;
						body.KVLink("하차", WndOfficial(OrderUnloadStr(so->GetUnloadType())), COL_CH_TEXT, ul);
					}
					MiniWndRowAct del;
					del.ord_del = true;
					body.Link("삭제", COL_CH_RED, del);
				}
			}
			if (own) {
				MiniWndRowAct add;
				add.ord_add = true;
				if (_order_pick_veh == v->index) {
					body.Link("추가 중. 지도에서 목적지 클릭, ESC 종료", COL_CH_ACCENT, add);
				} else {
					body.Link("+ 목적지 추가", COL_CH_ACCENT, add);
				}
			}
			break;
		}

		case 3: {
			body.KV("구매", fmt::format("{}년", v->build_year.base()), COL_CH_TEXT);
			Money value = 0;
			for (const Vehicle *u = v; u != nullptr; u = u->Next()) value += u->value;
			body.KV("가치", GetString(STR_JUST_CURRENCY_LONG, value), COL_CH_TEXT);
			body.KV("유지비", fmt::format("{}/년", GetString(STR_JUST_CURRENCY_LONG, v->GetDisplayRunningCost())), COL_CH_TEXT);
			body.KV("차령", fmt::format("{}년 / {}년", v->age.base() / 366, v->max_age.base() / 366), COL_CH_TEXT);
			body.Plain(StrMakeValid(GetString(STR_VEHICLE_INFO_PROFIT_THIS_YEAR_LAST_YEAR, v->GetDisplayProfitThisYear(), v->GetDisplayProfitLastYear()), {}), COL_CH_TEXT);
			if (v->type == VEH_TRAIN) {
				body.KV("총길이", fmt::format("{:.1f}타일", Train::From(v)->gcache.cached_total_length / (double)TILE_SIZE), COL_CH_TEXT);
			}
			if (v->type == VEH_TRAIN || (v->type == VEH_ROAD && _settings_game.vehicle.roadveh_acceleration_model != AM_ORIGINAL)) {
				const GroundVehicleCache *gc = v->GetGroundVehicleCache();
				int64_t ms = PackVelocity(v->GetDisplayMaxSpeed(), v->type);
				if (v->type == VEH_TRAIN && (_settings_game.vehicle.train_acceleration_model == AM_ORIGINAL ||
						Train::From(v)->GetAccelerationType() == VehicleAccelerationModel::Maglev)) {
					body.Plain(StrMakeValid(GetString(STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED, gc->cached_weight, gc->cached_power, ms), {}), COL_CH_TEXT);
				} else {
					body.Plain(StrMakeValid(GetString(STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED_MAX_TE, gc->cached_weight, gc->cached_power, ms, gc->cached_max_te), {}), COL_CH_TEXT);
				}
			}
			break;
		}
	}
}

static void DrawStationWndBody(const MiniWnd &mw, MiniWndBody &body, const Station *st)
{
	switch (mw.tab) {
		case 0: {
			bool any = false;
			for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
				const GoodsEntry &ge = st->goods[cs->Index()];
				if (!ge.HasRating()) continue;
				any = true;
				body.KV(WndOfficial(cs->name), fmt::format("{}", ge.TotalCount()), COL_CH_TEXT);
			}
			if (!any) body.Plain("대기 화물 없음", COL_CH_DIM);
			break;
		}

		case 3: {
			if (st->town != nullptr) {
				MiniWndRowAct act;
				act.open_town = st->town->index;
				body.KVLink("도시", StrMakeValid(GetString(STR_TOWN_NAME, st->town->index), {}), COL_CH_TEXT, act);
			}
			if (Company::IsValidID(st->owner)) {
				body.KV("소유", StrMakeValid(GetString(STR_COMPANY_NAME, st->owner), {}), COL_CH_TEXT);
			}
			std::string fac;
			for (const auto &[f, name] : std::initializer_list<std::pair<StationFacility, std::string_view>>{
					{StationFacility::Train, "철도"}, {StationFacility::BusStop, "버스"}, {StationFacility::TruckStop, "트럭"},
					{StationFacility::Dock, "부두"}, {StationFacility::Airport, "공항"}}) {
				if (!st->facilities.Test(f)) continue;
				if (!fac.empty()) fac += " ";
				fac += name;
			}
			if (!fac.empty()) body.KV("시설", fac, COL_CH_TEXT);
			body.Plain(StrMakeValid(GetString(STR_LAND_AREA_INFORMATION_BUILD_DATE, st->build_date), {}), COL_CH_TEXT);
			if (st->facilities.Test(StationFacility::Train) && st->train_station.tile != INVALID_TILE) {
				uint longest = 0;
				for (TileIndex t : st->train_station) {
					if (!st->TileBelongsToRailStation(t)) continue;
					longest = std::max(longest, st->GetPlatformLength(t));
				}
				body.KV(WndOfficial(STR_STATION_BUILD_PLATFORM_LENGTH), fmt::format("{}칸", longest), COL_CH_TEXT);
			}
			body.Plain(StrMakeValid(GetString(STR_STATION_VIEW_ACCEPTS_CARGO, GetAcceptanceMask(st)), {}), COL_CH_TEXT);
			bool rated = false;
			for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
				const GoodsEntry &ge = st->goods[cs->Index()];
				if (!ge.HasRating()) continue;
				if (!rated) {
					body.Header("화물 처리 평가");
					rated = true;
				}
				uint pct = ToPercent8(ge.rating);
				uint32_t tint = pct < 25 ? COL_CH_RED : pct < 50 ? COL_CH_YELLOW : COL_CH_TEXT;
				body.KV(WndOfficial(cs->name), fmt::format("{} {}%", WndOfficial(STR_CARGO_RATING_APPALLING + (ge.rating >> 5)), pct), tint);
			}
			break;
		}

		case 1: {
			std::vector<const Industry *> supply;
			for (const Industry *i : Industry::Iterate()) {
				if (i->stations_near.find(const_cast<Station *>(st)) == i->stations_near.end()) continue;
				if (std::none_of(std::begin(i->produced), std::end(i->produced), [](const auto &p) { return IsValidCargoType(p.cargo); })) continue;
				supply.push_back(i);
			}
			if (!supply.empty()) {
				body.Header("공급처");
				for (const Industry *i : supply) {
					MiniWndRowAct act;
					act.jump = i->location.tile;
					body.Link(StrMakeValid(GetString(STR_INDUSTRY_NAME, i->index), {}), COL_CH_TEXT, act);
				}
			}
			if (!st->industries_near.empty()) {
				body.Header("납품처");
				for (const IndustryListEntry &e : st->industries_near) {
					MiniWndRowAct act;
					act.jump = e.industry->location.tile;
					body.Link(StrMakeValid(GetString(STR_INDUSTRY_NAME, e.industry->index), {}), COL_CH_TEXT, act);
				}
			}
			if (body.row == 0) body.Plain("주변 산업 없음", COL_CH_DIM);
			break;
		}

		case 2: {
			bool any = false;
			for (VehicleType vt : {VEH_TRAIN, VEH_ROAD, VEH_SHIP, VEH_AIRCRAFT}) {
				VehicleList list;
				if (!GenerateVehicleSortList(&list, VehicleListIdentifier(VL_STATION_LIST, vt, _local_company, st->index))) continue;
				for (const Vehicle *v : list) {
					any = true;
					bool here = v->current_order.IsType(OT_GOTO_STATION) && v->current_order.GetDestination().ToStationID() == st->index;
					MiniWndRowAct act;
					act.open_veh = v->index;
					body.Link(StrMakeValid(GetString(STR_VEHICLE_NAME, v->index), {}), here ? COL_CH_ACCENT : COL_CH_TEXT, act);
				}
			}
			if (!any) body.Plain("이 역에 오는 차량 없음", COL_CH_DIM);
			break;
		}
	}
}

static StringID TownRatingString(int rating)
{
	if (rating > RATING_EXCELLENT) return STR_CARGO_RATING_OUTSTANDING;
	if (rating > RATING_VERYGOOD)  return STR_CARGO_RATING_EXCELLENT;
	if (rating > RATING_GOOD)      return STR_CARGO_RATING_VERY_GOOD;
	if (rating > RATING_MEDIOCRE)  return STR_CARGO_RATING_GOOD;
	if (rating > RATING_POOR)      return STR_CARGO_RATING_MEDIOCRE;
	if (rating > RATING_VERYPOOR)  return STR_CARGO_RATING_POOR;
	if (rating > RATING_APPALLING) return STR_CARGO_RATING_VERY_POOR;
	return STR_CARGO_RATING_APPALLING;
}

static void DrawTownWndBody(const MiniWnd &mw, MiniWndBody &body, const Town *t)
{
	switch (mw.tab) {
		case 0: {
			body.Plain(StrMakeValid(GetString(STR_TOWN_VIEW_POPULATION_HOUSES, t->cache.population, t->cache.num_houses), {}), COL_CH_TEXT);
			if (t->flags.Test(TownFlag::IsGrowing)) {
				StringID str = t->fund_buildings_months == 0 ? STR_TOWN_VIEW_TOWN_GROWS_EVERY : STR_TOWN_VIEW_TOWN_GROWS_EVERY_FUNDED;
				body.Plain(StrMakeValid(GetString(str, RoundDivSU(t->growth_rate + 1, Ticks::DAY_TICKS)), {}), COL_CH_TEXT);
			} else {
				body.Plain(WndOfficial(STR_TOWN_VIEW_TOWN_GROW_STOPPED), COL_CH_YELLOW);
			}
			if (t->larger_town) body.Plain("대도시", COL_CH_ACCENT);
			if (_settings_game.economy.station_noise_level) {
				body.Plain(StrMakeValid(GetString(STR_TOWN_VIEW_NOISE_IN_TOWN, t->noise_reached, t->MaxTownNoise()), {}), COL_CH_TEXT);
			}
			break;
		}

		case 2: {
			StringID str_last = TimerGameEconomy::UsingWallclockUnits() ? STR_TOWN_VIEW_CARGO_LAST_MINUTE_MAX : STR_TOWN_VIEW_CARGO_LAST_MONTH_MAX;
			for (auto tpe : {TPE_PASSENGERS, TPE_MAIL}) {
				for (const CargoSpec *cs : CargoSpec::town_production_cargoes[tpe]) {
					CargoType ct = cs->Index();
					auto it = t->GetCargoSupplied(ct);
					uint transported = it != std::end(t->supplied) ? it->history[LAST_MONTH].transported : 0;
					uint production = it != std::end(t->supplied) ? it->history[LAST_MONTH].production : 0;
					body.Plain(StrMakeValid(GetString(str_last, 1ULL << ct, transported, production), {}), COL_CH_TEXT);
				}
			}
			bool first = true;
			for (int i = TAE_BEGIN; i < TAE_END; i++) {
				if (t->goal[i] == 0) continue;
				if (t->goal[i] == TOWN_GROWTH_WINTER && (TileHeight(t->xy) < LowestSnowLine() || t->cache.population <= 90)) continue;
				if (t->goal[i] == TOWN_GROWTH_DESERT && (GetTropicZone(t->xy) != TROPICZONE_DESERT || t->cache.population <= 60)) continue;
				if (first) {
					body.Header(WndOfficial(STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH));
					first = false;
				}
				const CargoSpec *cargo = FindFirstCargoWithTownAcceptanceEffect((TownAcceptanceEffect)i);
				if (cargo == nullptr) continue;
				if (t->goal[i] == TOWN_GROWTH_DESERT || t->goal[i] == TOWN_GROWTH_WINTER) {
					bool done = t->received[i].old_act > 0;
					body.KV(WndOfficial(cargo->name), done ? "공급됨" : "필요", done ? COL_CH_TEXT : COL_CH_YELLOW);
				} else {
					bool done = t->received[i].old_act >= t->goal[i];
					body.KV(WndOfficial(cargo->name), fmt::format("{} / {}", t->received[i].old_act, t->goal[i]), done ? COL_CH_TEXT : COL_CH_YELLOW);
				}
			}
			break;
		}

		case 1: {
			bool any = false;
			for (const Company *c : Company::Iterate()) {
				if (!t->have_ratings.Test(c->index) && t->exclusivity != c->index) continue;
				any = true;
				int rating = t->ratings[c->index];
				uint32_t tint = rating <= RATING_VERYPOOR ? COL_CH_RED : rating <= RATING_MEDIOCRE ? COL_CH_YELLOW : COL_CH_TEXT;
				std::string name = StrMakeValid(GetString(STR_COMPANY_NAME, c->index), {});
				if (t->exclusivity == c->index) name += " · 독점";
				body.KV(name, WndOfficial(TownRatingString(rating)), tint);
			}
			if (!any) body.Plain("회사 평가 없음", COL_CH_DIM);
			break;
		}
	}
}

static void DrawIndustryWndBody(const MiniWnd &mw, MiniWndBody &body, const Industry *i)
{
	switch (mw.tab) {
		case 0: {
			if (i->prod_level == PRODLEVEL_CLOSURE) body.Plain(WndOfficial(STR_INDUSTRY_VIEW_INDUSTRY_ANNOUNCED_CLOSURE), COL_CH_RED);
			bool any = false;
			for (const auto &p : i->produced) {
				if (!IsValidCargoType(p.cargo)) continue;
				any = true;
				uint pct = ToPercent8(p.history[LAST_MONTH].PctTransported());
				uint32_t tint = pct < 25 ? COL_CH_RED : pct < 50 ? COL_CH_YELLOW : COL_CH_TEXT;
				body.KV(WndOfficial(CargoSpec::Get(p.cargo)->name), fmt::format("{} · {}%", p.history[LAST_MONTH].production, pct), tint);
			}
			if (!any) body.Plain("생산 없음", COL_CH_DIM);
			if (i->prod_level != PRODLEVEL_DEFAULT && i->prod_level != PRODLEVEL_CLOSURE) {
				body.Plain(StrMakeValid(GetString(STR_INDUSTRY_VIEW_PRODUCTION_LEVEL, RoundDivSU(i->prod_level * 100, PRODLEVEL_DEFAULT)), {}), COL_CH_TEXT);
			}
			break;
		}

		case 2: {
			body.Plain(StrMakeValid(GetString(STR_LAND_AREA_INFORMATION_BUILD_DATE, i->construction_date), {}), COL_CH_TEXT);
			bool first = true;
			for (const auto &a : i->accepted) {
				if (!IsValidCargoType(a.cargo)) continue;
				if (first) {
					body.Header(WndOfficial(STR_INDUSTRY_VIEW_REQUIRES));
					first = false;
				}
				body.KV(WndOfficial(CargoSpec::Get(a.cargo)->name), a.waiting > 0 ? fmt::format("{}", a.waiting) : std::string("-"), COL_CH_TEXT);
			}
			break;
		}

		case 1: {
			if (i->stations_near.empty()) {
				body.Plain("주변 역 없음", COL_CH_DIM);
				break;
			}
			for (const Station *st : i->stations_near) {
				MiniWndRowAct act;
				act.open_st = st->index;
				body.Link(StrMakeValid(GetString(STR_STATION_NAME, st->index), {}), COL_CH_TEXT, act);
			}
			break;
		}
	}
}

/* Fleet window: one communal screen per vehicle type. A consist is drafted
 * from the engine list, then produced whole by clicking a depot row; every
 * depot lists its consists for reassembly and selling. */
static void DrawFleetWndBody(MiniWnd &mw, MiniWndBody &body)
{
	VehicleType vt = (VehicleType)mw.tab;
	std::vector<EngineID> &draft = _fleet_draft[mw.tab];
	std::erase_if(draft, [](EngineID eid) {
		const Engine *e = Engine::GetIfValid(eid);
		return e == nullptr || !e->IsEnabled();
	});

	const Vehicle *sv = Vehicle::GetIfValid(mw.sel);
	if (sv != nullptr && (sv->type != vt || !sv->First()->IsChainInDepot())) {
		mw.sel = VehicleID::Invalid();
		sv = nullptr;
	}

	body.Header("설계");
	if (draft.empty()) {
		body.Plain(vt == VEH_TRAIN ? "엔진 목록을 눌러 편성 구성" : "엔진 목록을 눌러 선택", COL_CH_DIM);
	} else {
		Money total = 0;
		for (size_t i = 0; i < draft.size(); i++) {
			const Engine *e = Engine::Get(draft[i]);
			total += e->GetCost();
			MiniWndRowAct act;
			act.draft_del = (int)i;
			body.Link(fmt::format("{}. {}", i + 1, StrMakeValid(GetString(STR_ENGINE_NAME, e->index), {})), COL_CH_TEXT, act);
		}
		body.KV("합계", GetString(STR_JUST_CURRENCY_LONG, total), COL_CH_ACCENT);
		body.Plain("차고 행 클릭으로 생산", COL_CH_DIM);
	}
	if (_deploy.depot != INVALID_TILE && _deploy.vt == vt) {
		body.Plain(fmt::format("생산 중 {} / {}", std::min(_deploy.next + 1, _deploy.units.size()), _deploy.units.size()), COL_CH_ACCENT);
	}

	struct BuyRow {
		EngineID eid;
		std::string name;
		Money cost;
		int64_t score;
	};
	std::vector<BuyRow> locos, wags;
	for (const Engine *e : Engine::IterateType(vt)) {
		if (!e->IsEnabled() || !e->company_avail.Test(_local_company)) continue;
		bool wagon = false;
		int64_t score;
		switch (vt) {
			case VEH_TRAIN: {
				const RailVehicleInfo &rvi = e->VehInfo<RailVehicleInfo>();
				wagon = rvi.railveh_type == RAILVEH_WAGON;
				score = wagon ? e->GetDisplayDefaultCapacity() : e->GetPower();
				break;
			}
			default:
				score = (int64_t)e->GetDisplayDefaultCapacity() * 1000 + e->GetDisplayMaxSpeed();
				break;
		}
		BuyRow r;
		r.eid = e->index;
		r.name = StrMakeValid(GetString(STR_ENGINE_NAME, e->index), {});
		r.cost = e->GetCost();
		r.score = score;
		(wagon ? wags : locos).push_back(std::move(r));
	}
	auto by_score = [](const BuyRow &a, const BuyRow &b) { return a.score > b.score; };
	std::sort(locos.begin(), locos.end(), by_score);
	std::sort(wags.begin(), wags.end(), by_score);
	auto engine_rows = [&](const std::vector<BuyRow> &list) {
		for (const BuyRow &r : list) {
			MiniWndRowAct act;
			act.buy = r.eid;
			body.KVLink(r.name, GetString(STR_JUST_CURRENCY_LONG, r.cost), COL_CH_TEXT, act, COL_CH_TEXT);
		}
	};
	if (vt == VEH_TRAIN) {
		if (!locos.empty()) body.Header("기관차");
		engine_rows(locos);
		if (!wags.empty()) body.Header("화차");
		engine_rows(wags);
	} else {
		body.Header("엔진");
		engine_rows(locos);
	}
	if (locos.empty() && wags.empty()) body.Plain("구매 가능 엔진 없음", COL_CH_DIM);

	body.Header("차고");
	if (sv != nullptr && vt == VEH_TRAIN) {
		body.Plain("표시 차량: 같은 차고 편성 클릭으로 연결", COL_CH_ACCENT);
		if (sv->First() != sv) {
			MiniWndRowAct act;
			act.detach = true;
			body.Link("새 편성으로 분리", COL_CH_ACCENT, act);
		}
	}
	bool anydep = false;
	auto chain_rows = [&](const Vehicle *head) {
		int len = 1;
		if (vt == VEH_TRAIN) {
			len = 0;
			for (const Train *u = Train::From(head); u != nullptr; u = u->GetNextUnit()) len++;
		}
		std::string label = head->IsPrimaryVehicle()
				? StrMakeValid(GetString(STR_VEHICLE_NAME, head->index), {})
				: StrMakeValid(GetString(STR_ENGINE_NAME, head->engine_type), {});
		if (len > 1) label = fmt::format("{} · {}량", label, len);
		label = fmt::format("{}{}", mw.sel == head->index ? "▶ " : "· ", label);
		bool stopped = head->vehstatus.Test(VehState::Stopped);
		MiniWndRowAct act;
		act.mark = head->index;
		act.attach = true;
		body.Link(label, mw.sel == head->index ? COL_CH_ACCENT : (stopped ? COL_CH_TEXT : COL_CH_YELLOW), act);
		if (vt == VEH_TRAIN && len > 1) {
			for (const Train *u = Train::From(head); u != nullptr; u = u->GetNextUnit()) {
				MiniWndRowAct ua;
				ua.mark = u->index;
				std::string ul = fmt::format("{}{}", mw.sel == u->index ? "  ▶ " : "  · ",
						StrMakeValid(GetString(STR_ENGINE_NAME, u->engine_type), {}));
				body.Link(ul, mw.sel == u->index ? COL_CH_ACCENT : COL_CH_DIM, ua);
			}
		}
	};
	auto depot_block = [&](TileIndex tile, uint dest) {
		anydep = true;
		VehicleList chains, wagons;
		BuildDepotVehicleList(vt, tile, &chains, &wagons);
		MiniWndRowAct act;
		act.deploy = tile;
		std::string dn = StrMakeValid(GetString(STR_DEPOT_NAME, vt, dest), {});
		body.Link(draft.empty() ? dn : fmt::format("▶ {} 생산", dn), draft.empty() ? COL_CH_TEXT : COL_CH_ACCENT, act);
		for (const Vehicle *head : chains) chain_rows(head);
		for (const Vehicle *head : wagons) chain_rows(head);
	};
	if (vt == VEH_AIRCRAFT) {
		for (const Station *st : Station::Iterate()) {
			if (st->owner != _local_company || !st->facilities.Test(StationFacility::Airport) || !st->airport.HasHangar()) continue;
			depot_block(st->airport.GetHangarTile(0), st->index.base());
		}
	} else {
		for (const Depot *d : Depot::Iterate()) {
			if (!IsDepotTile(d->xy) || GetDepotVehicleType(d->xy) != vt) continue;
			if (GetTileOwner(d->xy) != _local_company) continue;
			depot_block(d->xy, d->index.base());
		}
	}
	if (!anydep) body.Plain("차고 없음", COL_CH_DIM);
}

/* Stock finance sign convention: positive table values are outgo, negative
 * are income and show with a plus sign. */
static std::string MiniPriceStr(Money amount)
{
	StringID str = STR_FINANCES_NEGATIVE_INCOME;
	if (amount == 0) {
		str = STR_FINANCES_ZERO_INCOME;
	} else if (amount < 0) {
		amount = -amount;
		str = STR_FINANCES_POSITIVE_INCOME;
	}
	return StrMakeValid(GetString(str, amount), {});
}

static void DrawFinanceWndBody(MiniWndBody &body, uint8_t tab)
{
	const Company *c = Company::GetIfValid(_local_company);
	if (c == nullptr) return;

	if (tab == 0) {
		body.KV(WndOfficial(STR_FINANCES_BANK_BALANCE_TITLE), StrMakeValid(GetString(STR_JUST_CURRENCY_LONG, c->money), {}), COL_CH_ACCENT);
		body.KV(WndOfficial(STR_FINANCES_OWN_FUNDS_TITLE), StrMakeValid(GetString(STR_JUST_CURRENCY_LONG, c->money - c->current_loan), {}), COL_CH_TEXT);
		body.KV(WndOfficial(STR_FINANCES_LOAN_TITLE), StrMakeValid(GetString(STR_JUST_CURRENCY_LONG, c->current_loan), {}), c->current_loan > 0 ? COL_CH_YELLOW : COL_CH_TEXT);
		body.Plain(StrMakeValid(GetString(STR_FINANCES_MAX_LOAN, c->GetMaxLoan()), {}), COL_CH_TEXT);
		body.Plain(StrMakeValid(GetString(STR_FINANCES_INTEREST_RATE, _economy.interest_rate), {}), COL_CH_TEXT);
		body.KV("회사 가치", StrMakeValid(GetString(STR_JUST_CURRENCY_LONG, CalculateCompanyValue(c)), {}), COL_CH_TEXT);
		return;
	}

	struct MiniExpCat {
		StringID title;
		std::initializer_list<ExpensesType> items;
	};
	static const MiniExpCat cats[] = {
		{STR_FINANCES_REVENUE_TITLE, {EXPENSES_TRAIN_REVENUE, EXPENSES_ROADVEH_REVENUE, EXPENSES_AIRCRAFT_REVENUE, EXPENSES_SHIP_REVENUE}},
		{STR_FINANCES_OPERATING_EXPENSES_TITLE, {EXPENSES_TRAIN_RUN, EXPENSES_ROADVEH_RUN, EXPENSES_AIRCRAFT_RUN, EXPENSES_SHIP_RUN, EXPENSES_PROPERTY, EXPENSES_LOAN_INTEREST}},
		{STR_FINANCES_CAPITAL_EXPENSES_TITLE, {EXPENSES_CONSTRUCTION, EXPENSES_NEW_VEHICLES, EXPENSES_OTHER}},
	};
	const Expenses &tbl = c->yearly_expenses[0];
	Money total = 0;
	for (const MiniExpCat &cat : cats) {
		body.Header(WndOfficial(cat.title));
		Money sum = 0;
		for (ExpensesType et : cat.items) {
			Money cost = tbl[et];
			sum += cost;
			if (cost == 0) continue;
			body.KV(WndOfficial(STR_FINANCES_SECTION_CONSTRUCTION + et), MiniPriceStr(cost), cost > 0 ? COL_CH_RED : COL_CH_TEXT);
		}
		total += sum;
		body.KV("합계", MiniPriceStr(sum), sum > 0 ? COL_CH_RED : COL_CH_TEXT);
	}
	body.Header(WndOfficial(STR_FINANCES_TOTAL_CAPTION));
	body.KV("올해 손익", MiniPriceStr(total), total > 0 ? COL_CH_RED : COL_CH_ACCENT);
}

/* Group window: one communal screen per vehicle type. A selected group
 * gates the membership lists and scopes the autoreplace rules, with the
 * all-vehicles pseudo group as the default scope. */
static void DrawGroupWndBody(MiniWnd &mw, MiniWndBody &body)
{
	if (!Company::IsValidID(_local_company)) return;
	VehicleType vt = (VehicleType)mw.tab;

	bool special = mw.sel_grp == ALL_GROUP || mw.sel_grp == DEFAULT_GROUP;
	const Group *sg = special ? nullptr : Group::GetIfValid(mw.sel_grp);
	if (!special && (sg == nullptr || sg->owner != _local_company || sg->vehicle_type != vt)) {
		mw.sel_grp = ALL_GROUP;
		sg = nullptr;
		special = true;
	}
	const Engine *se = Engine::GetIfValid(mw.sel_eng);
	if (se != nullptr && se->type != vt) {
		mw.sel_eng = EngineID::Invalid();
		se = nullptr;
	}

	body.Header("그룹");
	auto group_row = [&](GroupID gid, std::string_view name, uint count) {
		MiniWndRowAct act;
		act.grp_sel = gid;
		body.Link(fmt::format("{}{} · {}대", mw.sel_grp == gid ? "▶ " : "· ", name, count),
				mw.sel_grp == gid ? COL_CH_ACCENT : COL_CH_TEXT, act);
	};
	group_row(ALL_GROUP, WndOfficial(STR_GROUP_ALL_TRAINS + vt), GetGroupNumVehicle(_local_company, ALL_GROUP, vt));
	group_row(DEFAULT_GROUP, WndOfficial(STR_GROUP_DEFAULT_TRAINS + vt), GetGroupNumVehicle(_local_company, DEFAULT_GROUP, vt));
	std::vector<const Group *> groups;
	for (const Group *g : Group::Iterate()) {
		if (g->owner != _local_company || g->vehicle_type != vt) continue;
		groups.push_back(g);
	}
	std::sort(groups.begin(), groups.end(), [](const Group *a, const Group *b) { return a->number < b->number; });
	for (const Group *g : groups) {
		group_row(g->index, StrMakeValid(GetString(STR_GROUP_NAME, g->index), {}), GetGroupNumVehicle(_local_company, g->index, vt));
	}

	body.Header("자동 교체");
	const Company *comp = Company::Get(_local_company);
	bool any_used = false;
	for (const Engine *e : Engine::IterateType(vt)) {
		uint num = GetGroupNumEngines(_local_company, mw.sel_grp, e->index);
		EngineID repl = EngineReplacementForCompany(comp, e->index, mw.sel_grp);
		if (num == 0 && repl == EngineID::Invalid()) continue;
		any_used = true;
		std::string label = fmt::format("{}{} · {}대", mw.sel_eng == e->index ? "▶ " : "· ",
				StrMakeValid(GetString(STR_ENGINE_NAME, e->index), {}), num);
		if (repl != EngineID::Invalid()) label += fmt::format(" → {}", StrMakeValid(GetString(STR_ENGINE_NAME, repl), {}));
		MiniWndRowAct act;
		act.repl_from = e->index;
		body.Link(label, mw.sel_eng == e->index ? COL_CH_ACCENT : (repl != EngineID::Invalid() ? COL_CH_YELLOW : COL_CH_TEXT), act);
	}
	if (!any_used) body.Plain("보유 엔진 없음", COL_CH_DIM);
	if (se != nullptr) {
		EngineID repl = EngineReplacementForCompany(comp, mw.sel_eng, mw.sel_grp);
		if (repl != EngineID::Invalid()) {
			MiniWndRowAct act;
			act.repl_clear = true;
			body.Link("교체 해제", COL_CH_RED, act);
		}
		body.Plain("교체할 새 엔진 클릭", COL_CH_DIM);
		for (const Engine *e : Engine::IterateType(vt)) {
			if (e->index == mw.sel_eng) continue;
			if (!CheckAutoreplaceValidity(mw.sel_eng, e->index, _local_company)) continue;
			MiniWndRowAct act;
			act.repl_to = e->index;
			body.Link(fmt::format("· {}", StrMakeValid(GetString(STR_ENGINE_NAME, e->index), {})),
					e->index == repl ? COL_CH_ACCENT : COL_CH_TEXT, act);
		}
	}

	if (special) {
		body.Header("차량");
		bool anyv = false;
		for (const Vehicle *v : Vehicle::Iterate()) {
			if (v->type != vt || !v->IsPrimaryVehicle() || v->owner != _local_company) continue;
			if (mw.sel_grp == DEFAULT_GROUP && v->group_id != DEFAULT_GROUP) continue;
			anyv = true;
			MiniWndRowAct act;
			act.open_veh = v->index;
			body.Link(StrMakeValid(GetString(STR_VEHICLE_NAME, v->index), {}), COL_CH_TEXT, act);
		}
		if (!anyv) body.Plain("차량 없음", COL_CH_DIM);
	} else {
		body.Header("소속 차량. 클릭으로 제외");
		bool anyin = false;
		for (const Vehicle *v : Vehicle::Iterate()) {
			if (v->type != vt || !v->IsPrimaryVehicle() || v->owner != _local_company || v->group_id != mw.sel_grp) continue;
			anyin = true;
			MiniWndRowAct act;
			act.grp_rm = v->index;
			body.Link(StrMakeValid(GetString(STR_VEHICLE_NAME, v->index), {}), COL_CH_TEXT, act);
		}
		if (!anyin) body.Plain("소속 차량 없음", COL_CH_DIM);
		body.Header("클릭으로 추가");
		bool anyout = false;
		for (const Vehicle *v : Vehicle::Iterate()) {
			if (v->type != vt || !v->IsPrimaryVehicle() || v->owner != _local_company || v->group_id == mw.sel_grp) continue;
			anyout = true;
			MiniWndRowAct act;
			act.grp_add = v->index;
			body.Link(StrMakeValid(GetString(STR_VEHICLE_NAME, v->index), {}), COL_CH_TEXT, act);
		}
		if (!anyout) body.Plain("없음", COL_CH_DIM);
	}
}

static void DrawWndCmdIcon(MiniWndKind kind, int cmd, const Rect &r, uint32_t c, bool alt)
{
	int cx = (r.left + r.right) / 2;
	int cy = (r.top + r.bottom) / 2;
	int q = (r.right - r.left + 1) / 4;
	int s = _ms.hud_scale;
	if (kind == MiniWndKind::Vehicle) {
		switch (cmd) {
			case 0:
				if (alt) {
					DrawPlayTriangle(cx - q, cy, 2 * q, q, c);
				} else {
					RlwCmdRect(cx - q, cy - q, cx + q, cy + q, c);
				}
				break;
			case 4:
				RlwCmdCircle(cx, cy, q + s, c);
				RlwCmdCircle(cx, cy, q - s + 1, alt ? COL_CH_ACTIVE : COL_CH_TILE);
				RlwCmdCircle(cx, cy, std::max(1, q / 3), c);
				break;
			case 1:
				RlwCmdRect(cx - q, cy - q / 3, cx + q, cy + q, c);
				RlwCmdTriangle(cx, cy - q, q + s, c);
				break;
			case 2:
				RlwCmdCircle(cx - q / 2, cy - q / 2, q / 2 + s, c);
				RlwCmdCircle(cx - q / 2, cy - q / 2, std::max(1, q / 2 - s), alt ? COL_CH_ACTIVE : COL_CH_TILE);
				RlwCmdLine(cx - q / 4, cy - q / 4, cx + q, cy + q, 2 * s, c);
				break;
			case 3:
				for (int i = -1; i <= 1; i++) {
					RlwCmdCircle(cx - q, cy + i * ((2 * q) / 2), s, c);
					RlwCmdRect(cx - q + 3 * s, cy + i * ((2 * q) / 2) - s / 2, cx + q, cy + i * ((2 * q) / 2) + s / 2 + (s == 1 ? 1 : 0), c);
				}
				break;
		}
	} else if (kind == MiniWndKind::Station) {
		switch (cmd) {
			case 0:
				RlwCmdCircle(cx, cy, q + s, c);
				RlwCmdCircle(cx, cy, q - s, alt ? COL_CH_ACTIVE : COL_CH_TILE);
				RlwCmdCircle(cx, cy, s + 1, c);
				break;
			case 1:
				RlwCmdLine(cx, cy - q - s, cx, cy + q + s, s, c);
				RlwCmdLine(cx - q - s, cy, cx + q + s, cy, s, c);
				RlwCmdCircle(cx, cy, q - s, c);
				RlwCmdCircle(cx, cy, q - 2 * s - 1, alt ? COL_CH_ACTIVE : COL_CH_TILE);
				break;
		}
	} else if (kind == MiniWndKind::Town) {
		switch (cmd) {
			case 0:
				RlwCmdLine(cx - q, cy - q - s, cx - q, cy + q + s, s, c);
				DrawPlayTriangle(cx - q + s, cy - q / 2, 2 * q, q / 2 + s, c);
				break;
			case 1:
				RlwCmdLine(cx, cy - q - s, cx, cy + q + s, s, c);
				RlwCmdLine(cx - q - s, cy, cx + q + s, cy, s, c);
				RlwCmdCircle(cx, cy, q - s, c);
				RlwCmdCircle(cx, cy, q - 2 * s - 1, alt ? COL_CH_ACTIVE : COL_CH_TILE);
				break;
		}
	} else if (kind == MiniWndKind::Finance) {
		switch (cmd) {
			case 0:
				RlwCmdRect(cx - q, cy - s, cx + q, cy + s, c);
				RlwCmdRect(cx - s, cy - q, cx + s, cy + q, c);
				break;
			case 1:
				RlwCmdRect(cx - q, cy - s, cx + q, cy + s, c);
				break;
		}
	} else if (kind == MiniWndKind::Group) {
		switch (cmd) {
			case 0:
				RlwCmdRect(cx - q, cy - q, cx + q, cy - q + s, c);
				RlwCmdRect(cx - q, cy - s / 2, cx + q, cy - s / 2 + s, c);
				RlwCmdRect(cx - q, cy + q - s, cx + q, cy + q, c);
				break;
			case 1:
				RlwCmdLine(cx - q, cy - q, cx + q, cy + q, s, c);
				RlwCmdLine(cx - q, cy + q, cx + q, cy - q, s, c);
				break;
			case 2:
				RlwCmdCircle(cx, cy, q, c);
				RlwCmdCircle(cx, cy, std::max(1, q - s), alt ? COL_CH_ACTIVE : COL_CH_TILE);
				break;
			case 3:
				RlwCmdRect(cx - q + s, cy - q + s, cx + q - s, cy + q - s, c);
				break;
			case 4:
				RlwCmdRect(cx - q, cy - q / 3, cx + q, cy + q, c);
				RlwCmdTriangle(cx, cy - q, q + s, c);
				break;
		}
	} else if (kind == MiniWndKind::Fleet) {
		switch (cmd) {
			case 0:
				RlwCmdLine(cx - q, cy - q, cx + q, cy + q, s, c);
				RlwCmdLine(cx - q, cy + q, cx + q, cy - q, s, c);
				RlwCmdLine(cx - q, cy + q + s, cx + q, cy + q + s, s, c);
				break;
			case 1:
				RlwCmdRect(cx - q, cy - q, cx + q, cy - q + s, c);
				RlwCmdRect(cx - q, cy + q - s, cx + q, cy + q, c);
				RlwCmdRect(cx - q, cy - q, cx - q + s, cy + q, c);
				RlwCmdRect(cx + q - s, cy - q, cx + q, cy + q, c);
				RlwCmdLine(cx - q + 2 * s, cy + q - 2 * s, cx + q - 2 * s, cy - q + 2 * s, s, c);
				break;
			case 2:
				RlwCmdRect(cx - q, cy - q, cx + s, cy + s, c);
				RlwCmdRect(cx - s, cy - s, cx + q, cy + q, c);
				break;
		}
	} else {
		switch (cmd) {
			case 0:
				RlwCmdLine(cx - q, cy, cx + q, cy, s, c);
				RlwCmdCircle(cx - q, cy, q / 2 + s, c);
				RlwCmdCircle(cx, cy, q / 2 + s, c);
				RlwCmdCircle(cx + q, cy, q / 2 + s, c);
				break;
			case 1:
				RlwCmdLine(cx, cy - q - s, cx, cy + q + s, s, c);
				RlwCmdLine(cx - q - s, cy, cx + q + s, cy, s, c);
				RlwCmdCircle(cx, cy, q - s, c);
				RlwCmdCircle(cx, cy, q - 2 * s - 1, alt ? COL_CH_ACTIVE : COL_CH_TILE);
				break;
		}
	}
}

static void DrawMiniWnd(MiniWnd &mw, size_t idx, bool hot)
{
	int s = _ms.hud_scale;
	int w = WndW();
	int pad = WndPad();
	Rect fr = WndFrameRect(mw);

	const Vehicle *v = mw.kind == MiniWndKind::Vehicle ? Vehicle::GetIfValid(mw.veh) : nullptr;
	const Station *st = mw.kind == MiniWndKind::Station ? (Station::IsValidID(mw.st) ? Station::Get(mw.st) : nullptr) : nullptr;
	const Town *t = mw.kind == MiniWndKind::Town ? Town::GetIfValid(mw.town) : nullptr;
	const Industry *ind = mw.kind == MiniWndKind::Industry ? Industry::GetIfValid(mw.ind) : nullptr;

	ChromePanel(fr.left, fr.top, fr.right, fr.bottom);

	/* Title bar: name, pen, close. */
	int th = WndTitleH();
	std::string title = "-";
	switch (mw.kind) {
		case MiniWndKind::Vehicle: if (v != nullptr) title = StrMakeValid(GetString(STR_VEHICLE_NAME, v->index), {}); break;
		case MiniWndKind::Station: if (st != nullptr) title = StrMakeValid(GetString(STR_STATION_NAME, st->index), {}); break;
		case MiniWndKind::Town: if (t != nullptr) title = StrMakeValid(GetString(STR_TOWN_NAME, t->index), {}); break;
		case MiniWndKind::Fleet: title = "차고"; break;
		case MiniWndKind::Finance: title = "재정"; break;
		case MiniWndKind::Group: title = "차량군"; break;
		default: if (ind != nullptr) title = StrMakeValid(GetString(STR_INDUSTRY_NAME, ind->index), {}); break;
	}
	WndText(fr.left + pad, fr.top + 2 * s, th, title, COL_CH_TEXT);

	int ts = th - 4 * s;
	Rect close_r = WndIconTile(fr.right - pad - ts + 1, fr.top + 2 * s, ts, false, true);
	WndCloseGlyph(close_r, WndHover(close_r) ? COL_CH_ACCENT : COL_CH_TEXT);
	bool own = (mw.kind == MiniWndKind::Vehicle && v != nullptr && v->owner == _local_company) ||
			(mw.kind == MiniWndKind::Station && st != nullptr && st->owner == _local_company) ||
			(mw.kind == MiniWndKind::Town && t != nullptr) ||
			(mw.kind == MiniWndKind::Fleet && Company::IsValidID(_local_company)) ||
			(mw.kind == MiniWndKind::Finance && Company::IsValidID(_local_company)) ||
			(mw.kind == MiniWndKind::Group && Company::IsValidID(_local_company));
	bool pen_ok;
	switch (mw.kind) {
		case MiniWndKind::Fleet:
		case MiniWndKind::Finance: pen_ok = false; break;
		case MiniWndKind::Group: pen_ok = own && Group::IsValidID(mw.sel_grp); break;
		default: pen_ok = own; break;
	}
	Rect pen_r = WndIconTile(close_r.left - 2 * s - ts, fr.top + 2 * s, ts, false, pen_ok);
	WndPenGlyph(pen_r, pen_ok ? (WndHover(pen_r) ? COL_CH_ACCENT : COL_CH_TEXT) : COL_CH_DIM);
	_wnd_hits.push_back({idx, close_r, MWA_CLOSE});
	if (pen_ok) _wnd_hits.push_back({idx, pen_r, MWA_RENAME});
	if (hot && WndHover(pen_r) && pen_ok) _wnd_tooltip = "이름 변경";
	RlwCmdRect(fr.left + 1, fr.top + th + 2 * s, fr.right - 1, fr.top + th + 2 * s, COL_CH_EDGE);

	/* Uniform-width tab strip. */
	int tab_y = fr.top + th + 2 * s + 1;
	int tabh = WndTabH();
	int ntab;
	switch (mw.kind) {
		case MiniWndKind::Station:
		case MiniWndKind::Vehicle:
		case MiniWndKind::Fleet:
		case MiniWndKind::Group: ntab = 4; break;
		case MiniWndKind::Finance: ntab = 2; break;
		default: ntab = 3; break;
	}
	for (int ti = 0; ti < ntab; ti++) {
		std::string label;
		if (mw.kind == MiniWndKind::Fleet || mw.kind == MiniWndKind::Group) {
			static const StringID type_strs[] = {STR_REPLACE_VEHICLE_TRAIN, STR_REPLACE_VEHICLE_ROAD_VEHICLE, STR_REPLACE_VEHICLE_SHIP, STR_REPLACE_VEHICLE_AIRCRAFT};
			label = WndOfficial(type_strs[ti]);
		} else if (mw.kind == MiniWndKind::Finance) {
			label = ti == 0 ? "개요" : "손익";
		} else if (mw.kind == MiniWndKind::Vehicle) {
			switch (ti) {
				case 0: label = "상태"; break;
				case 1: label = WndOfficial(STR_VEHICLE_DETAIL_TAB_CARGO); break;
				case 2: label = "주문"; break;
				case 3: label = WndOfficial(STR_VEHICLE_DETAIL_TAB_INFORMATION); break;
			}
		} else if (mw.kind == MiniWndKind::Station) {
			switch (ti) {
				case 0: label = "상태"; break;
				case 1: label = WndOfficial(STR_SMALLMAP_TYPE_INDUSTRIES); break;
				case 2: label = WndOfficial(STR_SMALLMAP_TYPE_VEHICLES); break;
				case 3: label = WndOfficial(STR_VEHICLE_DETAIL_TAB_INFORMATION); break;
			}
		} else if (mw.kind == MiniWndKind::Town) {
			switch (ti) {
				case 0: label = "상태"; break;
				case 1: label = "평판"; break;
				case 2: label = WndOfficial(STR_VEHICLE_DETAIL_TAB_INFORMATION); break;
			}
		} else {
			switch (ti) {
				case 0: label = "상태"; break;
				case 1: label = "역"; break;
				case 2: label = WndOfficial(STR_VEHICLE_DETAIL_TAB_INFORMATION); break;
			}
		}
		int x0 = fr.left + 1 + ti * (w - 2) / ntab;
		int x1 = fr.left + (ti + 1) * (w - 2) / ntab;
		Rect tr = {x0, tab_y, x1, tab_y + tabh - 1};
		bool active = mw.tab == ti;
		bool hover = hot && WndHover(tr);
		RlwCmdRect(tr.left, tr.top, tr.right, tr.bottom, active ? COL_CH_ACTIVE : (hover ? COL_CH_TILE : COL_CH_PANEL));
		const MiniTextEntry *e = TextTexture(label);
		if (e != nullptr) DrawTextQuad(e, (tr.left + tr.right - e->w) / 2, tr.top + (tabh - e->h) / 2, active ? COL_CH_ACCENT : COL_CH_TEXT);
		_wnd_hits.push_back({idx, tr, MWA_TAB_BASE + ti});
	}
	RlwCmdRect(fr.left + 1, tab_y + tabh, fr.right - 1, tab_y + tabh, COL_CH_EDGE);

	/* Body: the status tab leads with the live viewport slot. */
	int body_y = tab_y + tabh + 1 + pad;
	Rect body = {fr.left + pad, body_y, fr.right - pad, body_y + WndBodyH() - 1};
	MiniWndBody bp;
	bp.rh = WndRowH();
	bp.scroll = mw.scroll;
	bp.wnd = idx;
	bp.hot = hot;
	bp.area = body;

	if (mw.tab == 0 && mw.kind != MiniWndKind::Fleet && mw.kind != MiniWndKind::Finance && mw.kind != MiniWndKind::Group) {
		Rect vs = {body.left, body.top, body.right, body.top + WndViewH() - 1};
		RlwCmdRect(vs.left, vs.top, vs.right, vs.bottom, 0xFF101010U);
		if (v != nullptr || st != nullptr || t != nullptr || ind != nullptr) {
			EnsureMiniCarrier(mw, vs.left, vs.top, vs.right - vs.left + 1, vs.bottom - vs.top + 1);
		}
		bp.area.top = vs.bottom + 1 + pad;
	}

	if (v != nullptr) DrawVehicleWndBody(mw, bp, v);
	if (st != nullptr) DrawStationWndBody(mw, bp, st);
	if (t != nullptr) DrawTownWndBody(mw, bp, t);
	if (ind != nullptr) DrawIndustryWndBody(mw, bp, ind);
	if (mw.kind == MiniWndKind::Fleet) DrawFleetWndBody(mw, bp);
	if (mw.kind == MiniWndKind::Finance) DrawFinanceWndBody(bp, mw.tab);
	if (mw.kind == MiniWndKind::Group) DrawGroupWndBody(mw, bp);

	/* Scroll clamp and position mark. */
	int vis_rows = (bp.area.bottom - bp.area.top + 1) / bp.rh;
	mw.rows = bp.row;
	mw.scroll = Clamp(mw.scroll, 0, std::max(0, bp.row - vis_rows));
	if (bp.row > vis_rows && vis_rows > 0) {
		int track_h = bp.area.bottom - bp.area.top + 1;
		int ty0 = bp.area.top + track_h * mw.scroll / bp.row;
		int ty1 = bp.area.top + track_h * std::min(bp.row, mw.scroll + vis_rows) / bp.row - 1;
		RlwCmdRect(fr.right - pad + 2 * s, ty0, fr.right - pad + 3 * s - 1, ty1, COL_CH_DIM);
	}

	/* Bottom command row: square icon tiles. */
	int cs2 = WndCmdS();
	int cmd_y = fr.bottom - pad - cs2 + 1;
	RlwCmdRect(fr.left + 1, cmd_y - pad / 2 - 1, fr.right - 1, cmd_y - pad / 2 - 1, COL_CH_EDGE);
	int ncmd;
	switch (mw.kind) {
		case MiniWndKind::Vehicle: ncmd = 5; break;
		case MiniWndKind::Fleet: ncmd = 3; break;
		case MiniWndKind::Group: ncmd = 5; break;
		default: ncmd = 2; break;
	}
	extern const Station *_viewport_highlight_station;
	for (int c = 0; c < ncmd; c++) {
		int cx = fr.left + pad + c * (cs2 + 2 * s);
		bool active = (mw.kind == MiniWndKind::Station && c == 0 && st != nullptr && _viewport_highlight_station == st) ||
				(mw.kind == MiniWndKind::Vehicle && c == 4 && v != nullptr && _follow_veh == v->index);
		bool enabled;
		switch (mw.kind) {
			case MiniWndKind::Station: enabled = st != nullptr && (c == 1 || own || st->owner == OWNER_NONE); break;
			case MiniWndKind::Town: enabled = t != nullptr; break;
			case MiniWndKind::Industry: enabled = ind != nullptr; break;
			case MiniWndKind::Fleet: enabled = own && (c == 1 ? !_fleet_draft[mw.tab].empty() : Vehicle::GetIfValid(mw.sel) != nullptr); break;
			case MiniWndKind::Finance: {
				const Company *fc = Company::GetIfValid(_local_company);
				enabled = fc != nullptr && (c == 0 ? fc->current_loan < fc->GetMaxLoan() : fc->current_loan > 0);
				break;
			}
			case MiniWndKind::Group: enabled = own && (c != 1 || Group::IsValidID(mw.sel_grp)); break;
			default: enabled = v != nullptr && (c == 4 || own); break;
		}
		Rect cr = WndIconTile(cx, cmd_y, cs2, active, enabled);
		bool alt = active || (mw.kind == MiniWndKind::Vehicle && c == 0 && v != nullptr && v->vehstatus.Test(VehState::Stopped));
		uint32_t ic = enabled ? (hot && WndHover(cr) ? COL_CH_ACCENT : COL_CH_TEXT) : COL_CH_DIM;
		DrawWndCmdIcon(mw.kind, c, cr, ic, alt);
		if (enabled) _wnd_hits.push_back({idx, cr, MWA_CMD_BASE + c});
		if (hot && WndHover(cr)) {
			static const std::string_view veh_tips[] = {"", "차고로", "", "주문 창", "따라가기"};
			if (mw.kind == MiniWndKind::Vehicle) {
				switch (c) {
					case 0: _wnd_tooltip = v != nullptr && v->vehstatus.Test(VehState::Stopped) ? WndOfficial(STR_VEHICLE_COMMAND_STARTED) : WndOfficial(STR_VEHICLE_COMMAND_STOPPED); break;
					case 2: _wnd_tooltip = WndOfficial(STR_ORDER_REFIT); break;
					default: _wnd_tooltip = veh_tips[c]; break;
				}
			} else if (mw.kind == MiniWndKind::Station) {
				_wnd_tooltip = c == 0 ? WndOfficial(STR_BUTTON_CATCHMENT) : WndOfficial(STR_STATION_VIEW_CENTER_TOOLTIP);
			} else if (mw.kind == MiniWndKind::Town) {
				_wnd_tooltip = c == 0 ? WndOfficial(STR_TOWN_VIEW_LOCAL_AUTHORITY_TOOLTIP) : WndOfficial(STR_TOWN_VIEW_CENTER_TOOLTIP);
			} else if (mw.kind == MiniWndKind::Fleet) {
				switch (c) {
					case 0: _wnd_tooltip = "표시한 차량 매각"; break;
					case 1: _wnd_tooltip = "설계 비우기"; break;
					case 2: _wnd_tooltip = "표시 편성 복제"; break;
				}
			} else if (mw.kind == MiniWndKind::Finance) {
				_wnd_tooltip = StrMakeValid(GetString(c == 0 ? STR_FINANCES_BORROW_BUTTON : STR_FINANCES_REPAY_BUTTON, LOAN_INTERVAL), {});
			} else if (mw.kind == MiniWndKind::Group) {
				switch (c) {
					case 0: _wnd_tooltip = "새 그룹"; break;
					case 1: _wnd_tooltip = "선택한 그룹 삭제"; break;
					case 2: _wnd_tooltip = "범위 전체 출발"; break;
					case 3: _wnd_tooltip = "범위 전체 정지"; break;
					case 4: _wnd_tooltip = "범위 전체 차고로"; break;
				}
			} else {
				_wnd_tooltip = c == 0 ? WndOfficial(STR_INDUSTRY_DISPLAY_CHAIN) : WndOfficial(STR_INDUSTRY_VIEW_LOCATION_TOOLTIP);
			}
		}
	}
}

static void DrawMiniWnds()
{
	_wnd_hits.clear();
	_wnd_row_acts.clear();
	_wnd_tooltip.clear();

	/* Dead entities close their windows. */
	for (size_t i = _wnds.size(); i-- > 0;) {
		bool alive;
		switch (_wnds[i].kind) {
			case MiniWndKind::Vehicle: alive = Vehicle::GetIfValid(_wnds[i].veh) != nullptr; break;
			case MiniWndKind::Station: alive = Station::IsValidID(_wnds[i].st); break;
			case MiniWndKind::Town: alive = Town::IsValidID(_wnds[i].town); break;
			case MiniWndKind::Fleet: alive = true; break;
			case MiniWndKind::Finance: alive = Company::IsValidID(_local_company); break;
			case MiniWndKind::Group: alive = Company::IsValidID(_local_company); break;
			default: alive = Industry::IsValidID(_wnds[i].ind); break;
		}
		if (!alive) CloseMiniWnd(i);
	}

	size_t hot = SIZE_MAX;
	for (size_t i = _wnds.size(); i-- > 0;) {
		if (InRect(WndFrameRect(_wnds[i]), _cursor.pos.x, _cursor.pos.y)) {
			hot = i;
			break;
		}
	}
	for (size_t i = 0; i < _wnds.size(); i++) {
		DrawMiniWnd(_wnds[i], i, i == hot);
	}

	if (!_wnd_tooltip.empty()) {
		int s = _ms.hud_scale;
		DrawHudText(_cursor.pos.x + 9 * s, _cursor.pos.y + 11 * s, _wnd_tooltip);
	}
}

static bool HandleWndClick(int x, int y)
{
	for (size_t i = _wnds.size(); i-- > 0;) {
		Rect fr = WndFrameRect(_wnds[i]);
		if (!InRect(fr, x, y)) continue;
		if (i + 1 != _wnds.size()) RaiseMiniWnd(i);

		/* Hit rects come from the last draw, keyed by pre-raise index, so
		 * the first click on a background window both raises and acts. */
		for (const auto &[owner, r, act] : _wnd_hits) {
			if (owner != i || !InRect(r, x, y)) continue;
			MiniWnd &mw = _wnds.back();
			const Vehicle *v = mw.kind == MiniWndKind::Vehicle ? Vehicle::GetIfValid(mw.veh) : nullptr;
			const Station *st = mw.kind == MiniWndKind::Station ? (Station::IsValidID(mw.st) ? Station::Get(mw.st) : nullptr) : nullptr;
			const Town *t = mw.kind == MiniWndKind::Town ? Town::GetIfValid(mw.town) : nullptr;
			const Industry *ind = mw.kind == MiniWndKind::Industry ? Industry::GetIfValid(mw.ind) : nullptr;

			if (act == MWA_CLOSE) {
				CloseMiniWnd(_wnds.size() - 1);
			} else if (act == MWA_RENAME) {
				Window *cw = EnsureMiniCarrier(mw, -10000, -10000, 64, 48);
				if (cw != nullptr) {
					if (mw.kind == MiniWndKind::Vehicle && v != nullptr) {
						ShowQueryString(GetString(STR_VEHICLE_NAME, v->index), STR_QUERY_RENAME_TRAIN_CAPTION + v->type,
								MAX_LENGTH_VEHICLE_NAME_CHARS, cw, CS_ALPHANUMERAL, {QueryStringFlag::EnableDefault, QueryStringFlag::LengthIsInChars});
					} else if (st != nullptr) {
						ShowQueryString(GetString(STR_STATION_NAME, st->index), STR_STATION_VIEW_EDIT_STATION_SIGN,
								MAX_LENGTH_STATION_NAME_CHARS, cw, CS_ALPHANUMERAL, {QueryStringFlag::EnableDefault, QueryStringFlag::LengthIsInChars});
					} else if (t != nullptr) {
						ShowQueryString(GetString(STR_TOWN_NAME, t->index), STR_TOWN_VIEW_RENAME_TOWN_BUTTON,
								MAX_LENGTH_TOWN_NAME_CHARS, cw, CS_ALPHANUMERAL, {QueryStringFlag::EnableDefault, QueryStringFlag::LengthIsInChars});
					} else if (mw.kind == MiniWndKind::Group && Group::IsValidID(mw.sel_grp)) {
						_mini_rename_grp = mw.sel_grp;
						ShowQueryString(GetString(STR_GROUP_NAME, mw.sel_grp), STR_GROUP_RENAME_CAPTION,
								MAX_LENGTH_GROUP_NAME_CHARS, cw, CS_ALPHANUMERAL, {QueryStringFlag::EnableDefault, QueryStringFlag::LengthIsInChars});
					}
				}
			} else if (act >= MWA_TAB_BASE && act < MWA_CMD_BASE) {
				if (mw.tab != act - MWA_TAB_BASE) {
					mw.tab = (uint8_t)(act - MWA_TAB_BASE);
					mw.scroll = 0;
					if (mw.tab != 0) CloseMiniCarrier(mw);
				}
			} else if (act >= MWA_CMD_BASE && act < MWA_ROW_BASE) {
				int c = act - MWA_CMD_BASE;
				if (mw.kind == MiniWndKind::Vehicle && v != nullptr) {
					switch (c) {
						case 0: Command<CMD_START_STOP_VEHICLE>::Post(STR_ERROR_CAN_T_STOP_START_TRAIN + v->type, v->tile, v->index, false); break;
						case 1: Command<CMD_SEND_VEHICLE_TO_DEPOT>::Post(GetCmdSendToDepotMsg(v), v->index, _ctrl_pressed ? DepotCommandFlag::Service : DepotCommandFlags{}, {}); break;
						case 2: {
							Window *cw = EnsureMiniCarrier(mw, -10000, -10000, 64, 48);
							ShowVehicleRefitWindow(v, INVALID_VEH_ORDER_ID, cw);
							break;
						}
						case 3: ShowOrdersWindow(v); break;
						case 4:
							if (_follow_veh == v->index) EnterIdleMode(); else EnterFollowMode(v->index);
							break;
					}
				} else if (st != nullptr) {
					extern const Station *_viewport_highlight_station;
					switch (c) {
						case 0: SetViewportCatchmentStation(st, _viewport_highlight_station != st); break;
						case 1: MiniUiScrollTo(TileX(st->xy) * TILE_SIZE, TileY(st->xy) * TILE_SIZE); break;
					}
				} else if (t != nullptr) {
					extern void ShowTownAuthorityWindow(uint town);
					switch (c) {
						case 0: ShowTownAuthorityWindow(t->index.base()); break;
						case 1: MiniUiScrollTo(TileX(t->xy) * TILE_SIZE, TileY(t->xy) * TILE_SIZE); break;
					}
				} else if (ind != nullptr) {
					extern void ShowIndustryCargoesWindow(IndustryType id);
					switch (c) {
						case 0: ShowIndustryCargoesWindow(ind->type); break;
						case 1: {
							TileIndex ct = ind->location.GetCenterTile();
							MiniUiScrollTo(TileX(ct) * TILE_SIZE, TileY(ct) * TILE_SIZE);
							break;
						}
					}
				} else if (mw.kind == MiniWndKind::Fleet) {
					if (c == 0) {
						const Vehicle *sv = Vehicle::GetIfValid(mw.sel);
						if (sv != nullptr) {
							bool chain = sv->type == VEH_TRAIN && sv->First() == sv;
							Command<CMD_SELL_VEHICLE>::Post(GetCmdSellVehMsg(sv->type), sv->tile, sv->index, chain, true, INVALID_CLIENT_ID);
							mw.sel = VehicleID::Invalid();
						}
					} else if (c == 1) {
						_fleet_draft[mw.tab].clear();
					} else {
						const Vehicle *sv = Vehicle::GetIfValid(mw.sel);
						if (sv != nullptr) {
							Command<CMD_CLONE_VEHICLE>::Post(GetCmdBuildVehMsg(sv->type), sv->tile, sv->First()->index, false);
						}
					}
				} else if (mw.kind == MiniWndKind::Finance) {
					if (c == 0) {
						Command<CMD_INCREASE_LOAN>::Post(STR_ERROR_CAN_T_BORROW_ANY_MORE_MONEY, LoanCommand::Interval, 0);
					} else {
						Command<CMD_DECREASE_LOAN>::Post(STR_ERROR_CAN_T_REPAY_LOAN, LoanCommand::Interval, 0);
					}
				} else if (mw.kind == MiniWndKind::Group) {
					VehicleListIdentifier vli(VL_GROUP_LIST, (VehicleType)mw.tab, _local_company, mw.sel_grp);
					if (c == 0) {
						Command<CMD_CREATE_GROUP>::Post(STR_ERROR_GROUP_CAN_T_CREATE, (VehicleType)mw.tab, GroupID::Invalid());
					} else if (c == 1) {
						if (Group::IsValidID(mw.sel_grp)) {
							Command<CMD_DELETE_GROUP>::Post(STR_ERROR_GROUP_CAN_T_DELETE, mw.sel_grp);
							mw.sel_grp = ALL_GROUP;
						}
					} else if (c == 2 || c == 3) {
						Command<CMD_MASS_START_STOP>::Post(TileIndex{}, c == 2, true, vli);
					} else {
						Command<CMD_SEND_VEHICLE_TO_DEPOT>::Post(GetCmdSendToDepotMsg((VehicleType)mw.tab), VehicleID::Invalid(), DepotCommandFlag::MassSend, vli);
					}
				}
			} else if (act >= MWA_ROW_BASE) {
				const MiniWndRowAct &ra = _wnd_row_acts[act - MWA_ROW_BASE];
				if (ra.skip_order >= 0 && v != nullptr && v->owner == _local_company) {
					Command<CMD_SKIP_TO_ORDER>::Post(STR_ERROR_CAN_T_SKIP_TO_ORDER, v->tile, v->index, (VehicleOrderID)ra.skip_order);
				} else if (ra.ord_sel >= 0 && v != nullptr) {
					mw.sel_ord = mw.sel_ord == ra.ord_sel ? -1 : (int16_t)ra.ord_sel;
				} else if (ra.ord_move != 0 && v != nullptr && v->owner == _local_company && mw.sel_ord >= 0) {
					int to = mw.sel_ord + ra.ord_move;
					if (to >= 0 && to < v->GetNumOrders() &&
							Command<CMD_MOVE_ORDER>::Post(STR_ERROR_CAN_T_MOVE_THIS_ORDER, v->tile, v->index, (VehicleOrderID)mw.sel_ord, (VehicleOrderID)to)) {
						mw.sel_ord = (int16_t)to;
					}
				} else if (ra.ord_del && v != nullptr && v->owner == _local_company && mw.sel_ord >= 0) {
					Command<CMD_DELETE_ORDER>::Post(STR_ERROR_CAN_T_DELETE_THIS_ORDER, v->tile, v->index, (VehicleOrderID)mw.sel_ord);
					mw.sel_ord = -1;
				} else if ((ra.ord_load || ra.ord_unload) && v != nullptr && v->owner == _local_company && mw.sel_ord >= 0) {
					const Order *so = v->GetOrder((VehicleOrderID)mw.sel_ord);
					if (so != nullptr && so->IsType(OT_GOTO_STATION)) {
						if (ra.ord_load) {
							OrderLoadType next;
							switch (so->GetLoadType()) {
								case OrderLoadType::LoadIfPossible: next = OrderLoadType::FullLoad; break;
								case OrderLoadType::FullLoad: next = OrderLoadType::FullLoadAny; break;
								case OrderLoadType::FullLoadAny: next = OrderLoadType::NoLoad; break;
								default: next = OrderLoadType::LoadIfPossible; break;
							}
							Command<CMD_MODIFY_ORDER>::Post(STR_ERROR_CAN_T_MODIFY_THIS_ORDER, v->tile, v->index, (VehicleOrderID)mw.sel_ord, MOF_LOAD, to_underlying(next));
						} else {
							OrderUnloadType next;
							switch (so->GetUnloadType()) {
								case OrderUnloadType::UnloadIfPossible: next = OrderUnloadType::Unload; break;
								case OrderUnloadType::Unload: next = OrderUnloadType::Transfer; break;
								case OrderUnloadType::Transfer: next = OrderUnloadType::NoUnload; break;
								default: next = OrderUnloadType::UnloadIfPossible; break;
							}
							Command<CMD_MODIFY_ORDER>::Post(STR_ERROR_CAN_T_MODIFY_THIS_ORDER, v->tile, v->index, (VehicleOrderID)mw.sel_ord, MOF_UNLOAD, to_underlying(next));
						}
					}
				} else if (ra.refit != INVALID_CARGO && v != nullptr && v->owner == _local_company && v->IsStoppedInDepot()) {
					Command<CMD_REFIT_VEHICLE>::Post(GetCmdRefitVehMsg(v->type), v->tile, v->index, ra.refit, 0, false, false, 0);
				} else if (ra.ord_add && v != nullptr && v->owner == _local_company) {
					if (_order_pick_veh == v->index) EnterIdleMode(); else EnterOrderPickMode(v->index);
				} else if (ra.grp_sel != GroupID::Invalid() && mw.kind == MiniWndKind::Group) {
					mw.sel_grp = ra.grp_sel;
					mw.sel_eng = EngineID::Invalid();
				} else if (ra.repl_from != EngineID::Invalid() && mw.kind == MiniWndKind::Group) {
					mw.sel_eng = mw.sel_eng == ra.repl_from ? EngineID::Invalid() : ra.repl_from;
				} else if (ra.repl_to != EngineID::Invalid() && mw.kind == MiniWndKind::Group) {
					if (Engine::GetIfValid(mw.sel_eng) != nullptr) {
						Command<CMD_SET_AUTOREPLACE>::Post(mw.sel_grp, mw.sel_eng, ra.repl_to, false);
					}
				} else if (ra.repl_clear && mw.kind == MiniWndKind::Group) {
					if (Engine::GetIfValid(mw.sel_eng) != nullptr) {
						Command<CMD_SET_AUTOREPLACE>::Post(mw.sel_grp, mw.sel_eng, EngineID::Invalid(), false);
					}
				} else if (ra.grp_add != VehicleID::Invalid() && mw.kind == MiniWndKind::Group) {
					if (Group::IsValidID(mw.sel_grp)) {
						Command<CMD_ADD_VEHICLE_GROUP>::Post(STR_ERROR_GROUP_CAN_T_ADD_VEHICLE, mw.sel_grp, ra.grp_add, false, VehicleListIdentifier{});
					}
				} else if (ra.grp_rm != VehicleID::Invalid() && mw.kind == MiniWndKind::Group) {
					Command<CMD_ADD_VEHICLE_GROUP>::Post(STR_ERROR_GROUP_CAN_T_ADD_VEHICLE, DEFAULT_GROUP, ra.grp_rm, false, VehicleListIdentifier{});
				} else if (ra.buy != EngineID::Invalid() && mw.kind == MiniWndKind::Fleet) {
					if (mw.tab == VEH_TRAIN) {
						_fleet_draft[mw.tab].push_back(ra.buy);
					} else {
						_fleet_draft[mw.tab].assign(1, ra.buy);
					}
				} else if (ra.draft_del >= 0 && mw.kind == MiniWndKind::Fleet) {
					std::vector<EngineID> &draft = _fleet_draft[mw.tab];
					if ((size_t)ra.draft_del < draft.size()) draft.erase(draft.begin() + ra.draft_del);
				} else if (ra.deploy != INVALID_TILE && mw.kind == MiniWndKind::Fleet) {
					std::vector<EngineID> &draft = _fleet_draft[mw.tab];
					if (!IsDepotTile(ra.deploy)) {
						/* stale row */
					} else if (draft.empty()) {
						MiniUiScrollTo(TileX(ra.deploy) * TILE_SIZE, TileY(ra.deploy) * TILE_SIZE);
					} else if (_deploy.depot == INVALID_TILE) {
						_deploy = FleetDeploy{};
						_deploy.depot = ra.deploy;
						_deploy.vt = (VehicleType)mw.tab;
						_deploy.units = draft;
					}
				} else if (ra.detach && mw.kind == MiniWndKind::Fleet) {
					const Vehicle *sv = Vehicle::GetIfValid(mw.sel);
					if (sv != nullptr && sv->type == VEH_TRAIN) {
						Command<CMD_MOVE_RAIL_VEHICLE>::Post(STR_ERROR_CAN_T_MOVE_VEHICLE, sv->tile, sv->index, VehicleID::Invalid(), false);
						mw.sel = VehicleID::Invalid();
					}
				} else if (ra.mark != VehicleID::Invalid() && mw.kind == MiniWndKind::Fleet) {
					const Vehicle *mv = Vehicle::GetIfValid(mw.sel);
					const Vehicle *cv = Vehicle::GetIfValid(ra.mark);
					if (cv == nullptr) {
						mw.sel = VehicleID::Invalid();
					} else if (ra.attach && mv != nullptr && mv->type == VEH_TRAIN && cv->type == VEH_TRAIN &&
							mv->First() != cv->First() && mv->tile == cv->tile) {
						Command<CMD_MOVE_RAIL_VEHICLE>::Post(STR_ERROR_CAN_T_MOVE_VEHICLE, mv->tile, mv->index, cv->Last()->index, mv->First() == mv);
						mw.sel = VehicleID::Invalid();
					} else {
						mw.sel = mw.sel == ra.mark ? VehicleID::Invalid() : ra.mark;
					}
				} else if (ra.open_veh != VehicleID::Invalid()) {
					const Vehicle *ov = Vehicle::GetIfValid(ra.open_veh);
					if (ov != nullptr) OpenMiniWnd(MiniWndKind::Vehicle, ov->First()->index, StationID::Invalid());
				} else if (ra.open_st != StationID::Invalid()) {
					if (Station::IsValidID(ra.open_st)) OpenMiniWnd(MiniWndKind::Station, VehicleID::Invalid(), ra.open_st);
				} else if (ra.open_town != TownID::Invalid()) {
					if (Town::IsValidID(ra.open_town)) OpenMiniWnd(MiniWndKind::Town, VehicleID::Invalid(), StationID::Invalid(), ra.open_town);
				} else if (ra.jump != INVALID_TILE) {
					MiniUiScrollTo(TileX(ra.jump) * TILE_SIZE, TileY(ra.jump) * TILE_SIZE);
				}
			}
			return true;
		}

		if (y <= fr.top + WndTitleH() + 2 * _ms.hud_scale) {
			_wnd_drag = (int)_wnds.size() - 1;
			_wnd_drag_dx = x - _wnds.back().x;
			_wnd_drag_dy = y - _wnds.back().y;
		}
		return true;
	}
	return false;
}

static bool HandleWndWheel(int x, int y, int dir)
{
	for (size_t i = _wnds.size(); i-- > 0;) {
		if (!InRect(WndFrameRect(_wnds[i]), x, y)) continue;
		_wnds[i].scroll += dir;
		return true;
	}
	return false;
}

bool ShowMiniVehicleWindow(const Vehicle *v)
{
	if (!_mini_active) return false;
	OpenMiniWnd(MiniWndKind::Vehicle, v->First()->index, StationID::Invalid());
	return true;
}

bool ShowMiniStationWindow(StationID station)
{
	if (!_mini_active || !Station::IsValidID(station)) return false;
	OpenMiniWnd(MiniWndKind::Station, VehicleID::Invalid(), station);
	return true;
}

bool ShowMiniTownWindow(TownID town)
{
	if (!_mini_active || !Town::IsValidID(town)) return false;
	OpenMiniWnd(MiniWndKind::Town, VehicleID::Invalid(), StationID::Invalid(), town);
	return true;
}

bool ShowMiniIndustryWindow(IndustryID industry)
{
	if (!_mini_active || !Industry::IsValidID(industry)) return false;
	OpenMiniWnd(MiniWndKind::Industry, VehicleID::Invalid(), StationID::Invalid(), TownID::Invalid(), industry);
	return true;
}

bool ShowMiniDepotWindow(TileIndex tile, VehicleType type)
{
	if (!_mini_active || !IsDepotTile(tile)) return false;
	OpenFleetMiniWnd((int)type);
	return true;
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
	DrawMiniWnds();
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
	_glide = false;
	/* While following, zooming keeps the vehicle centred instead of
	 * anchoring the cursor point. */
	if (_follow_veh != VehicleID::Invalid()) return;
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
	CloseAllMiniWnds();
	_mini_active = false;
	EnterIdleMode();
	_overlay = MiniLayer::None;
	_overlay_auto = false;
	_last_tool_layer = MiniLayer::None;
	_menu_open = -1;
	_win_open = -1;
	_dragging = false;
	_zoom_anchored = false;
	_glide = false;
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

static size_t CarrierOwner(const Window *w)
{
	if (w->window_class != WC_EXTRA_VIEWPORT || w->window_number < MW_CARRIER_NUM_BASE) return SIZE_MAX;
	for (size_t i = 0; i < _wnds.size(); i++) {
		if (MiniCarrierNum(_wnds[i]) == w->window_number) return i;
	}
	return SIZE_MAX;
}

static void SubtractOverlayRect(const RlwRectI &p, const Rect &o, std::vector<RlwRectI> &out)
{
	int px1 = p.x + p.w, py1 = p.y + p.h;
	int ox1 = o.right + 1, oy1 = o.bottom + 1;
	if (o.left >= px1 || ox1 <= p.x || o.top >= py1 || oy1 <= p.y) {
		out.push_back(p);
		return;
	}
	if (o.top > p.y) out.push_back({p.x, p.y, p.w, o.top - p.y});
	if (oy1 < py1) out.push_back({p.x, oy1, p.w, py1 - oy1});
	int my0 = std::max(p.y, o.top), my1 = std::min(py1, oy1);
	if (o.left > p.x) out.push_back({p.x, my0, o.left - p.x, my1 - my0});
	if (ox1 < px1) out.push_back({ox1, my0, px1 - ox1, my1 - my0});
}

void MiniUiOverlayRects(std::vector<RlwRectI> &rects)
{
	if (!_mini_active) return;
	for (const Window *w : Window::IterateFromBack()) {
		if (MiniUiHidesWindow(w->window_class)) continue;
		size_t owner = CarrierOwner(w);
		if (owner == SIZE_MAX) {
			rects.push_back({w->left, w->top, w->width, w->height});
			continue;
		}
		std::vector<RlwRectI> parts{{w->left, w->top, w->width, w->height}};
		for (size_t j = owner + 1; j < _wnds.size(); j++) {
			std::vector<RlwRectI> next;
			for (const RlwRectI &p : parts) SubtractOverlayRect(p, WndFrameRect(_wnds[j]), next);
			parts = std::move(next);
		}
		rects.insert(rects.end(), parts.begin(), parts.end());
	}
}

void MiniUiScrollTo(int x, int y)
{
	_follow_veh = VehicleID::Invalid();
	if (!_mini_active) return;
	_zoom_anchored = false;
	_glide = true;
	_glide_x = x / (double)TILE_SIZE;
	_glide_y = y / (double)TILE_SIZE;
	_dest_ppt = std::max(_dest_ppt, _ms.jump_ppt);
}

bool MiniUiHandleMouseEvents(bool native_capture)
{
	if (!_mini_active) return false;

	if (!_dragging && !_middle_button_down && _wnd_drag < 0) {
		if (native_capture) return false;
		Window *w = FindWindowFromPt(_cursor.pos.x, _cursor.pos.y);
		if (w != nullptr && !MiniUiHidesWindow(w->window_class)) {
			/* A carrier under a higher mini window is visually covered
			 * there, so the chrome takes the click instead. */
			size_t owner = CarrierOwner(w);
			bool covered = false;
			if (owner != SIZE_MAX) {
				for (size_t j = owner + 1; j < _wnds.size(); j++) {
					if (InRect(WndFrameRect(_wnds[j]), _cursor.pos.x, _cursor.pos.y)) {
						covered = true;
						break;
					}
				}
			}
			if (!covered) return false;
		}
	}

	if (_wnd_drag >= 0) {
		if (_left_button_down && _wnd_drag < (int)_wnds.size()) {
			MiniWnd &mw = _wnds[_wnd_drag];
			mw.x = Clamp(_cursor.pos.x - _wnd_drag_dx, -WndW() / 2, _fbw - WndW() / 2);
			mw.y = Clamp(_cursor.pos.y - _wnd_drag_dy, 0, _fbh - WndTitleH());
		} else {
			_wnd_drag = -1;
		}
	}

	if (_middle_button_down && (_cursor.delta.x != 0 || _cursor.delta.y != 0)) {
		_zoom_anchored = false;
		_glide = false;
		_pan_vx = 0.0;
		_pan_vy = 0.0;
		_cam_y -= _cursor.delta.x * _ms.drag_pan_multiplier / _cam_ppt;
		_cam_x -= _cursor.delta.y * _ms.drag_pan_multiplier / _cam_ppt;
		ClampCamera();
	}

	if (_cursor.wheel != 0) {
		if (HandleWndWheel(_cursor.pos.x, _cursor.pos.y, _cursor.wheel > 0 ? 1 : -1)) {
			/* consumed by a mini window body */
		} else if (_menu_open >= 0 && InRect(_menu_panel_rect, _cursor.pos.x, _cursor.pos.y)) {
			_menu_scroll += _cursor.wheel > 0 ? 1 : -1;
		} else {
			ZoomAt(_cursor.pos.x, _cursor.pos.y, _cursor.wheel < 0);
		}
		_cursor.wheel = 0;
	}

	if (_left_button_down && !_left_button_clicked) {
		_left_button_clicked = true;
		if (!HandleWndClick(_cursor.pos.x, _cursor.pos.y) && !HandleMenuClick(_cursor.pos.x, _cursor.pos.y) && !HandleCmdClick(_cursor.pos.x, _cursor.pos.y) && !HandleWinClick(_cursor.pos.x, _cursor.pos.y) && !HandleSpeedClick(_cursor.pos.x, _cursor.pos.y) && !HandleStatusClick(_cursor.pos.x, _cursor.pos.y)) {
			if (_tool == MiniTool::None) {
				if (_order_pick_veh != VehicleID::Invalid()) {
					OrderPickClick(_cursor.pos.x, _cursor.pos.y);
				} else if (!HandleLabelClick(_cursor.pos.x, _cursor.pos.y) && !OpenVehicleWndAt(_cursor.pos.x, _cursor.pos.y)) {
					OpenDepotWndAt(_cursor.pos.x, _cursor.pos.y);
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
		if (_tool == MiniTool::Canal) CommitCanalPlan();
	}
	_prev_left = _left_button_down;

	if (_right_button_clicked) {
		_right_button_clicked = false;
		if (_dragging) {
			_dragging = false;
			ClearPlans();
		} else {
			EnterIdleMode();
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
			} else if (_tool != MiniTool::None || _follow_veh != VehicleID::Invalid() || _order_pick_veh != VehicleID::Invalid()) {
				EnterIdleMode();
			} else if (!_wnds.empty()) {
				CloseMiniWnd(_wnds.size() - 1);
			} else if (_menu_open >= 0 || _win_open >= 0) {
				_menu_open = -1;
				_win_open = -1;
			}
			break;

		/* Blueprint rotation is modal, not a global shortcut: it only lives
		 * while a directional placement tool is in hand. The airport tool
		 * reuses the pair to walk the available airport types. */
		case 'E':
			if (_tool == MiniTool::Airport) {
				CycleAirportType(1);
			} else if (IsDirPointTool(_tool)) {
				_point_dir = ChangeDiagDir(_point_dir, DIAGDIRDIFF_90RIGHT);
			}
			break;

		case 'Q':
			if (_tool == MiniTool::Airport) {
				CycleAirportType(-1);
			} else if (IsDirPointTool(_tool)) {
				_point_dir = ChangeDiagDir(_point_dir, DIAGDIRDIFF_90LEFT);
			}
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

	_fbw = _screen.width;
	_fbh = _screen.height;
	if (_fbw <= 0 || _fbh <= 0) return;

	_mini_frame++;
	PruneTextCache();
	MiniAtlasEnsure();
	UpdateLerpClock(delta_ms);
	ProcessFleetDeploy();
	RlwCmdClear();

	/* WASD and arrows arrive via _dirkeys; pan speed is constant in screen space.
	 * A held axis takes its velocity directly so movement starts instantly; a
	 * released axis decays on its own, so letting go of one diagonal key keeps
	 * the other axis coasting. */
	if (_dirkeys != 0) {
		_zoom_anchored = false;
		_glide = false;
	} else if (_glide || _zoom_anchored) {
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
			_glide = false;
			_cam_y += ex;
			_cam_x += ey;
			ClampCamera();
		}
	}

	if (_follow_veh != VehicleID::Invalid()) {
		const Vehicle *fv = Vehicle::GetIfValid(_follow_veh);
		if (fv == nullptr || _dirkeys != 0 || _middle_button_down) {
			_follow_veh = VehicleID::Invalid();
		} else {
			_glide = false;
			auto [fx, fy] = LerpVehWorld(fv);
			double f = 1.0 - std::exp(delta_ms / -_ms.glide_ms);
			_cam_x += (fx - _cam_x) * f;
			_cam_y += (fy - _cam_y) * f;
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
	DrawVehicleRing(ppt);
	Present();
}
