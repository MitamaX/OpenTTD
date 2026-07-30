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
#include "company_cmd.h"
#include "company_func.h"
#include "company_gui.h"
#include "elrail_func.h"
#include "engine_base.h"
#include "engine_cmd.h"
#include "engine_gui.h"
#include "core/backup_type.hpp"
#include "core/math_func.hpp"
#include "core/random_func.hpp"
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
#include "goal_base.h"
#include "graph_gui.h"
#include "ground_vehicle.hpp"
#include "group.h"
#include "group_cmd.h"
#include "gui.h"
#include "industry.h"
#include "industry_cmd.h"
#include "industry_map.h"
#include "industrytype.h"
#include "newgrf_industries.h"
#include "ini_type.h"
#include "landscape.h"
#include "landscape_cmd.h"
#include "league_gui.h"
#include "mini_atlas.h"
#include "economy_cmd.h"
#include "economy_func.h"
#include "misc_cmd.h"
#include "network/network.h"
#include "network/network_type.h"
#include "newgrf_airport.h"
#include "newgrf_roadstop.h"
#include "newgrf_station.h"
#include "news_gui.h"
#include "object_cmd.h"
#include "object_type.h"
#include "openttd.h"
#include "order_base.h"
#include "order_cmd.h"
#include "rail.h"
#include "palette_func.h"
#include "rail_cmd.h"
#include "rail_gui.h"
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
#include "subsidy_base.h"
#include "terraform_cmd.h"
#include "town.h"
#include "town_cmd.h"
#include "tile_map.h"
#include "train.h"
#include "tree_cmd.h"
#include "tree_map.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_economy.h"
#include "timer/timer_game_tick.h"
#include "tunnelbridge_cmd.h"
#include "tunnelbridge_map.h"
#include "vehicle_base.h"
#include "vehicle_cmd.h"
#include "water_cmd.h"
#include "waypoint_base.h"
#include "waypoint_cmd.h"
#include "waypoint_func.h"
#include "vehicle_func.h"
#include "vehicle_gui.h"
#include "vehiclelist.h"
#include "video/video_driver.hpp"
#include "viewport_func.h"
#include "water_map.h"
#include "window_func.h"
#include "window_gui.h"

#include "table/strings.h"

#include "imgui.h"

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
	Convert,
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
	RailBridge,
	RoadBridge,
	Terraform,
	Headquarters,
	Trees,
	BuyLand,
	Industry,
};

static bool IsRectTool(MiniTool t)
{
	return t == MiniTool::Station || t == MiniTool::Demolish || t == MiniTool::Terraform || t == MiniTool::Canal || t == MiniTool::Convert ||
			t == MiniTool::Trees || t == MiniTool::BuyLand;
}

static bool IsPointTool(MiniTool t)
{
	return t == MiniTool::BusStop || t == MiniTool::TruckStop || t == MiniTool::TrainDepot || t == MiniTool::RoadDepot || t == MiniTool::Signal || t == MiniTool::RailTunnel || t == MiniTool::RoadTunnel || t == MiniTool::RailWaypoint || t == MiniTool::ShipDepot || t == MiniTool::Dock || t == MiniTool::Buoy || t == MiniTool::Airport || t == MiniTool::Lock || t == MiniTool::Headquarters || t == MiniTool::Industry;
}

static bool IsBridgeTool(MiniTool t)
{
	return t == MiniTool::RailBridge || t == MiniTool::RoadBridge;
}

static bool IsDirPointTool(MiniTool t)
{
	return t == MiniTool::BusStop || t == MiniTool::TruckStop || t == MiniTool::TrainDepot || t == MiniTool::RoadDepot || t == MiniTool::ShipDepot;
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
		case MiniTool::Convert:
		case MiniTool::Station:
		case MiniTool::RailWaypoint:
		case MiniTool::TrainDepot:
		case MiniTool::Signal:
		case MiniTool::RailTunnel:
		case MiniTool::RailBridge:
			return MiniLayer::Rail;
		case MiniTool::Road:
		case MiniTool::BusStop:
		case MiniTool::TruckStop:
		case MiniTool::RoadDepot:
		case MiniTool::RoadTunnel:
		case MiniTool::RoadBridge:
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

struct MiniSignalPlan {
	TileIndex start = INVALID_TILE;
	TileIndex end = INVALID_TILE;
	Track track = INVALID_TRACK;
	std::vector<TileIndex> tiles;
};

static MiniSignalPlan _sig_plan;

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
	int imgui_demo = 0;
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
	ReadIniNumber(group, "imgui_demo", _ms.imgui_demo);

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

static ImVec4 ImGuiCol32(uint32_t argb)
{
	return ImVec4(((argb >> 16) & 0xFF) / 255.0f, ((argb >> 8) & 0xFF) / 255.0f, (argb & 0xFF) / 255.0f, ((argb >> 24) & 0xFF) / 255.0f);
}

/* The 1.92 dynamic atlas pulls glyphs on demand, so one Korean-capable font
 * covers every string without range tables. */
static void MiniImGuiEnsureSetup()
{
	static bool done = false;
	if (done) return;
	done = true;

	ImGuiIO &io = ImGui::GetIO();
	const char *font_path = "C:\\Windows\\Fonts\\malgun.ttf";
	if (FileExists(font_path)) {
		ImFont *font = io.Fonts->AddFontFromFileTTF(font_path, (float)std::max(13, GetCharacterHeight(FS_NORMAL)));
		if (font != nullptr) io.FontDefault = font;
	}

	ImGuiStyle &style = ImGui::GetStyle();
	style.WindowRounding = 4.0f;
	style.ChildRounding = 3.0f;
	style.FrameRounding = 3.0f;
	style.PopupRounding = 3.0f;
	style.TabRounding = 3.0f;
	style.ScrollbarRounding = 3.0f;
	style.GrabRounding = 3.0f;
	style.WindowBorderSize = 1.0f;

	ImVec4 *c = style.Colors;
	c[ImGuiCol_Text] = ImGuiCol32(COL_CH_TEXT);
	c[ImGuiCol_TextDisabled] = ImGuiCol32(COL_CH_DIM);
	c[ImGuiCol_WindowBg] = ImGuiCol32(COL_CH_PANEL);
	c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
	c[ImGuiCol_PopupBg] = ImGuiCol32(COL_CH_PANEL);
	c[ImGuiCol_Border] = ImGuiCol32(COL_CH_EDGE);
	c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
	c[ImGuiCol_FrameBg] = ImGuiCol32(COL_CH_TILE);
	c[ImGuiCol_FrameBgHovered] = ImGuiCol32(COL_CH_ACTIVE);
	c[ImGuiCol_FrameBgActive] = ImGuiCol32(COL_CH_ACTIVE);
	c[ImGuiCol_TitleBg] = ImGuiCol32(COL_CH_EDGE);
	c[ImGuiCol_TitleBgActive] = ImGuiCol32(COL_CH_TILE);
	c[ImGuiCol_TitleBgCollapsed] = ImGuiCol32(COL_CH_EDGE);
	c[ImGuiCol_MenuBarBg] = ImGuiCol32(COL_CH_EDGE);
	c[ImGuiCol_ScrollbarBg] = ImGuiCol32(COL_CH_EDGE);
	c[ImGuiCol_ScrollbarGrab] = ImGuiCol32(COL_CH_DIM);
	c[ImGuiCol_ScrollbarGrabHovered] = ImGuiCol32(COL_CH_ACCENT);
	c[ImGuiCol_ScrollbarGrabActive] = ImGuiCol32(COL_CH_ACCENT);
	c[ImGuiCol_CheckMark] = ImGuiCol32(COL_CH_ACCENT);
	c[ImGuiCol_SliderGrab] = ImGuiCol32(COL_CH_ACCENT);
	c[ImGuiCol_SliderGrabActive] = ImGuiCol32(COL_CH_ACCENT);
	c[ImGuiCol_Button] = ImGuiCol32(COL_CH_TILE);
	c[ImGuiCol_ButtonHovered] = ImGuiCol32(COL_CH_ACTIVE);
	c[ImGuiCol_ButtonActive] = ImGuiCol32(COL_CH_ACTIVE);
	c[ImGuiCol_Header] = ImGuiCol32(COL_CH_TILE);
	c[ImGuiCol_HeaderHovered] = ImGuiCol32(COL_CH_ACTIVE);
	c[ImGuiCol_HeaderActive] = ImGuiCol32(COL_CH_ACTIVE);
	c[ImGuiCol_Separator] = ImGuiCol32(COL_CH_EDGE);
	c[ImGuiCol_SeparatorHovered] = ImGuiCol32(COL_CH_ACCENT);
	c[ImGuiCol_SeparatorActive] = ImGuiCol32(COL_CH_ACCENT);
	c[ImGuiCol_ResizeGrip] = ImGuiCol32(COL_CH_DIM);
	c[ImGuiCol_ResizeGripHovered] = ImGuiCol32(COL_CH_ACCENT);
	c[ImGuiCol_ResizeGripActive] = ImGuiCol32(COL_CH_ACCENT);
	c[ImGuiCol_Tab] = ImGuiCol32(COL_CH_TILE);
	c[ImGuiCol_TabHovered] = ImGuiCol32(COL_CH_ACTIVE);
	c[ImGuiCol_TabSelected] = ImGuiCol32(COL_CH_ACTIVE);
	c[ImGuiCol_TabDimmed] = ImGuiCol32(COL_CH_EDGE);
	c[ImGuiCol_TabDimmedSelected] = ImGuiCol32(COL_CH_TILE);
	c[ImGuiCol_TableHeaderBg] = ImGuiCol32(COL_CH_TILE);
	c[ImGuiCol_TableBorderStrong] = ImGuiCol32(COL_CH_EDGE);
	c[ImGuiCol_TableBorderLight] = ImGuiCol32(COL_CH_EDGE);
	c[ImGuiCol_TextSelectedBg] = ImGuiCol32(COL_CH_ACTIVE);
	c[ImGuiCol_NavCursor] = ImGuiCol32(COL_CH_ACCENT);
}

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

static uint32_t PaletteRgb(PixelColour p)
{
	Colour c = _cur_palette.palette[p.p];
	return 0xFF000000U | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

static uint32_t CargoRgb(CargoType ct)
{
	return PaletteRgb(CargoSpec::Get(ct)->legend_colour);
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

/* Waypoints and buoys carry no map label, so the tile itself is the way into
 * their window. */
static bool OpenWaypointWndAt(int sx, int sy)
{
	int tx = (int)std::floor(MapXAt(sy));
	int ty = (int)std::floor(MapYAt(sx));
	if (tx < 0 || ty < 0 || tx >= (int)Map::SizeX() || ty >= (int)Map::SizeY()) return false;
	TileIndex tile = TileXY(tx, ty);
	if (!IsRailWaypointTile(tile) && !IsRoadWaypointTile(tile) && !IsBuoyTile(tile)) return false;
	return ShowMiniWaypointWindow(GetStationIndex(tile));
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
			if (hv == nullptr || hv->tile != _deploy.depot) {
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

static RailType _rail_type_sel = INVALID_RAILTYPE;

static RailType PickRailType()
{
	const Company *c = Company::GetIfValid(_local_company);
	if (c == nullptr) return RAILTYPE_RAIL;
	if (_rail_type_sel != INVALID_RAILTYPE && c->avail_railtypes.Test(_rail_type_sel)) return _rail_type_sel;
	/* Electric rail runs everything plain rail does, so once it exists it is
	 * the better default; mono and maglev stay an explicit choice. */
	if (c->avail_railtypes.Test(RAILTYPE_ELECTRIC)) return RAILTYPE_ELECTRIC;
	for (RailType rt = RAILTYPE_BEGIN; rt != RAILTYPE_END; rt++) {
		if (c->avail_railtypes.Test(rt)) return rt;
	}
	return RAILTYPE_RAIL;
}

static void CycleRailType(int dir)
{
	const Company *c = Company::GetIfValid(_local_company);
	if (c == nullptr) return;
	RailType cur = PickRailType();
	for (int i = 1; i <= (int)RAILTYPE_END; i++) {
		int t = ((int)cur + dir * i) % (int)RAILTYPE_END;
		if (t < 0) t += (int)RAILTYPE_END;
		if (c->avail_railtypes.Test((RailType)t)) {
			_rail_type_sel = (RailType)t;
			return;
		}
	}
}

static void ClearPlans()
{
	_plan.pieces.clear();
	_plan.path.clear();
	_road_plan.tiles.clear();
	_road_plan.start = INVALID_TILE;
	_sig_plan.tiles.clear();
	_sig_plan.start = INVALID_TILE;
	_rect_plan.valid = false;
}

/* Out of range until the player picks one, so spans keep taking the fastest
 * bridge the year allows. */
static BridgeType _bridge_sel = MAX_BRIDGES;

static BridgeType FastestBridgeType(uint len)
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

static BridgeType PickBridgeType(uint len)
{
	if (_bridge_sel < MAX_BRIDGES && CheckBridgeAvailability(_bridge_sel, len).Succeeded()) return _bridge_sel;
	return FastestBridgeType(len);
}

static void CycleBridgeType(int dir)
{
	int at = (int)PickBridgeType(1);
	for (int i = 1; i <= (int)MAX_BRIDGES; i++) {
		int bt = (at + dir * i) % (int)MAX_BRIDGES;
		if (bt < 0) bt += (int)MAX_BRIDGES;
		if (CheckBridgeAvailability((BridgeType)bt, 1).Succeeded()) {
			_bridge_sel = (BridgeType)bt;
			return;
		}
	}
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

/* The span reuses the axis-locked line drag: the two end tiles become ramps
 * and everything between them is the bridge itself. */
static uint BridgePlanLength()
{
	return _road_plan.tiles.size() < 3 ? 0 : (uint)(_road_plan.tiles.size() - 2);
}

static void DrawBridgePlan(int ppt)
{
	uint32_t c = BridgePlanLength() > 0 ? COL_BP : COL_BP_RM;
	int w = std::max(2, ppt / 2);
	const std::vector<TileIndex> &ts = _road_plan.tiles;
	for (size_t i = 0; i < ts.size(); i++) {
		int tx = TileX(ts[i]);
		int ty = TileY(ts[i]);
		int x0 = ScrX(ty), y0 = ScrY(tx), x1 = ScrX(ty + 1) - 1, y1 = ScrY(tx + 1) - 1;
		if (i == 0 || i + 1 == ts.size()) {
			BlendRect(x0, y0, x1, y1, c, 110);
		} else {
			DrawAxisBand(_road_plan.axis, x0, y0, x1, y1, w, c);
		}
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

static void CommitConvertPlan()
{
	if (!_rect_plan.valid) return;
	Command<CMD_CONVERT_RAIL>::Post(STR_ERROR_CAN_T_CONVERT_RAIL, TileXY(_rect_plan.x1, _rect_plan.y1), TileXY(_rect_plan.x0, _rect_plan.y0), PickRailType(), false);
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

static void CommitTreePlan()
{
	if (!_rect_plan.valid) return;
	if (_drag_remove) {
		Command<CMD_CLEAR_AREA>::Post(STR_ERROR_CAN_T_CLEAR_THIS_AREA, TileXY(_rect_plan.x1, _rect_plan.y1), TileXY(_rect_plan.x0, _rect_plan.y0), false);
	} else {
		Command<CMD_PLANT_TREE>::Post(STR_ERROR_CAN_T_PLANT_TREE_HERE, TileXY(_rect_plan.x1, _rect_plan.y1), TileXY(_rect_plan.x0, _rect_plan.y0), TREE_INVALID, false);
	}
	ClearPlans();
}

static void CommitBuyLandPlan()
{
	if (!_rect_plan.valid) return;
	if (_drag_remove) {
		Command<CMD_CLEAR_AREA>::Post(STR_ERROR_CAN_T_CLEAR_THIS_AREA, TileXY(_rect_plan.x1, _rect_plan.y1), TileXY(_rect_plan.x0, _rect_plan.y0), false);
	} else {
		Command<CMD_BUILD_OBJECT_AREA>::Post(STR_ERROR_CAN_T_PURCHASE_THIS_LAND, TileXY(_rect_plan.x1, _rect_plan.y1), TileXY(_rect_plan.x0, _rect_plan.y0), OBJECT_OWNED_LAND, 0, false);
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

/* Signals lay in runs as well as one at a time. Only the two grid-straight
 * tracks carry a run: the half tracks stop the plan at the anchor tile, so
 * the preview always matches what the command will build. */
static void UpdateSignalPlan(double wx, double wy)
{
	_sig_plan.tiles.clear();
	_sig_plan.start = INVALID_TILE;

	int ax = Clamp<int>((int)std::floor(_drag_ax), 0, Map::SizeX() - 1);
	int ay = Clamp<int>((int)std::floor(_drag_ay), 0, Map::SizeY() - 1);
	TileIndex anchor = TileXY(ax, ay);
	_sig_plan.track = PickSignalTrack(anchor, _drag_ax, _drag_ay);
	if (_sig_plan.track == INVALID_TRACK) return;

	_sig_plan.start = anchor;
	_sig_plan.end = anchor;
	_sig_plan.tiles.push_back(anchor);
	if (_sig_plan.track != TRACK_X && _sig_plan.track != TRACK_Y) return;

	bool along_x = _sig_plan.track == TRACK_X;
	int steps = Clamp((int)std::lround(along_x ? wx - _drag_ax : wy - _drag_ay), -127, 127);
	int dir = steps >= 0 ? 1 : -1;
	for (int i = dir; i != steps + dir; i += dir) {
		int tx = along_x ? ax + i : ax;
		int ty = along_x ? ay : ay + i;
		if (tx < 0 || ty < 0 || tx >= (int)Map::SizeX() || ty >= (int)Map::SizeY()) break;
		TileIndex tile = TileXY(tx, ty);
		if (!IsPlainRailTile(tile)) break;
		if ((GetTrackBits(tile) & TrackToTrackBits(_sig_plan.track)) == TRACK_BIT_NONE) break;
		_sig_plan.tiles.push_back(tile);
		_sig_plan.end = tile;
	}
}

static void DrawSignalPlan(int ppt)
{
	uint32_t c = _drag_remove ? COL_BP_RM : COL_BP;
	int w = std::max(2, ppt / 5);
	for (TileIndex tile : _sig_plan.tiles) {
		int tx = TileX(tile);
		int ty = TileY(tile);
		int x0 = ScrX(ty), y0 = ScrY(tx), x1 = ScrX(ty + 1) - 1, y1 = ScrY(tx + 1) - 1;
		BlendRect(x0, y0, x1, y1, c, 70);
		DrawTrackPiece(_sig_plan.track, x0, y0, x1, y1, w, c);
	}
}

/* Funding an industry has no window either: Q/E walk the types the fund list
 * would offer and the HUD names the type with its price. */
static IndustryType _industry_type = 0;

static bool MiniIndustryAvailable(IndustryType it)
{
	if (it >= NUM_INDUSTRYTYPES) return false;
	const IndustrySpec *indsp = GetIndustrySpec(it);
	if (!indsp->enabled) return false;
	if (indsp->IsRawIndustry() && _settings_game.construction.raw_industry_construction == 0) return false;
	return GetIndustryProbabilityCallback(it, IACT_USERCREATION, 1) > 0;
}

static IndustryType PickIndustryType()
{
	if (MiniIndustryAvailable(_industry_type)) return _industry_type;
	for (IndustryType it = 0; it < NUM_INDUSTRYTYPES; it++) {
		if (MiniIndustryAvailable(it)) return it;
	}
	return IT_INVALID;
}

static void CycleIndustryType(int dir)
{
	IndustryType at = PickIndustryType();
	if (at == IT_INVALID) return;
	for (int i = 1; i <= NUM_INDUSTRYTYPES; i++) {
		int t = ((int)at + dir * i) % NUM_INDUSTRYTYPES;
		if (t < 0) t += NUM_INDUSTRYTYPES;
		if (MiniIndustryAvailable((IndustryType)t)) {
			_industry_type = (IndustryType)t;
			return;
		}
	}
}

/* Signal choice lives in the tool as well: the picker window is replaced by
 * Q/E over the types the signal GUI setting exposes. */
static const SignalType _mini_signal_path[] = {SIGTYPE_PBS, SIGTYPE_PBS_ONEWAY};
static const SignalType _mini_signal_all[] = {SIGTYPE_BLOCK, SIGTYPE_ENTRY, SIGTYPE_EXIT, SIGTYPE_COMBO, SIGTYPE_PBS, SIGTYPE_PBS_ONEWAY};

static SignalType _signal_type = SIGTYPE_PBS;

static std::span<const SignalType> SignalChoices()
{
	if (_settings_client.gui.signal_gui_mode == SIGNAL_GUI_ALL) return _mini_signal_all;
	return _mini_signal_path;
}

static SignalType PickSignalType()
{
	std::span<const SignalType> choices = SignalChoices();
	for (SignalType t : choices) {
		if (t == _signal_type) return t;
	}
	return choices.front();
}

static void CycleSignalType(int dir)
{
	std::span<const SignalType> choices = SignalChoices();
	int n = (int)choices.size();
	int at = 0;
	for (int i = 0; i < n; i++) {
		if (choices[i] == PickSignalType()) at = i;
	}
	_signal_type = choices[((at + dir) % n + n) % n];
}

static std::string_view SignalTypeLabel(SignalType t)
{
	switch (t) {
		case SIGTYPE_ENTRY: return "ENTRY";
		case SIGTYPE_EXIT: return "EXIT";
		case SIGTYPE_COMBO: return "COMBO";
		case SIGTYPE_PBS: return "PATH";
		case SIGTYPE_PBS_ONEWAY: return "ONE-WAY PATH";
		default: return "BLOCK";
	}
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
				/* The waypoint must follow the track under it, so the axis comes
				 * from the tile rather than the tool rotation. */
				Axis axis = GetAxisForNewRailWaypoint(tile);
				Command<CMD_BUILD_RAIL_WAYPOINT>::Post(STR_ERROR_CAN_T_BUILD_RAIL_WAYPOINT, tile, IsValidAxis(axis) ? axis : AXIS_X, 1, 1, STAT_CLASS_WAYP, 0, StationID::Invalid(), false);
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

		case MiniTool::Industry: {
			IndustryType it = PickIndustryType();
			if (it == IT_INVALID) break;
			if (_drag_remove) {
				Command<CMD_LANDSCAPE_CLEAR>::Post(STR_ERROR_CAN_T_CLEAR_THIS_AREA, tile);
				break;
			}
			const IndustrySpec *indsp = GetIndustrySpec(it);
			/* Raw industries are prospected under the funding setting that hides
			 * their location, so the click only pays for the search. */
			bool prospect = _settings_game.construction.raw_industry_construction == 2 && indsp->IsRawIndustry();
			uint32_t seed = InteractiveRandom();
			uint32_t layout = InteractiveRandomRange((uint32_t)indsp->layouts.size());
			Command<CMD_BUILD_INDUSTRY>::Post(STR_ERROR_CAN_T_CONSTRUCT_THIS_INDUSTRY, prospect ? TileIndex{} : tile, it, prospect ? 0 : layout, false, seed);
			break;
		}

		case MiniTool::Headquarters:
			if (_drag_remove) {
				Command<CMD_LANDSCAPE_CLEAR>::Post(STR_ERROR_CAN_T_CLEAR_THIS_AREA, tile);
			} else {
				Command<CMD_BUILD_OBJECT>::Post(STR_ERROR_CAN_T_BUILD_COMPANY_HEADQUARTERS, tile, OBJECT_HQ, 0);
			}
			break;

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
	} else if (_tool == MiniTool::RailWaypoint) {
		Axis axis = GetAxisForNewRailWaypoint(TileXY(tx, ty));
		if (IsValidAxis(axis)) DrawAxisBand(axis, x0, y0, x1, y1, std::max(2, ppt / 3), c);
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
		if (as->IsAvailable()) BlendRect(x0, y0, ScrX(ty + as->size_y) - 1, ScrY(tx + as->size_x) - 1, c, 60);
	} else if (_tool == MiniTool::Headquarters) {
		BlendRect(x0, y0, ScrX(ty + 2) - 1, ScrY(tx + 2) - 1, c, 60);
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

static void CommitSignalPlan()
{
	if (_sig_plan.start == INVALID_TILE) {
		ClearPlans();
		return;
	}

	SignalVariant sigvar = TimerGameCalendar::year < _settings_client.gui.semaphore_build_before ? SIG_SEMAPHORE : SIG_ELECTRIC;
	if (_sig_plan.tiles.size() <= 1) {
		if (_drag_remove) {
			Command<CMD_REMOVE_SINGLE_SIGNAL>::Post(STR_ERROR_CAN_T_REMOVE_SIGNALS_FROM, _sig_plan.start, _sig_plan.track);
		} else {
			Command<CMD_BUILD_SINGLE_SIGNAL>::Post(STR_ERROR_CAN_T_BUILD_SIGNALS_HERE, _sig_plan.start, _sig_plan.track, PickSignalType(), sigvar, false, false, false, SIGTYPE_PBS, SIGTYPE_LAST, 0, 0);
		}
	} else if (_drag_remove) {
		Command<CMD_REMOVE_SIGNAL_TRACK>::Post(STR_ERROR_CAN_T_REMOVE_SIGNALS_FROM, _sig_plan.start, _sig_plan.end, _sig_plan.track, false);
	} else {
		Command<CMD_BUILD_SIGNAL_TRACK>::Post(STR_ERROR_CAN_T_BUILD_SIGNALS_HERE, _sig_plan.start, _sig_plan.end, _sig_plan.track, PickSignalType(), sigvar,
				false, false, !_settings_client.gui.drag_signals_fixed_distance, _settings_client.gui.drag_signals_density);
	}
	ClearPlans();
}

static void CommitBridgePlan()
{
	uint len = BridgePlanLength();
	if (len == 0) {
		ClearPlans();
		return;
	}
	const std::vector<TileIndex> &ts = _road_plan.tiles;
	if (_tool == MiniTool::RailBridge) {
		Command<CMD_BUILD_BRIDGE>::Post(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE, ts.back(), ts.front(), TRANSPORT_RAIL, PickBridgeType(len), (uint8_t)PickRailType());
	} else {
		Command<CMD_BUILD_BRIDGE>::Post(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE, ts.back(), ts.front(), TRANSPORT_ROAD, PickBridgeType(len), (uint8_t)PickRoadType());
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
	{INVALID_STRING_ID, "BRIDGE", MiniTool::RailBridge},
	{INVALID_STRING_ID, "CONVERT", MiniTool::Convert},
};

static const MiniMenuItem _menu_road_items[] = {
	{STR_LAI_ROAD_DESCRIPTION_ROAD, "ROAD", MiniTool::Road},
	{STR_LAI_STATION_DESCRIPTION_BUS_STATION, "BUS", MiniTool::BusStop},
	{STR_LAI_STATION_DESCRIPTION_TRUCK_LOADING_AREA, "TRUCK", MiniTool::TruckStop},
	{STR_LAI_ROAD_DESCRIPTION_ROAD_VEHICLE_DEPOT, "DEPOT", MiniTool::RoadDepot},
	{STR_LAI_TUNNEL_DESCRIPTION_ROAD, "TUNNEL", MiniTool::RoadTunnel},
	{INVALID_STRING_ID, "BRIDGE", MiniTool::RoadBridge},
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

static const MiniMenuItem _menu_land_items[] = {
	{STR_LAI_OBJECT_DESCRIPTION_COMPANY_HEADQUARTERS, "HQ", MiniTool::Headquarters},
	{STR_LAI_TREE_NAME_TREES, "TREES", MiniTool::Trees},
	{STR_LAI_OBJECT_DESCRIPTION_COMPANY_OWNED_LAND, "LAND", MiniTool::BuyLand},
	{INVALID_STRING_ID, "INDUSTRY", MiniTool::Industry},
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
	{INVALID_STRING_ID, "LAND", MiniTool::Headquarters, _menu_land_items},
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
		case MiniTool::Convert:
			ScreenThickLine(cx - h, cy + h - 2, cx + h, cy - 2, t, COL_BRIDGE);
			ScreenThickLine(cx - h, cy + 2, cx + h, cy - h + 2, t, COL_GO);
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
		case MiniTool::RailBridge:
		case MiniTool::RoadBridge:
			ScreenFillRect(cx - h, cy + is / 5, cx + h, cy + h, COL_WATER);
			ScreenFillRect(cx - h, cy - t, cx + h, cy, COL_BRIDGE);
			ScreenFillRect(cx - h, cy, cx - h + t, cy + is / 5, COL_BRIDGE);
			ScreenFillRect(cx + h - t, cy, cx + h, cy + is / 5, COL_BRIDGE);
			ScreenFillRect(cx - h, cy - t - 2, cx + h, cy - t - 1, tool == MiniTool::RailBridge ? COL_PAPER : COL_CATENARY);
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
		case MiniTool::Headquarters: {
			uint32_t cc = Company::IsValidID(_local_company) ? _company_rgb[_company_colours[_local_company]] : COL_OBJ;
			ScreenFillRect(cx - h, cy - is / 6, cx + h, cy + h, COL_OBJ_B);
			ScreenFillRect(cx - h + 2, cy - is / 6 + 2, cx + h - 2, cy + h - 2, cc);
			ScreenFillRect(cx - t, cy - h, cx + t, cy - is / 6, COL_OBJ);
			break;
		}
		case MiniTool::Trees:
			ScreenFillRect(cx - 1, cy, cx + 1, cy + h, COL_ROAD);
			ScreenFillCircle(cx, cy - is / 6, h - 1, COL_TREE);
			break;
		case MiniTool::BuyLand:
			ScreenFillRect(cx - h, cy - h, cx + h, cy + h, COL_FIELDS);
			ScreenFillRect(cx - h + 2, cy - h + 2, cx + h - 2, cy + h - 2, COL_OBJ);
			ScreenFillCircle(cx, cy, t, COL_ST_BUOY);
			break;
		case MiniTool::Industry:
			ScreenFillRect(cx - h, cy - is / 6, cx + h, cy + h, COL_IND_B);
			ScreenFillRect(cx - h + 2, cy - is / 6 + 2, cx + h - 2, cy + h - 2, COL_IND);
			ScreenFillRect(cx + h / 4, cy - h, cx + h / 4 + t, cy - is / 6, COL_IND_B);
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
	Map,
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
	{INVALID_STRING_ID, "MAP", MiniWin::Map},
	{STR_NEWS_MENU_MESSAGE_HISTORY_MENU, "NEWS", MiniWin::News},
	{STR_TOWN_MENU_TOWN_DIRECTORY, "TOWNS", MiniWin::Towns},
	{STR_INDUSTRY_MENU_INDUSTRY_DIRECTORY, "INDUSTRY", MiniWin::Industries},
	{STR_SUBSIDIES_MENU_SUBSIDIES, "SUBSIDY", MiniWin::Subsidies},
};

static const MiniWinCategory _win_cats[] = {
	{STR_CONFIG_SETTING_COMPANY, "COMPANY", MiniWin::Finances, _win_company_items},
	{STR_CONFIG_SETTING_VEHICLES, "VEHICLES", MiniWin::Trains, _win_vehicle_items},
	{STR_CONFIG_SETTING_ENVIRONMENT, "WORLD", MiniWin::Map, _win_world_items},
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
		case MiniWin::Map:
			ScreenFillRect(cx - h, cy - h, cx + h, cy + h, _height_ramp[5]);
			ScreenThickLine(cx - h, cy + h / 2, cx + h, cy - h / 2, t, COL_WATER);
			ScreenFillRect(cx - h, cy - h, cx + h, cy - h + 1, COL_INK);
			ScreenFillRect(cx - h, cy + h - 1, cx + h, cy + h, COL_INK);
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
static void OpenCompanyMiniWnd();
static void OpenGroupMiniWnd(int vt);
static void OpenStationListMiniWnd();
static void OpenTownListMiniWnd();
static void OpenIndustryListMiniWnd();
static void OpenNewsListMiniWnd();
static void OpenSubsidyListMiniWnd();
static void OpenGoalListMiniWnd();
static void OpenLeagueMiniWnd();
static void OpenGraphMiniWnd();
static void OpenMapMiniWnd();

static void OpenMiniWindow(MiniWin win)
{
	bool company = Company::IsValidID(_local_company);
	switch (win) {
		case MiniWin::Finances: if (company) OpenFinanceMiniWnd(); break;
		case MiniWin::CompanyInfo: if (company) OpenCompanyMiniWnd(); break;
		case MiniWin::Goals: OpenGoalListMiniWnd(); break;
		case MiniWin::League: OpenLeagueMiniWnd(); break;
		case MiniWin::Graph: OpenGraphMiniWnd(); break;
		case MiniWin::Stations: if (company) OpenStationListMiniWnd(); break;
		case MiniWin::Trains: if (company) OpenGroupMiniWnd(VEH_TRAIN); break;
		case MiniWin::RoadVehicles: if (company) OpenGroupMiniWnd(VEH_ROAD); break;
		case MiniWin::Ships: if (company) OpenGroupMiniWnd(VEH_SHIP); break;
		case MiniWin::Aircraft: if (company) OpenGroupMiniWnd(VEH_AIRCRAFT); break;
		case MiniWin::News: OpenNewsListMiniWnd(); break;
		case MiniWin::Towns: OpenTownListMiniWnd(); break;
		case MiniWin::Industries: OpenIndustryListMiniWnd(); break;
		case MiniWin::Subsidies: OpenSubsidyListMiniWnd(); break;
		case MiniWin::Buy: if (company) OpenFleetMiniWnd(-1); break;
		case MiniWin::Groups: if (company) OpenGroupMiniWnd(-1); break;
		case MiniWin::Map: OpenMapMiniWnd(); break;
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

/* Command failures land here instead of the stock modal: a stack of cards
 * above the command bar that ages out on its own. */
struct MiniToast {
	std::string summary;
	std::string detail;
	uint repeat = 1;
	uint left_ms = 0;
	uint full_ms = 0;
	bool warn = false;
	NewsReference ref{};
};

static std::vector<MiniToast> _toasts;
static std::vector<Rect> _toast_rows;

static constexpr size_t MINI_TOAST_MAX = 4;
static constexpr uint MINI_TOAST_FADE_MS = 400;

static void UpdateToasts(uint delta_ms)
{
	for (MiniToast &t : _toasts) t.left_ms = t.left_ms > delta_ms ? t.left_ms - delta_ms : 0;
	std::erase_if(_toasts, [](const MiniToast &t) { return t.left_ms == 0; });
}

static void DrawToasts()
{
	_toast_rows.clear();
	if (_toasts.empty()) return;

	int s = _ms.hud_scale;
	int lh = GetCharacterHeight(FS_NORMAL);
	int margin = 6 * s;
	int pad = 5 * s;
	int bar_w = 3 * s;
	int card_w = std::min(340 * s, _fbw - 2 * margin);
	int maxw = card_w - bar_w - 2 * pad;
	int y = _fbh - margin - MenuTileSide() - 4 * s;

	for (size_t i = _toasts.size(); i-- > 0;) {
		const MiniToast &t = _toasts[i];
		std::string head = t.repeat > 1 ? fmt::format("{} ×{}", t.summary, t.repeat) : t.summary;
		std::string line0{TruncateText(head, maxw)};
		std::string line1 = t.detail.empty() ? std::string{} : std::string{TruncateText(t.detail, maxw)};
		int lines = line1.empty() ? 1 : 2;
		int ch = lines * lh + (lines - 1) * 2 * s + 2 * pad;
		int top = y - ch + 1;
		if (top < 0) break;

		uint a = t.left_ms >= MINI_TOAST_FADE_MS ? 255 : t.left_ms * 255 / MINI_TOAST_FADE_MS;
		uint32_t am = a << 24;
		uint32_t bar = (t.warn ? 0x00E05F4AU : 0x00E0B64AU) | am;
		uint32_t bg = (t.warn ? 0x004A2320U : 0x00453A1EU) | am;
		uint32_t tcol = (t.warn ? 0x00F2D9D2U : 0x00EBD9A8U) | am;

		int x0 = (_fbw - card_w) / 2;
		Rect r = {x0, top, x0 + card_w - 1, y};
		ScreenFillRect(r.left, r.top, r.left + bar_w - 1, r.bottom, bar);
		ScreenFillRect(r.left + bar_w, r.top, r.right, r.bottom, bg);
		int tx = r.left + bar_w + pad;
		if (const MiniTextEntry *e = TextTexture(line0); e != nullptr) DrawTextQuad(e, tx, top + pad, tcol);
		if (!line1.empty()) {
			if (const MiniTextEntry *e = TextTexture(line1); e != nullptr) DrawTextQuad(e, tx, top + pad + lh + 2 * s, (tcol & 0x00FFFFFFU) | ((a * 7 / 10) << 24));
		}
		_toast_rows.push_back(r);
		y = top - 3 * s;
	}
}

static void NewsRefFollow(const NewsReference &ref);

static bool HandleToastClick(int x, int y)
{
	for (size_t i = 0; i < _toast_rows.size(); i++) {
		if (!InRect(_toast_rows[i], x, y)) continue;
		size_t idx = _toasts.size() - 1 - i;
		if (idx >= _toasts.size()) return true;
		NewsReference ref = _toasts[idx].ref;
		_toasts.erase(_toasts.begin() + (ptrdiff_t)idx);
		NewsRefFollow(ref);
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
			case MiniTool::Rail: hint = "DRAG PATH / Q E TYPE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Convert: hint = "DRAG AREA / Q E TYPE / RMB CANCEL"; break;
			case MiniTool::Road: hint = "DRAG LINE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Station: hint = "DRAG AREA / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::BusStop:
			case MiniTool::TruckStop:
			case MiniTool::RailWaypoint: hint = "CLICK TRACK / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::TrainDepot:
			case MiniTool::RoadDepot: hint = "Q E ROTATE EXIT / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::ShipDepot: hint = "Q E ROTATE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Dock: hint = "CLICK SHORE SLOPE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Buoy: hint = "CLICK WATER / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Airport: hint = "Q E TYPE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Canal: hint = "DRAG AREA / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Lock: hint = "CLICK SLOPE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Demolish: hint = "DRAG AREA / RMB CANCEL"; break;
			case MiniTool::Signal: hint = "DRAG TRACK / Q E TYPE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::RailTunnel:
			case MiniTool::RoadTunnel: hint = "CLICK SLOPE / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::RailBridge:
			case MiniTool::RoadBridge: hint = "DRAG SPAN / Q E TYPE / RMB CANCEL"; break;
			case MiniTool::Terraform: hint = "DRAG LEVEL / CLICK RAISE / CTRL LOWER / RMB CANCEL"; break;
			case MiniTool::Headquarters: hint = "CLICK 2x2 SPOT / CTRL REMOVE / RMB CANCEL"; break;
			case MiniTool::Trees: hint = "DRAG AREA / CTRL CLEAR / RMB CANCEL"; break;
			case MiniTool::BuyLand: hint = "DRAG AREA / CTRL SELL / RMB CANCEL"; break;
			case MiniTool::Industry: hint = "CLICK SITE / Q E TYPE / CTRL REMOVE / RMB CANCEL"; break;
			default: break;
		}
		if (_cursor.in_window) {
			std::string title = ToolLabel(_tool);
			if (_tool == MiniTool::Airport) {
				const AirportSpec *as = AirportSpec::Get(PickAirportType());
				if (as->IsAvailable()) title += fmt::format("  {}", StrMakeValid(GetString(as->name), {}));
			}
			if (_tool == MiniTool::Rail || _tool == MiniTool::Convert) {
				title += fmt::format("  {}", StrMakeValid(GetString(GetRailTypeInfo(PickRailType())->strings.name), {}));
			}
			if (_tool == MiniTool::Signal) title += fmt::format("  {}", SignalTypeLabel(PickSignalType()));
			if (IsBridgeTool(_tool)) {
				uint len = std::max(BridgePlanLength(), 1U);
				title += fmt::format("  {}", StrMakeValid(GetString(GetBridgeSpec(PickBridgeType(len))->material), {}));
			}
			if (_tool == MiniTool::Industry) {
				IndustryType it = PickIndustryType();
				if (it != IT_INVALID) {
					const IndustrySpec *indsp = GetIndustrySpec(it);
					title += fmt::format("  {}  {}", StrMakeValid(GetString(indsp->name), {}),
							StrMakeValid(GetString(STR_JUST_CURRENCY_LONG, indsp->GetConstructionCost()), {}));
				}
			}
			if (_dragging) {
				if (_tool == MiniTool::Rail && !_plan.pieces.empty()) title += fmt::format("  {}", _plan.pieces.size());
				if (_tool == MiniTool::Road && !_road_plan.tiles.empty()) title += fmt::format("  {}", _road_plan.tiles.size());
				if (IsBridgeTool(_tool)) title += fmt::format("  {}", BridgePlanLength());
				if (_tool == MiniTool::Signal && !_sig_plan.tiles.empty()) title += fmt::format("  {}", _sig_plan.tiles.size());
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
	Company,
	Group,
	StationList,
	TownList,
	IndustryList,
	NewsList,
	SubsidyList,
	GoalList,
	League,
	Graph,
	Preview,
	Takeover,
	Waypoint,
	Map,
};

/* Kinds that carry no entity and take no commands: they read the world each
 * frame and act on a click in the body. */
static bool WndIsList(MiniWndKind kind)
{
	return kind == MiniWndKind::StationList || kind == MiniWndKind::TownList ||
			kind == MiniWndKind::IndustryList || kind == MiniWndKind::NewsList ||
			kind == MiniWndKind::SubsidyList || kind == MiniWndKind::GoalList ||
			kind == MiniWndKind::League || kind == MiniWndKind::Graph ||
			kind == MiniWndKind::Map;
}

struct MiniWnd {
	MiniWndKind kind = MiniWndKind::Vehicle;
	VehicleID veh = VehicleID::Invalid();
	StationID st = StationID::Invalid();
	TownID town = TownID::Invalid();
	IndustryID ind = IndustryID::Invalid();
	EngineID eng = EngineID::Invalid();
	CompanyID comp = CompanyID::Invalid();
	bool hostile = false;
	VehicleID sel = VehicleID::Invalid();
	GroupID sel_grp = ALL_GROUP;
	EngineID sel_eng = EngineID::Invalid();
	int16_t sel_ord = -1;
	int x = 0, y = 0;
	uint8_t tab = 0;
	bool want_raise = false;
	int8_t want_tab = -1;
	bool renaming = false;
	bool focus_name = false;
	GroupID rename_grp = GroupID::Invalid();
	bool rename_pres = false;
	bool want_close = false;
	char name_buf[128] = {};
};

static std::vector<MiniWnd> _wnds;

/* The last focused mini window decides whose order route shows; focus is
 * tracked from the ImGui side each frame. */
static VehicleID _front_wnd_veh = VehicleID::Invalid();

static VehicleID FrontWndVehicle()
{
	return _front_wnd_veh;
}

/* Work windows and the plot run twice as wide; the other kinds keep the
 * narrow single-column shape. */
static bool WndWide(const MiniWnd &mw) { return mw.kind == MiniWndKind::Fleet || mw.kind == MiniWndKind::Group || mw.kind == MiniWndKind::Graph || mw.kind == MiniWndKind::Map; }
static int WndW(const MiniWnd &mw) { return std::min((WndWide(mw) ? 560 : 250) * _ms.hud_scale, _fbw - 12 * _ms.hud_scale); }
static int WndTitleH() { return GetCharacterHeight(FS_NORMAL) + 8 * _ms.hud_scale; }
static int WndTabH() { return GetCharacterHeight(FS_NORMAL) + 8 * _ms.hud_scale; }
static int WndRowH() { return GetCharacterHeight(FS_NORMAL) + 5 * _ms.hud_scale; }
static int WndViewH() { return 100 * _ms.hud_scale; }
static int WndCmdS() { return 26 * _ms.hud_scale; }
static int WndPad() { return 6 * _ms.hud_scale; }
static int WndBodyH(const MiniWnd &mw) { return WndViewH() + WndPad() + (WndWide(mw) ? 10 : 6) * WndRowH(); }
static int WndH(const MiniWnd &mw) { return WndTitleH() + WndTabH() + WndBodyH(mw) + WndCmdS() + 3 * WndPad(); }

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
		case MiniWndKind::Industry: id = (int)mw.ind.base(); break;
		default: id = 0; break;
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

/* Frameless native viewport window aligned with a mini window's view slot. */
struct MiniCarrierWindow : Window {
	TileIndex focus_tile = INVALID_TILE;

	MiniCarrierWindow(WindowDesc &desc, WindowNumber num, std::variant<TileIndex, VehicleID> focus) : Window(desc)
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
		cw = new MiniCarrierWindow(_mini_carrier_desc, MiniCarrierNum(mw), focus);
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
	if (_wnds[i].kind == MiniWndKind::Vehicle && _wnds[i].veh == _front_wnd_veh) _front_wnd_veh = VehicleID::Invalid();
	_wnds.erase(_wnds.begin() + (ptrdiff_t)i);
}

/* The carrier rises with its window so native z-order keeps matching the
 * mini window order where slots overlap. */
static void RaiseMiniWnd(size_t i)
{
	std::rotate(_wnds.begin() + (ptrdiff_t)i, _wnds.begin() + (ptrdiff_t)i + 1, _wnds.end());
	BringWindowToFrontById(WC_EXTRA_VIEWPORT, MiniCarrierNum(_wnds.back()));
	_wnds.back().want_raise = true;
}

/* Rows open other windows from inside the draw loop; growing or reordering
 * _wnds there would strand the reference the loop is drawing through, so the
 * request waits until the loop is done. */
struct MiniOpenReq {
	MiniWndKind kind;
	VehicleID veh;
	StationID st;
	TownID town;
	IndustryID ind;
	EngineID eng;
	CompanyID comp;
};

static std::vector<MiniOpenReq> _wnd_opens;
static bool _wnds_drawing = false;

static void OpenMiniWnd(MiniWndKind kind, VehicleID veh, StationID st, TownID town = TownID::Invalid(), IndustryID ind = IndustryID::Invalid(), EngineID eng = EngineID::Invalid(), CompanyID comp = CompanyID::Invalid())
{
	if (_wnds_drawing) {
		_wnd_opens.push_back({kind, veh, st, town, ind, eng, comp});
		return;
	}
	for (size_t i = 0; i < _wnds.size(); i++) {
		if (_wnds[i].kind == kind && _wnds[i].veh == veh && _wnds[i].st == st && _wnds[i].town == town && _wnds[i].ind == ind && _wnds[i].eng == eng && _wnds[i].comp == comp) {
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
	mw.eng = eng;
	mw.comp = comp;
	mw.x = Clamp(_fbw - 6 * s - WndW(mw) - (int)_wnds.size() * 20 * s, 0, std::max(0, _fbw - WndW(mw)));
	mw.y = Clamp(_win_bar_bottom + 6 * s + (int)_wnds.size() * 20 * s, 0, std::max(0, _fbh - WndH(mw)));
	_wnds.push_back(mw);
}

static void CloseAllMiniWnds()
{
	for (const MiniWnd &mw : _wnds) CloseMiniCarrier(mw);
	_wnds.clear();
	_wnd_opens.clear();
}

static void OpenFleetMiniWnd(int vt)
{
	OpenMiniWnd(MiniWndKind::Fleet, VehicleID::Invalid(), StationID::Invalid());
	if (vt >= 0) _wnds.back().tab = (uint8_t)vt;
}

static void OpenFinanceMiniWnd()
{
	OpenMiniWnd(MiniWndKind::Finance, VehicleID::Invalid(), StationID::Invalid());
}

static void OpenCompanyMiniWnd()
{
	OpenMiniWnd(MiniWndKind::Company, VehicleID::Invalid(), StationID::Invalid());
}

static void OpenGroupMiniWnd(int vt)
{
	OpenMiniWnd(MiniWndKind::Group, VehicleID::Invalid(), StationID::Invalid());
	if (vt >= 0) _wnds.back().tab = (uint8_t)vt;
}

static void OpenStationListMiniWnd()
{
	OpenMiniWnd(MiniWndKind::StationList, VehicleID::Invalid(), StationID::Invalid());
}

static void OpenTownListMiniWnd()
{
	OpenMiniWnd(MiniWndKind::TownList, VehicleID::Invalid(), StationID::Invalid());
}

static void OpenIndustryListMiniWnd()
{
	OpenMiniWnd(MiniWndKind::IndustryList, VehicleID::Invalid(), StationID::Invalid());
}

static void OpenNewsListMiniWnd()
{
	OpenMiniWnd(MiniWndKind::NewsList, VehicleID::Invalid(), StationID::Invalid());
}

static void OpenSubsidyListMiniWnd()
{
	OpenMiniWnd(MiniWndKind::SubsidyList, VehicleID::Invalid(), StationID::Invalid());
}

static void OpenGoalListMiniWnd()
{
	OpenMiniWnd(MiniWndKind::GoalList, VehicleID::Invalid(), StationID::Invalid());
}

static void OpenLeagueMiniWnd()
{
	OpenMiniWnd(MiniWndKind::League, VehicleID::Invalid(), StationID::Invalid());
}

static void OpenGraphMiniWnd()
{
	OpenMiniWnd(MiniWndKind::Graph, VehicleID::Invalid(), StationID::Invalid());
}

static void OpenMapMiniWnd()
{
	OpenMiniWnd(MiniWndKind::Map, VehicleID::Invalid(), StationID::Invalid());
}

static std::string WndOfficial(StringID str)
{
	return StrMakeValid(GetString(str), {});
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

/* ImGui mini windows: layout, clipping, scroll and input routing come from
 * ImGui; rows execute their commands at the click site. */

static int _imrow;

static ImU32 MiniImU32(uint32_t argb)
{
	return IM_COL32((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, (argb >> 24) & 0xFF);
}

/* Rows never widen the window: anything past the content edge wraps onto the
 * next line, and key/value rows wrap the key against the value's column. */
static float ImWndTextWidth(std::string_view text)
{
	return ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
}

static float ImWndTextHeight(std::string_view text, float wrap_w)
{
	return std::max(ImGui::CalcTextSize(text.data(), text.data() + text.size(), false, wrap_w).y, ImGui::GetFontSize());
}

static void ImWndDrawWrapped(ImDrawList *dl, ImVec2 pos, std::string_view text, uint32_t tint, float wrap_w)
{
	dl->AddText(nullptr, 0.0f, pos, MiniImU32(tint), text.data(), text.data() + text.size(), wrap_w);
}

static void ImWndText(std::string_view text, uint32_t tint)
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImGuiCol32(tint));
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextUnformatted(text.data(), text.data() + text.size());
	ImGui::PopTextWrapPos();
	ImGui::PopStyleColor();
}

/* Splits the row into a wrapped key column and a value pinned to the right of
 * the first line. */
static float ImWndKeyColumn(float avail, std::string_view value)
{
	return std::max(avail - ImWndTextWidth(value) - ImGui::GetStyle().ItemSpacing.x, ImGui::GetFontSize());
}

static void ImWndKV(std::string_view label, std::string_view value, uint32_t vtint)
{
	float avail = ImGui::GetContentRegionAvail().x;
	float lw = ImWndKeyColumn(avail, value);
	ImVec2 p = ImGui::GetCursorScreenPos();
	ImDrawList *dl = ImGui::GetWindowDrawList();
	ImWndDrawWrapped(dl, p, label, COL_CH_DIM, lw);
	ImWndDrawWrapped(dl, ImVec2(p.x + avail - ImWndTextWidth(value), p.y), value, vtint, 0.0f);
	ImGui::Dummy(ImVec2(avail, ImWndTextHeight(label, lw)));
}

static bool ImWndLink(std::string_view label, uint32_t tint)
{
	ImGui::PushID(_imrow++);
	float avail = ImGui::GetContentRegionAvail().x;
	ImVec2 p = ImGui::GetCursorScreenPos();
	bool clicked = ImGui::Selectable("##link", false, 0, ImVec2(0.0f, ImWndTextHeight(label, avail)));
	ImWndDrawWrapped(ImGui::GetWindowDrawList(), p, label, tint, avail);
	ImGui::PopID();
	return clicked;
}

static bool ImWndKVLink(std::string_view label, std::string_view value, uint32_t ltint, uint32_t vtint)
{
	ImGui::PushID(_imrow++);
	float avail = ImGui::GetContentRegionAvail().x;
	float lw = ImWndKeyColumn(avail, value);
	ImVec2 p = ImGui::GetCursorScreenPos();
	bool clicked = ImGui::Selectable("##kv", false, 0, ImVec2(0.0f, ImWndTextHeight(label, lw)));
	ImDrawList *dl = ImGui::GetWindowDrawList();
	ImWndDrawWrapped(dl, p, label, ltint, lw);
	ImWndDrawWrapped(dl, ImVec2(p.x + avail - ImWndTextWidth(value), p.y), value, vtint, 0.0f);
	ImGui::PopID();
	return clicked;
}

static void ImWndHeader(std::string_view text)
{
	ImGui::SeparatorText(std::string(text).c_str());
}

/* The carrier keeps painting into the CPU screen texture below the ImGui
 * layer; the slot samples that region back as an image, so window z-order
 * and clipping stay correct for free. */
static void ImWndViewSlot(const MiniWnd &mw)
{
	float vh = (float)(100 * _ms.hud_scale);
	ImVec2 pos = ImGui::GetCursorScreenPos();
	float vw = ImGui::GetContentRegionAvail().x;
	if (vw < 32.0f || _fbw <= 0 || _fbh <= 0) return;
	EnsureMiniCarrier(mw, (int)pos.x, (int)pos.y, (int)vw, (int)vh);
	uintptr_t tid = RlwScreenTexId();
	if (tid != 0) {
		ImVec2 uv0(pos.x / (float)_fbw, pos.y / (float)_fbh);
		ImVec2 uv1((pos.x + vw) / (float)_fbw, (pos.y + vh) / (float)_fbh);
		ImGui::Image((ImTextureID)tid, ImVec2(vw, vh), uv0, uv1);
	} else {
		ImGui::Dummy(ImVec2(vw, vh));
	}
}

struct ImStripUnit {
	int len8 = 8;
	uint32_t fill = 0;
	bool engine = false;
	bool selected = false;
};

static int ImWndUnitStrip(const std::vector<ImStripUnit> &units)
{
	int s = _ms.hud_scale;
	float bh = ImGui::GetTextLineHeight() * 1.6f;
	float avail = ImGui::GetContentRegionAvail().x;
	ImVec2 origin = ImGui::GetCursorScreenPos();
	ImDrawList *dl = ImGui::GetWindowDrawList();
	int clicked = -1;
	float x = 0.0f;
	ImGui::PushID(_imrow++);
	for (size_t i = 0; i < units.size(); i++) {
		const ImStripUnit &u = units[i];
		float uw = (float)std::max(4 * s, 3 * s * u.len8 / 2);
		if (x + uw > avail - 8.0f * s) {
			dl->AddText(ImVec2(origin.x + x, origin.y), MiniImU32(COL_CH_DIM), "…");
			break;
		}
		ImGui::SetCursorScreenPos(ImVec2(origin.x + x, origin.y));
		ImGui::PushID((int)i);
		if (ImGui::InvisibleButton("u", ImVec2(uw, bh))) clicked = (int)i;
		ImVec2 p0 = ImGui::GetItemRectMin();
		ImVec2 p1 = ImGui::GetItemRectMax();
		dl->AddRectFilled(p0, p1, MiniImU32(u.fill));
		if (u.engine) dl->AddRectFilled(ImVec2(p0.x, p1.y - 2.0f * s), p1, MiniImU32(COL_CH_ACCENT));
		if (u.selected) dl->AddRect(p0, p1, MiniImU32(COL_CH_ACCENT), 0.0f, 0, (float)s);
		if (ImGui::IsItemHovered()) dl->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, 48));
		ImGui::PopID();
		x += uw + (float)s;
	}
	ImGui::PopID();
	ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + bh + 2.0f * _ms.hud_scale));
	ImGui::Dummy(ImVec2(0.0f, 0.0f));
	return clicked;
}

static int ImTrainStrip(const Train *head, VehicleID sel, std::vector<VehicleID> &ids)
{
	std::vector<ImStripUnit> units;
	for (const Train *u = head; u != nullptr; u = u->GetNextUnit()) {
		ImStripUnit su;
		su.len8 = std::max<int>(1, u->gcache.cached_veh_length);
		su.engine = u->GetEngine()->VehInfo<RailVehicleInfo>().railveh_type != RAILVEH_WAGON;
		su.fill = u->cargo_cap > 0 && IsValidCargoType(u->cargo_type) ? CargoRgb(u->cargo_type) : COL_CH_TILE;
		su.selected = sel == u->index;
		units.push_back(su);
		ids.push_back(u->index);
	}
	return ImWndUnitStrip(units);
}

static void FleetMarkUnit(MiniWnd &mw, VehicleID target, bool attach)
{
	const Vehicle *mv = Vehicle::GetIfValid(mw.sel);
	const Vehicle *cv = Vehicle::GetIfValid(target);
	if (cv == nullptr) {
		mw.sel = VehicleID::Invalid();
	} else if (attach && mv != nullptr && mv->type == VEH_TRAIN && cv->type == VEH_TRAIN &&
			mv->First() != cv->First() && mv->tile == cv->tile) {
		Command<CMD_MOVE_RAIL_VEHICLE>::Post(STR_ERROR_CAN_T_MOVE_VEHICLE, mv->tile, mv->index, cv->Last()->index, mv->First() == mv);
		mw.sel = VehicleID::Invalid();
	} else {
		mw.sel = mw.sel == target ? VehicleID::Invalid() : target;
	}
}

/* Names are edited where they are shown: the label swaps for a text field in
 * place, Enter commits and anything else leaves the name alone. */
static void ImWndNameEditBegin(MiniWnd &mw, std::string_view name)
{
	size_t n = std::min(name.size(), sizeof(mw.name_buf) - 1);
	std::char_traits<char>::copy(mw.name_buf, name.data(), n);
	mw.name_buf[n] = '\0';
	mw.focus_name = true;
}

/* Returns 1 once the name is committed, -1 once the edit is abandoned. */
static int ImWndNameEdit(MiniWnd &mw, float width)
{
	ImGui::SetNextItemWidth(width);
	if (mw.focus_name) {
		ImGui::SetKeyboardFocusHere();
		mw.focus_name = false;
	}
	if (ImGui::InputText("##edit", mw.name_buf, sizeof(mw.name_buf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) return 1;
	return ImGui::IsItemDeactivated() ? -1 : 0;
}

static void ImVehicleBody(MiniWnd &mw, const Vehicle *v)
{
	bool own = v->owner == _local_company;
	switch (mw.tab) {
		case 0: {
			if (v->vehstatus.Test(VehState::Crashed)) {
				ImWndText(WndOfficial(STR_VEHICLE_STATUS_CRASHED), COL_CH_RED);
			} else if (v->vehstatus.Test(VehState::Stopped)) {
				ImWndText(WndOfficial(STR_VEHICLE_STATUS_STOPPED), COL_CH_RED);
			} else if (v->current_order.IsType(OT_GOTO_STATION)) {
				ImWndText(StrMakeValid(GetString(STR_STATION_NAME, v->current_order.GetDestination().ToStationID()), {}), COL_CH_ACCENT);
			} else if (v->current_order.IsType(OT_GOTO_DEPOT)) {
				ImWndText("차고로 이동 중", COL_CH_ACCENT);
			} else {
				ImWndText("-", COL_CH_DIM);
			}
			ImWndKV("속도", fmt::format("{} / {}", v->GetDisplaySpeed(), v->GetDisplayMaxSpeed()), COL_CH_TEXT);
			ImWndText(StrMakeValid(GetString(STR_VEHICLE_INFO_RELIABILITY_BREAKDOWNS, v->reliability * 100 >> 16, v->breakdowns_since_last_service), {}), COL_CH_TEXT);
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
				ImWndKV(WndOfficial(CargoSpec::Get(ct)->name), fmt::format("{} / {}", stored, cap), COL_CH_TEXT);
			}
			if (!any) ImWndText("적재 화물 없음", COL_CH_DIM);
			if (own && v->IsStoppedInDepot()) {
				CargoTypes mask = 0;
				for (const Vehicle *u = v; u != nullptr; u = u->Next()) {
					mask |= u->GetEngine()->info.refit_mask;
				}
				bool any_ref = false;
				for (const CargoSpec *cs : _sorted_cargo_specs) {
					if (!HasBit(mask, cs->Index())) continue;
					if (!any_ref) {
						ImWndHeader("개조");
						any_ref = true;
					}
					bool cur = false;
					for (const Vehicle *u = v; u != nullptr; u = u->Next()) {
						if (u->cargo_cap > 0 && u->cargo_type == cs->Index()) cur = true;
					}
					if (ImWndLink(fmt::format("{}{}", cur ? "▶ " : "· ", WndOfficial(cs->name)), cur ? COL_CH_ACCENT : COL_CH_TEXT)) {
						Command<CMD_REFIT_VEHICLE>::Post(GetCmdRefitVehMsg(v->type), v->tile, v->index, cs->Index(), 0xFF, false, false, 0);
					}
				}
			}
			break;
		}

		case 2: {
			int nord = v->GetNumOrders();
			if (mw.sel_ord >= nord) mw.sel_ord = -1;
			if (nord == 0) ImWndText("주문 없음", COL_CH_DIM);
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
					if (ImWndLink(fmt::format("{}{}. {}", cur ? "▶ " : "", oi + 1, label), mw.sel_ord == oi ? COL_CH_ACCENT : (cur ? COL_CH_YELLOW : COL_CH_TEXT))) {
						mw.sel_ord = mw.sel_ord == oi ? -1 : (int16_t)oi;
					}
				}
				oi++;
			}
			if (own && mw.sel_ord >= 0) {
				const Order *so = v->GetOrder((VehicleOrderID)mw.sel_ord);
				if (so != nullptr) {
					ImWndHeader(fmt::format("{}번 주문", mw.sel_ord + 1));
					if (ImWndLink("위로", COL_CH_TEXT)) {
						int to = mw.sel_ord - 1;
						if (to >= 0 && Command<CMD_MOVE_ORDER>::Post(STR_ERROR_CAN_T_MOVE_THIS_ORDER, v->tile, v->index, (VehicleOrderID)mw.sel_ord, (VehicleOrderID)to)) {
							mw.sel_ord = (int16_t)to;
						}
					}
					if (ImWndLink("아래로", COL_CH_TEXT)) {
						int to = mw.sel_ord + 1;
						if (to < v->GetNumOrders() && Command<CMD_MOVE_ORDER>::Post(STR_ERROR_CAN_T_MOVE_THIS_ORDER, v->tile, v->index, (VehicleOrderID)mw.sel_ord, (VehicleOrderID)to)) {
							mw.sel_ord = (int16_t)to;
						}
					}
					if (ImWndLink("여기로 건너뛰기", COL_CH_TEXT)) {
						Command<CMD_SKIP_TO_ORDER>::Post(STR_ERROR_CAN_T_SKIP_TO_ORDER, v->tile, v->index, (VehicleOrderID)mw.sel_ord);
					}
					if (so->IsType(OT_GOTO_STATION)) {
						if (ImWndKVLink("적재", WndOfficial(OrderLoadStr(so->GetLoadType())), COL_CH_DIM, COL_CH_TEXT)) {
							OrderLoadType next;
							switch (so->GetLoadType()) {
								case OrderLoadType::LoadIfPossible: next = OrderLoadType::FullLoad; break;
								case OrderLoadType::FullLoad: next = OrderLoadType::FullLoadAny; break;
								case OrderLoadType::FullLoadAny: next = OrderLoadType::NoLoad; break;
								default: next = OrderLoadType::LoadIfPossible; break;
							}
							Command<CMD_MODIFY_ORDER>::Post(STR_ERROR_CAN_T_MODIFY_THIS_ORDER, v->tile, v->index, (VehicleOrderID)mw.sel_ord, MOF_LOAD, to_underlying(next));
						}
						if (ImWndKVLink("하차", WndOfficial(OrderUnloadStr(so->GetUnloadType())), COL_CH_DIM, COL_CH_TEXT)) {
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
					if (ImWndLink("삭제", COL_CH_RED)) {
						Command<CMD_DELETE_ORDER>::Post(STR_ERROR_CAN_T_DELETE_THIS_ORDER, v->tile, v->index, (VehicleOrderID)mw.sel_ord);
						mw.sel_ord = -1;
					}
				}
			}
			if (own) {
				bool picking = _order_pick_veh == v->index;
				if (ImWndLink(picking ? "추가 중. 지도에서 목적지 클릭, ESC 종료" : "+ 목적지 추가", COL_CH_ACCENT)) {
					if (picking) EnterIdleMode(); else EnterOrderPickMode(v->index);
				}
			}
			break;
		}

		case 3: {
			ImWndKV("구매", fmt::format("{}년", v->build_year.base()), COL_CH_TEXT);
			Money value = 0;
			for (const Vehicle *u = v; u != nullptr; u = u->Next()) value += u->value;
			ImWndKV("가치", GetString(STR_JUST_CURRENCY_LONG, value), COL_CH_TEXT);
			ImWndKV("유지비", fmt::format("{}/년", GetString(STR_JUST_CURRENCY_LONG, v->GetDisplayRunningCost())), COL_CH_TEXT);
			ImWndKV("차령", fmt::format("{}년 / {}년", v->age.base() / 366, v->max_age.base() / 366), COL_CH_TEXT);
			ImWndText(StrMakeValid(GetString(STR_VEHICLE_INFO_PROFIT_THIS_YEAR_LAST_YEAR, v->GetDisplayProfitThisYear(), v->GetDisplayProfitLastYear()), {}), COL_CH_TEXT);
			if (v->type == VEH_TRAIN) {
				ImWndKV("총길이", fmt::format("{:.1f}타일", Train::From(v)->gcache.cached_total_length / (double)TILE_SIZE), COL_CH_TEXT);
			}
			if (v->type == VEH_TRAIN || (v->type == VEH_ROAD && _settings_game.vehicle.roadveh_acceleration_model != AM_ORIGINAL)) {
				const GroundVehicleCache *gc = v->GetGroundVehicleCache();
				int64_t ms = PackVelocity(v->GetDisplayMaxSpeed(), v->type);
				if (v->type == VEH_TRAIN && (_settings_game.vehicle.train_acceleration_model == AM_ORIGINAL ||
						Train::From(v)->GetAccelerationType() == VehicleAccelerationModel::Maglev)) {
					ImWndText(StrMakeValid(GetString(STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED, gc->cached_weight, gc->cached_power, ms), {}), COL_CH_TEXT);
				} else {
					ImWndText(StrMakeValid(GetString(STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED_MAX_TE, gc->cached_weight, gc->cached_power, ms, gc->cached_max_te), {}), COL_CH_TEXT);
				}
			}
			break;
		}
	}
}

static void ImStationBody(MiniWnd &mw, const Station *st)
{
	switch (mw.tab) {
		case 0: {
			bool any = false;
			for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
				const GoodsEntry &ge = st->goods[cs->Index()];
				if (!ge.HasRating()) continue;
				any = true;
				ImWndKV(WndOfficial(cs->name), fmt::format("{}", ge.TotalCount()), COL_CH_TEXT);
			}
			if (!any) ImWndText("대기 화물 없음", COL_CH_DIM);
			break;
		}

		case 1: {
			bool any = false;
			std::vector<const Industry *> supply;
			for (const Industry *i : Industry::Iterate()) {
				if (i->stations_near.find(const_cast<Station *>(st)) == i->stations_near.end()) continue;
				if (std::none_of(std::begin(i->produced), std::end(i->produced), [](const auto &p) { return IsValidCargoType(p.cargo); })) continue;
				supply.push_back(i);
			}
			if (!supply.empty()) {
				any = true;
				ImWndHeader("공급처");
				for (const Industry *i : supply) {
					if (ImWndLink(StrMakeValid(GetString(STR_INDUSTRY_NAME, i->index), {}), COL_CH_TEXT)) {
						MiniUiScrollTo(TileX(i->location.tile) * TILE_SIZE, TileY(i->location.tile) * TILE_SIZE);
					}
				}
			}
			if (!st->industries_near.empty()) {
				any = true;
				ImWndHeader("납품처");
				for (const IndustryListEntry &e : st->industries_near) {
					if (ImWndLink(StrMakeValid(GetString(STR_INDUSTRY_NAME, e.industry->index), {}), COL_CH_TEXT)) {
						TileIndex jt = e.industry->location.tile;
						MiniUiScrollTo(TileX(jt) * TILE_SIZE, TileY(jt) * TILE_SIZE);
					}
				}
			}
			if (!any) ImWndText("주변 산업 없음", COL_CH_DIM);
			break;
		}

		case 2: {
			bool any = false;
			for (VehicleType vt : {VEH_TRAIN, VEH_ROAD, VEH_SHIP, VEH_AIRCRAFT}) {
				VehicleList list;
				if (!GenerateVehicleSortList(&list, VehicleListIdentifier(VL_STATION_LIST, vt, _local_company, st->index))) continue;
				for (const Vehicle *lv : list) {
					any = true;
					bool here = lv->current_order.IsType(OT_GOTO_STATION) && lv->current_order.GetDestination().ToStationID() == st->index;
					if (ImWndLink(StrMakeValid(GetString(STR_VEHICLE_NAME, lv->index), {}), here ? COL_CH_ACCENT : COL_CH_TEXT)) {
						OpenMiniWnd(MiniWndKind::Vehicle, lv->First()->index, StationID::Invalid());
					}
				}
			}
			if (!any) ImWndText("이 역에 오는 차량 없음", COL_CH_DIM);
			break;
		}

		case 3: {
			if (st->town != nullptr) {
				if (ImWndKVLink("도시", StrMakeValid(GetString(STR_TOWN_NAME, st->town->index), {}), COL_CH_DIM, COL_CH_TEXT)) {
					OpenMiniWnd(MiniWndKind::Town, VehicleID::Invalid(), StationID::Invalid(), st->town->index);
				}
			}
			if (Company::IsValidID(st->owner)) {
				ImWndKV("소유", StrMakeValid(GetString(STR_COMPANY_NAME, st->owner), {}), COL_CH_TEXT);
			}
			std::string fac;
			for (const auto &[f, name] : std::initializer_list<std::pair<StationFacility, std::string_view>>{
					{StationFacility::Train, "철도"}, {StationFacility::BusStop, "버스"}, {StationFacility::TruckStop, "트럭"},
					{StationFacility::Dock, "부두"}, {StationFacility::Airport, "공항"}}) {
				if (!st->facilities.Test(f)) continue;
				if (!fac.empty()) fac += " ";
				fac += name;
			}
			if (!fac.empty()) ImWndKV("시설", fac, COL_CH_TEXT);
			ImWndText(StrMakeValid(GetString(STR_LAND_AREA_INFORMATION_BUILD_DATE, st->build_date), {}), COL_CH_TEXT);
			if (st->facilities.Test(StationFacility::Train) && st->train_station.tile != INVALID_TILE) {
				uint longest = 0;
				for (TileIndex ti : st->train_station) {
					if (!st->TileBelongsToRailStation(ti)) continue;
					longest = std::max(longest, st->GetPlatformLength(ti));
				}
				ImWndKV(WndOfficial(STR_STATION_BUILD_PLATFORM_LENGTH), fmt::format("{}칸", longest), COL_CH_TEXT);
			}
			ImWndText(StrMakeValid(GetString(STR_STATION_VIEW_ACCEPTS_CARGO, GetAcceptanceMask(st)), {}), COL_CH_TEXT);
			bool rated = false;
			for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
				const GoodsEntry &ge = st->goods[cs->Index()];
				if (!ge.HasRating()) continue;
				if (!rated) {
					ImWndHeader("화물 처리 평가");
					rated = true;
				}
				uint pct = ToPercent8(ge.rating);
				uint32_t tint = pct < 25 ? COL_CH_RED : pct < 50 ? COL_CH_YELLOW : COL_CH_TEXT;
				ImWndKV(WndOfficial(cs->name), fmt::format("{} {}%", WndOfficial(STR_CARGO_RATING_APPALLING + (ge.rating >> 5)), pct), tint);
			}
			break;
		}
	}
}

static TownActions MiniEnabledTownActions()
{
	TownActions enabled{};
	enabled.Set();
	if (!_settings_game.economy.fund_roads) enabled.Reset(TownAction::RoadRebuild);
	if (!_settings_game.economy.fund_buildings) enabled.Reset(TownAction::FundBuildings);
	if (!_settings_game.economy.exclusive_rights) enabled.Reset(TownAction::BuyRights);
	if (!_settings_game.economy.bribe) enabled.Reset(TownAction::Bribe);
	return enabled;
}

static void ImTownBody(MiniWnd &mw, const Town *t)
{
	switch (mw.tab) {
		case 0: {
			ImWndText(StrMakeValid(GetString(STR_TOWN_VIEW_POPULATION_HOUSES, t->cache.population, t->cache.num_houses), {}), COL_CH_TEXT);
			if (t->flags.Test(TownFlag::IsGrowing)) {
				StringID str = t->fund_buildings_months == 0 ? STR_TOWN_VIEW_TOWN_GROWS_EVERY : STR_TOWN_VIEW_TOWN_GROWS_EVERY_FUNDED;
				ImWndText(StrMakeValid(GetString(str, RoundDivSU(t->growth_rate + 1, Ticks::DAY_TICKS)), {}), COL_CH_TEXT);
			} else {
				ImWndText(WndOfficial(STR_TOWN_VIEW_TOWN_GROW_STOPPED), COL_CH_YELLOW);
			}
			if (t->larger_town) ImWndText("대도시", COL_CH_ACCENT);
			if (_settings_game.economy.station_noise_level) {
				ImWndText(StrMakeValid(GetString(STR_TOWN_VIEW_NOISE_IN_TOWN, t->noise_reached, t->MaxTownNoise()), {}), COL_CH_TEXT);
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
				ImWndKV(name, WndOfficial(TownRatingString(rating)), tint);
			}
			if (!any) ImWndText("회사 평가 없음", COL_CH_DIM);

			ImWndHeader(WndOfficial(STR_LOCAL_AUTHORITY_ACTIONS_TITLE));
			TownActions enabled = MiniEnabledTownActions();
			TownActions avail = GetMaskOfTownActions(_local_company, t);
			for (TownAction a = {}; a != TownAction::End; ++a) {
				if (!enabled.Test(a)) continue;
				Money price = _price[PR_TOWN_ACTION] * GetTownActionCost(a) >> 8;
				bool can = avail.Test(a);
				bool sel = mw.sel_ord == (int16_t)to_underlying(a);
				uint32_t tint = sel ? COL_CH_ACCENT : can ? COL_CH_TEXT : COL_CH_DIM;
				std::string label = WndOfficial(STR_LOCAL_AUTHORITY_ACTION_SMALL_ADVERTISING_CAMPAIGN + to_underlying(a));
				std::string cost = StrMakeValid(GetString(STR_JUST_CURRENCY_LONG, price), {});
				if (ImWndKVLink(label, cost, tint, tint) && can) mw.sel_ord = sel ? -1 : (int16_t)to_underlying(a);
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
					ImWndText(StrMakeValid(GetString(str_last, 1ULL << ct, transported, production), {}), COL_CH_TEXT);
				}
			}
			bool first = true;
			for (int i = TAE_BEGIN; i < TAE_END; i++) {
				if (t->goal[i] == 0) continue;
				if (t->goal[i] == TOWN_GROWTH_WINTER && (TileHeight(t->xy) < LowestSnowLine() || t->cache.population <= 90)) continue;
				if (t->goal[i] == TOWN_GROWTH_DESERT && (GetTropicZone(t->xy) != TROPICZONE_DESERT || t->cache.population <= 60)) continue;
				if (first) {
					ImWndHeader(WndOfficial(STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH));
					first = false;
				}
				const CargoSpec *cargo = FindFirstCargoWithTownAcceptanceEffect((TownAcceptanceEffect)i);
				if (cargo == nullptr) continue;
				if (t->goal[i] == TOWN_GROWTH_DESERT || t->goal[i] == TOWN_GROWTH_WINTER) {
					bool done = t->received[i].old_act > 0;
					ImWndKV(WndOfficial(cargo->name), done ? "공급됨" : "필요", done ? COL_CH_TEXT : COL_CH_YELLOW);
				} else {
					bool done = t->received[i].old_act >= t->goal[i];
					ImWndKV(WndOfficial(cargo->name), fmt::format("{} / {}", t->received[i].old_act, t->goal[i]), done ? COL_CH_TEXT : COL_CH_YELLOW);
				}
			}
			break;
		}
	}
}

static void ImIndustryBody(MiniWnd &mw, const Industry *i)
{
	switch (mw.tab) {
		case 0: {
			if (i->prod_level == PRODLEVEL_CLOSURE) ImWndText(WndOfficial(STR_INDUSTRY_VIEW_INDUSTRY_ANNOUNCED_CLOSURE), COL_CH_RED);
			bool any = false;
			for (const auto &p : i->produced) {
				if (!IsValidCargoType(p.cargo)) continue;
				any = true;
				uint pct = ToPercent8(p.history[LAST_MONTH].PctTransported());
				uint32_t tint = pct < 25 ? COL_CH_RED : pct < 50 ? COL_CH_YELLOW : COL_CH_TEXT;
				ImWndKV(WndOfficial(CargoSpec::Get(p.cargo)->name), fmt::format("{} · {}%", p.history[LAST_MONTH].production, pct), tint);
			}
			if (!any) ImWndText("생산 없음", COL_CH_DIM);
			if (i->prod_level != PRODLEVEL_DEFAULT && i->prod_level != PRODLEVEL_CLOSURE) {
				ImWndText(StrMakeValid(GetString(STR_INDUSTRY_VIEW_PRODUCTION_LEVEL, RoundDivSU(i->prod_level * 100, PRODLEVEL_DEFAULT)), {}), COL_CH_TEXT);
			}
			break;
		}

		case 1: {
			if (i->stations_near.empty()) {
				ImWndText("주변 역 없음", COL_CH_DIM);
				break;
			}
			for (const Station *st : i->stations_near) {
				if (ImWndLink(StrMakeValid(GetString(STR_STATION_NAME, st->index), {}), COL_CH_TEXT)) {
					OpenMiniWnd(MiniWndKind::Station, VehicleID::Invalid(), st->index);
				}
			}
			break;
		}

		case 2: {
			ImWndText(StrMakeValid(GetString(STR_LAND_AREA_INFORMATION_BUILD_DATE, i->construction_date), {}), COL_CH_TEXT);
			bool first = true;
			for (const auto &a : i->accepted) {
				if (!IsValidCargoType(a.cargo)) continue;
				if (first) {
					ImWndHeader(WndOfficial(STR_INDUSTRY_VIEW_REQUIRES));
					first = false;
				}
				ImWndKV(WndOfficial(CargoSpec::Get(a.cargo)->name), a.waiting > 0 ? fmt::format("{}", a.waiting) : std::string("-"), COL_CH_TEXT);
			}
			break;
		}
	}
}

static void ImFleetBody(MiniWnd &mw)
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

	float lw = ImGui::GetContentRegionAvail().x * 0.45f;
	ImGui::BeginChild("buy", ImVec2(lw, 0.0f));
	{
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
				if (ImWndKVLink(r.name, GetString(STR_JUST_CURRENCY_LONG, r.cost), COL_CH_TEXT, COL_CH_TEXT)) {
					if (vt == VEH_TRAIN) {
						draft.push_back(r.eid);
					} else {
						draft.assign(1, r.eid);
					}
				}
			}
		};
		if (vt == VEH_TRAIN) {
			if (!locos.empty()) ImWndHeader("기관차");
			engine_rows(locos);
			if (!wags.empty()) ImWndHeader("화차");
			engine_rows(wags);
		} else {
			ImWndHeader("엔진");
			engine_rows(locos);
		}
		if (locos.empty() && wags.empty()) ImWndText("구매 가능 엔진 없음", COL_CH_DIM);
	}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("yard", ImVec2(0.0f, 0.0f));
	{
		ImWndHeader("설계");
		if (draft.empty()) {
			ImWndText(vt == VEH_TRAIN ? "엔진 목록을 눌러 편성 구성" : "엔진 목록을 눌러 선택", COL_CH_DIM);
		} else {
			std::vector<ImStripUnit> units;
			Money total = 0;
			uint64_t power = 0, weight = 0;
			uint16_t speed = 0;
			uint cap = 0;
			int len8 = 0;
			for (size_t i = 0; i < draft.size(); i++) {
				const Engine *e = Engine::Get(draft[i]);
				total += e->GetCost();
				power += e->GetPower();
				weight += e->GetDisplayWeight();
				uint16_t espd = e->GetDisplayMaxSpeed();
				if (espd > 0) speed = speed == 0 ? espd : std::min(speed, espd);
				cap += e->GetDisplayDefaultCapacity();
				ImStripUnit su;
				if (vt == VEH_TRAIN) {
					const RailVehicleInfo &rvi = e->VehInfo<RailVehicleInfo>();
					su.engine = rvi.railveh_type != RAILVEH_WAGON;
					su.len8 = 8 - rvi.shorten_factor;
				}
				len8 += su.len8;
				CargoType dc = e->GetDefaultCargoType();
				su.fill = e->GetDisplayDefaultCapacity() > 0 && IsValidCargoType(dc) ? CargoRgb(dc) : COL_CH_TILE;
				units.push_back(su);
			}
			int del = ImWndUnitStrip(units);
			if (del >= 0 && (size_t)del < draft.size()) draft.erase(draft.begin() + del);
			ImWndKV("합계", GetString(STR_JUST_CURRENCY_LONG, total), COL_CH_ACCENT);
			if (vt == VEH_TRAIN) {
				ImWndText(StrMakeValid(GetString(STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED, weight, power, PackVelocity(speed, vt)), {}), COL_CH_TEXT);
				ImWndKV("길이", fmt::format("{:.1f}타일", len8 / 16.0), COL_CH_TEXT);
			}
			if (cap > 0) ImWndKV("용량", fmt::format("{}", cap), COL_CH_TEXT);
			ImWndText("차고 행 클릭으로 생산 · 블록 클릭으로 제외", COL_CH_DIM);
		}
		if (_deploy.depot != INVALID_TILE && _deploy.vt == vt) {
			ImWndText(fmt::format("생산 중 {} / {}", std::min(_deploy.next + 1, _deploy.units.size()), _deploy.units.size()), COL_CH_ACCENT);
		}

		ImWndHeader("차고");
		if (sv != nullptr && vt == VEH_TRAIN) {
			ImWndText("표시 차량: 같은 차고 편성 클릭으로 연결", COL_CH_ACCENT);
			if (sv->First() != sv) {
				if (ImWndLink("새 편성으로 분리", COL_CH_ACCENT)) {
					Command<CMD_MOVE_RAIL_VEHICLE>::Post(STR_ERROR_CAN_T_MOVE_VEHICLE, sv->tile, sv->index, VehicleID::Invalid(), false);
					mw.sel = VehicleID::Invalid();
				}
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
			if (ImWndLink(label, mw.sel == head->index ? COL_CH_ACCENT : (stopped ? COL_CH_TEXT : COL_CH_YELLOW))) {
				FleetMarkUnit(mw, head->index, true);
			}
			if (vt == VEH_TRAIN) {
				std::vector<VehicleID> ids;
				int hit = ImTrainStrip(Train::From(head), mw.sel, ids);
				if (hit >= 0 && (size_t)hit < ids.size()) FleetMarkUnit(mw, ids[hit], false);
			}
		};
		auto depot_block = [&](TileIndex tile, uint dest) {
			anydep = true;
			VehicleList chains, wagons;
			BuildDepotVehicleList(vt, tile, &chains, &wagons);
			std::string dn = StrMakeValid(GetString(STR_DEPOT_NAME, vt, dest), {});
			if (ImWndLink(draft.empty() ? dn : fmt::format("▶ {} 생산", dn), draft.empty() ? COL_CH_TEXT : COL_CH_ACCENT)) {
				if (!IsDepotTile(tile)) {
					/* stale row */
				} else if (draft.empty()) {
					MiniUiScrollTo(TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE);
				} else if (_deploy.depot == INVALID_TILE) {
					_deploy = FleetDeploy{};
					_deploy.depot = tile;
					_deploy.vt = vt;
					_deploy.units = draft;
				}
			}
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
		if (!anydep) ImWndText("차고 없음", COL_CH_DIM);
	}
	ImGui::EndChild();
}

static void ImFinanceBody(uint8_t tab)
{
	const Company *c = Company::GetIfValid(_local_company);
	if (c == nullptr) return;

	if (tab == 0) {
		ImWndKV(WndOfficial(STR_FINANCES_BANK_BALANCE_TITLE), StrMakeValid(GetString(STR_JUST_CURRENCY_LONG, c->money), {}), COL_CH_ACCENT);
		ImWndKV(WndOfficial(STR_FINANCES_OWN_FUNDS_TITLE), StrMakeValid(GetString(STR_JUST_CURRENCY_LONG, c->money - c->current_loan), {}), COL_CH_TEXT);
		ImWndKV(WndOfficial(STR_FINANCES_LOAN_TITLE), StrMakeValid(GetString(STR_JUST_CURRENCY_LONG, c->current_loan), {}), c->current_loan > 0 ? COL_CH_YELLOW : COL_CH_TEXT);
		ImWndText(StrMakeValid(GetString(STR_FINANCES_MAX_LOAN, c->GetMaxLoan()), {}), COL_CH_TEXT);
		ImWndText(StrMakeValid(GetString(STR_FINANCES_INTEREST_RATE, _economy.interest_rate), {}), COL_CH_TEXT);
		ImWndKV("회사 가치", StrMakeValid(GetString(STR_JUST_CURRENCY_LONG, CalculateCompanyValue(c)), {}), COL_CH_TEXT);
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
		ImWndHeader(WndOfficial(cat.title));
		Money sum = 0;
		for (ExpensesType et : cat.items) {
			Money cost = tbl[et];
			sum += cost;
			if (cost == 0) continue;
			ImWndKV(WndOfficial(STR_FINANCES_SECTION_CONSTRUCTION + et), MiniPriceStr(cost), cost > 0 ? COL_CH_RED : COL_CH_TEXT);
		}
		total += sum;
		ImWndKV("합계", MiniPriceStr(sum), sum > 0 ? COL_CH_RED : COL_CH_TEXT);
	}
	ImWndHeader(WndOfficial(STR_FINANCES_TOTAL_CAPTION));
	ImWndKV("올해 손익", MiniPriceStr(total), total > 0 ? COL_CH_RED : COL_CH_ACCENT);
}

static void ImCompanyBody(MiniWnd &mw)
{
	const Company *c = Company::GetIfValid(_local_company);
	if (c == nullptr) return;

	if (mw.tab == 1) {
		static const StringID veh_strs[] = {STR_REPLACE_VEHICLE_TRAIN, STR_REPLACE_VEHICLE_ROAD_VEHICLE, STR_REPLACE_VEHICLE_SHIP, STR_REPLACE_VEHICLE_AIRCRAFT};
		ImWndHeader(WndOfficial(STR_COMPANY_VIEW_VEHICLES_TITLE));
		uint fleet = 0;
		for (VehicleType vt = VEH_BEGIN; vt < VEH_COMPANY_END; vt++) {
			uint amount = c->group_all[vt].num_vehicle;
			fleet += amount;
			ImWndKV(WndOfficial(veh_strs[vt]), fmt::format("{}대", amount), amount > 0 ? COL_CH_TEXT : COL_CH_DIM);
		}
		if (fleet == 0) ImWndText(WndOfficial(STR_COMPANY_VIEW_VEHICLES_NONE), COL_CH_DIM);

		ImWndHeader(WndOfficial(STR_COMPANY_VIEW_INFRASTRUCTURE));
		uint rail = c->infrastructure.GetRailTotal() + c->infrastructure.signal;
		uint road = c->infrastructure.GetRoadTotal() + c->infrastructure.GetTramTotal();
		ImWndKV("선로", fmt::format("{}", rail), rail > 0 ? COL_CH_TEXT : COL_CH_DIM);
		ImWndKV("도로", fmt::format("{}", road), road > 0 ? COL_CH_TEXT : COL_CH_DIM);
		ImWndKV("수로", fmt::format("{}", c->infrastructure.water), c->infrastructure.water > 0 ? COL_CH_TEXT : COL_CH_DIM);
		ImWndKV("역 타일", fmt::format("{}", c->infrastructure.station), c->infrastructure.station > 0 ? COL_CH_TEXT : COL_CH_DIM);
		ImWndKV("공항", fmt::format("{}", c->infrastructure.airport), c->infrastructure.airport > 0 ? COL_CH_TEXT : COL_CH_DIM);
		return;
	}

	/* The manager name is edited on its own row, so the caption stays the
	 * company name. */
	if (mw.rename_pres) {
		ImGui::PushID("pres");
		int r = ImWndNameEdit(mw, ImGui::GetContentRegionAvail().x);
		ImGui::PopID();
		if (r == 1 && mw.name_buf[0] != '\0') Command<CMD_RENAME_PRESIDENT>::Post(STR_ERROR_CAN_T_CHANGE_PRESIDENT, mw.name_buf);
		if (r != 0) mw.rename_pres = false;
	} else {
		std::string pres = StrMakeValid(GetString(STR_PRESIDENT_NAME, c->index), {});
		ImWndKV("사장", pres, COL_CH_TEXT);
		if (ImGui::IsItemHovered()) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				ImWndNameEditBegin(mw, pres);
				mw.rename_pres = true;
				mw.renaming = false;
			}
		}
	}

	if (TimerGameEconomy::UsingWallclockUnits()) {
		ImWndKV("설립", fmt::format("{} · {}년", c->inaugurated_year.base(), c->inaugurated_year_calendar.base()), COL_CH_TEXT);
	} else {
		ImWndKV("설립", fmt::format("{}년", c->inaugurated_year.base()), COL_CH_TEXT);
	}
	ImWndKV("회사 가치", StrMakeValid(GetString(STR_JUST_CURRENCY_LONG, CalculateCompanyValue(c)), {}), COL_CH_TEXT);
	int perf = c->old_economy[0].performance_history;
	ImWndKV("성능 지수", fmt::format("{}/1000", perf), perf >= 500 ? COL_CH_ACCENT : COL_CH_TEXT);
	ImWndKV("본사", c->location_of_HQ == INVALID_TILE ? "없음" : "있음", c->location_of_HQ == INVALID_TILE ? COL_CH_DIM : COL_CH_TEXT);
}

static void ImGroupBody(MiniWnd &mw)
{
	if (!Company::IsValidID(_local_company)) return;
	VehicleType vt = (VehicleType)mw.tab;

	bool special = mw.sel_grp == ALL_GROUP || mw.sel_grp == DEFAULT_GROUP;
	const Group *sg = special ? nullptr : Group::GetIfValid(mw.sel_grp);
	if (!special && (sg == nullptr || sg->owner != _local_company || sg->vehicle_type != vt)) {
		mw.sel_grp = ALL_GROUP;
		special = true;
	}
	const Engine *se = Engine::GetIfValid(mw.sel_eng);
	if (se != nullptr && se->type != vt) {
		mw.sel_eng = EngineID::Invalid();
		se = nullptr;
	}

	/* A rename left open on a group this tab no longer lists would edit a row
	 * that is never drawn. */
	const Group *rg = Group::GetIfValid(mw.rename_grp);
	if (rg != nullptr && (rg->owner != _local_company || rg->vehicle_type != vt)) rg = nullptr;
	if (rg == nullptr) mw.rename_grp = GroupID::Invalid();

	float lw = ImGui::GetContentRegionAvail().x * 0.45f;
	ImGui::BeginChild("groups", ImVec2(lw, 0.0f));
	{
		ImWndHeader("그룹");
		auto group_row = [&](GroupID gid, std::string_view name, uint count) {
			if (mw.rename_grp == gid) {
				ImGui::PushID((int)gid.base());
				int r = ImWndNameEdit(mw, ImGui::GetContentRegionAvail().x);
				ImGui::PopID();
				if (r == 1 && mw.name_buf[0] != '\0') {
					Command<CMD_ALTER_GROUP>::Post(STR_ERROR_GROUP_CAN_T_RENAME, AlterGroupMode::Rename, gid, GroupID::Invalid(), mw.name_buf);
				}
				if (r != 0) mw.rename_grp = GroupID::Invalid();
				return;
			}
			if (ImWndLink(fmt::format("{}{} · {}대", mw.sel_grp == gid ? "▶ " : "· ", name, count),
					mw.sel_grp == gid ? COL_CH_ACCENT : COL_CH_TEXT)) {
				mw.sel_grp = gid;
				mw.sel_eng = EngineID::Invalid();
			}
			if (Group::IsValidID(gid) && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				ImWndNameEditBegin(mw, name);
				mw.rename_grp = gid;
				mw.renaming = false;
			}
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

		ImWndHeader("자동 교체");
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
			if (ImWndLink(label, mw.sel_eng == e->index ? COL_CH_ACCENT : (repl != EngineID::Invalid() ? COL_CH_YELLOW : COL_CH_TEXT))) {
				mw.sel_eng = mw.sel_eng == e->index ? EngineID::Invalid() : e->index;
			}
		}
		if (!any_used) ImWndText("보유 엔진 없음", COL_CH_DIM);
		if (Engine::GetIfValid(mw.sel_eng) != nullptr) {
			EngineID repl = EngineReplacementForCompany(comp, mw.sel_eng, mw.sel_grp);
			if (repl != EngineID::Invalid()) {
				if (ImWndLink("교체 해제", COL_CH_RED)) {
					Command<CMD_SET_AUTOREPLACE>::Post(mw.sel_grp, mw.sel_eng, EngineID::Invalid(), false);
				}
			}
			ImWndText("교체할 새 엔진 클릭", COL_CH_DIM);
			for (const Engine *e : Engine::IterateType(vt)) {
				if (e->index == mw.sel_eng) continue;
				if (!CheckAutoreplaceValidity(mw.sel_eng, e->index, _local_company)) continue;
				if (ImWndLink(fmt::format("· {}", StrMakeValid(GetString(STR_ENGINE_NAME, e->index), {})),
						e->index == repl ? COL_CH_ACCENT : COL_CH_TEXT)) {
					Command<CMD_SET_AUTOREPLACE>::Post(mw.sel_grp, mw.sel_eng, e->index, false);
				}
			}
		}
	}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("vehicles", ImVec2(0.0f, 0.0f));
	{
		if (special) {
			ImWndHeader("차량");
			bool anyv = false;
			for (const Vehicle *v : Vehicle::Iterate()) {
				if (v->type != vt || !v->IsPrimaryVehicle() || v->owner != _local_company) continue;
				if (mw.sel_grp == DEFAULT_GROUP && v->group_id != DEFAULT_GROUP) continue;
				anyv = true;
				if (ImWndLink(StrMakeValid(GetString(STR_VEHICLE_NAME, v->index), {}), COL_CH_TEXT)) {
					OpenMiniWnd(MiniWndKind::Vehicle, v->First()->index, StationID::Invalid());
				}
			}
			if (!anyv) ImWndText("차량 없음", COL_CH_DIM);
		} else {
			ImWndHeader("소속 차량. 클릭으로 제외");
			bool anyin = false;
			for (const Vehicle *v : Vehicle::Iterate()) {
				if (v->type != vt || !v->IsPrimaryVehicle() || v->owner != _local_company || v->group_id != mw.sel_grp) continue;
				anyin = true;
				if (ImWndLink(StrMakeValid(GetString(STR_VEHICLE_NAME, v->index), {}), COL_CH_TEXT)) {
					Command<CMD_ADD_VEHICLE_GROUP>::Post(STR_ERROR_GROUP_CAN_T_ADD_VEHICLE, DEFAULT_GROUP, v->index, false, VehicleListIdentifier{});
				}
			}
			if (!anyin) ImWndText("소속 차량 없음", COL_CH_DIM);
			ImWndHeader("클릭으로 추가");
			bool anyout = false;
			for (const Vehicle *v : Vehicle::Iterate()) {
				if (v->type != vt || !v->IsPrimaryVehicle() || v->owner != _local_company || v->group_id == mw.sel_grp) continue;
				anyout = true;
				if (ImWndLink(StrMakeValid(GetString(STR_VEHICLE_NAME, v->index), {}), COL_CH_TEXT)) {
					if (Group::IsValidID(mw.sel_grp)) {
						Command<CMD_ADD_VEHICLE_GROUP>::Post(STR_ERROR_GROUP_CAN_T_ADD_VEHICLE, mw.sel_grp, v->index, false, VehicleListIdentifier{});
					}
				}
			}
			if (!anyout) ImWndText("없음", COL_CH_DIM);
		}
	}
	ImGui::EndChild();
}

static uint32_t RatingTint(int rating)
{
	if (rating < 0) return COL_CH_DIM;
	if (rating <= RATING_VERYPOOR) return COL_CH_RED;
	if (rating <= RATING_MEDIOCRE) return COL_CH_YELLOW;
	return COL_CH_TEXT;
}

static void ImStationListBody(MiniWnd &mw)
{
	if (!Company::IsValidID(_local_company)) {
		ImWndText("회사 없음", COL_CH_DIM);
		return;
	}

	struct Row {
		StationID id;
		std::string name;
		uint waiting;
		int rating;
	};
	std::vector<Row> rows;
	for (const Station *st : Station::Iterate()) {
		if (st->owner != _local_company) continue;
		Row r{st->index, StrMakeValid(GetString(STR_STATION_NAME, st->index), {}), 0, -1};
		int sum = 0, n = 0;
		for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
			const GoodsEntry &ge = st->goods[cs->Index()];
			r.waiting += ge.TotalCount();
			if (!ge.HasRating()) continue;
			sum += ge.rating;
			n++;
		}
		if (n > 0) r.rating = sum / n;
		rows.push_back(std::move(r));
	}
	if (rows.empty()) {
		ImWndText("역 없음", COL_CH_DIM);
		return;
	}

	switch (mw.tab) {
		case 1: std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.waiting > b.waiting; }); break;
		case 2: std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.rating < b.rating; }); break;
		default: std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.name < b.name; }); break;
	}

	for (const Row &r : rows) {
		std::string value;
		uint32_t tint = COL_CH_TEXT;
		if (mw.tab == 2) {
			value = r.rating < 0 ? std::string("-") : fmt::format("{}%", ToPercent8(r.rating));
			tint = RatingTint(r.rating);
		} else {
			value = fmt::format("{}", r.waiting);
		}
		if (ImWndKVLink(r.name, value, COL_CH_TEXT, tint)) OpenMiniWnd(MiniWndKind::Station, VehicleID::Invalid(), r.id);
	}
}

static void ImTownListBody(MiniWnd &mw)
{
	struct Row {
		TownID id;
		std::string name;
		uint pop;
		int rating;
	};
	std::vector<Row> rows;
	bool company = Company::IsValidID(_local_company);
	for (const Town *t : Town::Iterate()) {
		Row r{t->index, StrMakeValid(GetString(STR_TOWN_NAME, t->index), {}), t->cache.population, -1};
		if (company && t->have_ratings.Test(_local_company)) r.rating = t->ratings[_local_company];
		rows.push_back(std::move(r));
	}
	if (rows.empty()) {
		ImWndText("도시 없음", COL_CH_DIM);
		return;
	}

	switch (mw.tab) {
		case 1: std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.pop > b.pop; }); break;
		case 2: std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.rating < b.rating; }); break;
		default: std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.name < b.name; }); break;
	}

	for (const Row &r : rows) {
		std::string value;
		uint32_t tint = COL_CH_TEXT;
		if (mw.tab == 2) {
			value = r.rating < 0 ? std::string("-") : WndOfficial(TownRatingString(r.rating));
			tint = RatingTint(r.rating);
		} else {
			value = fmt::format("{}", r.pop);
		}
		if (ImWndKVLink(r.name, value, COL_CH_TEXT, tint)) {
			OpenMiniWnd(MiniWndKind::Town, VehicleID::Invalid(), StationID::Invalid(), r.id);
		}
	}
}

static void ImIndustryListBody(MiniWnd &mw)
{
	struct Row {
		IndustryID id;
		std::string name;
		uint64_t prod;
		uint pct;
		bool closing;
	};
	std::vector<Row> rows;
	for (const Industry *i : Industry::Iterate()) {
		Row r{i->index, StrMakeValid(GetString(STR_INDUSTRY_NAME, i->index), {}), 0, 0, i->prod_level == PRODLEVEL_CLOSURE};
		uint64_t weighted = 0;
		for (const auto &p : i->produced) {
			if (!IsValidCargoType(p.cargo)) continue;
			uint amount = p.history[LAST_MONTH].production;
			r.prod += amount;
			weighted += (uint64_t)amount * ToPercent8(p.history[LAST_MONTH].PctTransported());
		}
		if (r.prod > 0) r.pct = (uint)(weighted / r.prod);
		rows.push_back(std::move(r));
	}
	if (rows.empty()) {
		ImWndText("산업 없음", COL_CH_DIM);
		return;
	}

	switch (mw.tab) {
		case 1: std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.prod > b.prod; }); break;
		case 2: std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.pct < b.pct; }); break;
		default: std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.name < b.name; }); break;
	}

	for (const Row &r : rows) {
		std::string value = mw.tab == 2 ? fmt::format("{}%", r.pct) : fmt::format("{}", r.prod);
		uint32_t tint = mw.tab == 2 ? (r.pct < 25 ? COL_CH_RED : r.pct < 50 ? COL_CH_YELLOW : COL_CH_TEXT) : COL_CH_TEXT;
		if (ImWndKVLink(r.name, value, r.closing ? COL_CH_RED : COL_CH_TEXT, tint)) {
			OpenMiniWnd(MiniWndKind::Industry, VehicleID::Invalid(), StationID::Invalid(), TownID::Invalid(), r.id);
		}
	}
}

/* News rows jump to whatever the item references, so a click lands on the
 * vehicle or place the message is about rather than reopening the paper. */
static void NewsRefFollow(const NewsReference &ref)
{
	struct visitor {
		void operator()(const std::monostate &) {}
		void operator()(const EngineID) {}
		void operator()(const TileIndex t) { MiniUiScrollTo(TileX(t) * TILE_SIZE, TileY(t) * TILE_SIZE); }
		void operator()(const VehicleID v)
		{
			const Vehicle *veh = Vehicle::GetIfValid(v);
			if (veh != nullptr) OpenMiniWnd(MiniWndKind::Vehicle, veh->First()->index, StationID::Invalid());
		}
		void operator()(const StationID s)
		{
			if (Station::IsValidID(s)) OpenMiniWnd(MiniWndKind::Station, VehicleID::Invalid(), s);
		}
		void operator()(const IndustryID i)
		{
			if (Industry::IsValidID(i)) OpenMiniWnd(MiniWndKind::Industry, VehicleID::Invalid(), StationID::Invalid(), TownID::Invalid(), i);
		}
		void operator()(const TownID t)
		{
			if (Town::IsValidID(t)) OpenMiniWnd(MiniWndKind::Town, VehicleID::Invalid(), StationID::Invalid(), t);
		}
	};
	std::visit(visitor{}, ref);
}

static void ImNewsListBody(MiniWnd &mw)
{
	bool any = false;
	for (const NewsItem &ni : GetNews()) {
		if (mw.tab == 1 && ni.type != NewsType::Advice) continue;
		any = true;
		std::string date = StrMakeValid(GetString(STR_JUST_DATE_TINY, ni.date), {});
		std::string text = StrMakeValid(ni.GetStatusText(), {});
		uint32_t tint = ni.type == NewsType::Advice ? COL_CH_YELLOW : COL_CH_TEXT;
		if (ImWndLink(fmt::format("{}  {}", date, text), tint)) NewsRefFollow(ni.ref1);
	}
	if (!any) ImWndText("소식 없음", COL_CH_DIM);
}

static void SubsidySourceOpen(const Source &src)
{
	switch (src.type) {
		case SourceType::Industry:
			if (Industry::IsValidID(src.ToIndustryID())) {
				OpenMiniWnd(MiniWndKind::Industry, VehicleID::Invalid(), StationID::Invalid(), TownID::Invalid(), src.ToIndustryID());
			}
			break;
		case SourceType::Town:
			if (Town::IsValidID(src.ToTownID())) {
				OpenMiniWnd(MiniWndKind::Town, VehicleID::Invalid(), StationID::Invalid(), src.ToTownID());
			}
			break;
		default:
			break;
	}
}

static void ImSubsidyListBody(MiniWnd &mw)
{
	bool awarded_tab = mw.tab == 1;
	bool any = false;
	for (const Subsidy *s : Subsidy::Iterate()) {
		if (s->IsAwarded() != awarded_tab) continue;
		any = true;
		std::string route = fmt::format("{}  {} → {}",
				StrMakeValid(GetString(CargoSpec::Get(s->cargo_type)->name), {}),
				StrMakeValid(GetString(s->src.GetFormat(), s->src.id), {}),
				StrMakeValid(GetString(s->dst.GetFormat(), s->dst.id), {}));
		uint32_t ltint = COL_CH_TEXT;
		if (awarded_tab) {
			route = fmt::format("{}  {}", StrMakeValid(GetString(STR_COMPANY_NAME, s->awarded), {}), route);
			if (s->awarded == _local_company) ltint = COL_CH_ACCENT;
		}
		/* The month in progress is not counted down yet, so it still counts as time left. */
		uint left = s->remaining + 1;
		std::string value = TimerGameEconomy::UsingWallclockUnits() ? fmt::format("{}분", left) : fmt::format("{}개월", left);
		if (ImWndKVLink(route, value, ltint, left <= 3 ? COL_CH_YELLOW : COL_CH_TEXT)) SubsidySourceOpen(s->src);
	}
	if (!any) ImWndText(awarded_tab ? "수주한 보조금 없음" : "제안된 보조금 없음", COL_CH_DIM);
}

/* Story pages have no mini window, so goals pointing at one stay inert. */
static void GoalTargetOpen(const Goal *g)
{
	switch (g->type) {
		case GT_TILE:
			if (IsValidTile(TileIndex{g->dst})) MiniUiScrollTo(TileX(TileIndex{g->dst}) * TILE_SIZE, TileY(TileIndex{g->dst}) * TILE_SIZE);
			break;
		case GT_TOWN:
			if (Town::IsValidID(g->dst)) {
				OpenMiniWnd(MiniWndKind::Town, VehicleID::Invalid(), StationID::Invalid(), TownID(static_cast<uint16_t>(g->dst)));
			}
			break;
		case GT_INDUSTRY:
			if (Industry::IsValidID(g->dst)) {
				OpenMiniWnd(MiniWndKind::Industry, VehicleID::Invalid(), StationID::Invalid(), TownID::Invalid(), IndustryID(static_cast<uint16_t>(g->dst)));
			}
			break;
		case GT_COMPANY:
			if (CompanyID(static_cast<uint8_t>(g->dst)) == _local_company) {
				OpenMiniWnd(MiniWndKind::Company, VehicleID::Invalid(), StationID::Invalid());
			}
			break;
		default:
			break;
	}
}

static void ImGoalListBody(MiniWnd &mw)
{
	CompanyID owner = mw.tab == 1 ? CompanyID::Invalid() : _local_company;
	bool any = false;
	for (const Goal *g : Goal::Iterate()) {
		if (g->company != owner) continue;
		any = true;
		std::string text = StrMakeValid(g->text.GetDecodedString(), {});
		std::string progress = g->progress.empty() ? std::string() : StrMakeValid(g->progress.GetDecodedString(), {});
		if (ImWndKVLink(text, progress, g->completed ? COL_CH_DIM : COL_CH_TEXT, g->completed ? COL_CH_ACCENT : COL_CH_TEXT)) {
			GoalTargetOpen(g);
		}
	}
	if (!any) ImWndText(WndOfficial(STR_GOALS_NONE), COL_CH_DIM);
}

static const StringID _mini_perf_titles[] = {
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_ENGINEER,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_ENGINEER,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_TRAFFIC_MANAGER,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_TRAFFIC_MANAGER,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_TRANSPORT_COORDINATOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_TRANSPORT_COORDINATOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_ROUTE_SUPERVISOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_ROUTE_SUPERVISOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_DIRECTOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_DIRECTOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_CHIEF_EXECUTIVE,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_CHIEF_EXECUTIVE,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_CHAIRMAN,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_CHAIRMAN,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_PRESIDENT,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_TYCOON,
};

static void ImLeagueBody()
{
	std::vector<const Company *> cs;
	for (const Company *c : Company::Iterate()) cs.push_back(c);
	if (cs.empty()) {
		ImWndText("회사 없음", COL_CH_DIM);
		return;
	}
	std::sort(cs.begin(), cs.end(), [](const Company *a, const Company *b) {
		return a->old_economy[0].performance_history > b->old_economy[0].performance_history;
	});

	for (size_t i = 0; i < cs.size(); i++) {
		const Company *c = cs[i];
		int perf = c->old_economy[0].performance_history;
		std::string title = WndOfficial(_mini_perf_titles[std::min<uint>(perf, 1000) >> 6]);
		std::string label = fmt::format("{}. {} · {}", i + 1, StrMakeValid(GetString(STR_COMPANY_NAME, c->index), {}), title);
		bool local = c->index == _local_company;
		if (ImWndKVLink(label, fmt::format("{}", perf), local ? COL_CH_ACCENT : COL_CH_TEXT, COL_CH_TEXT) && local) {
			OpenMiniWnd(MiniWndKind::Company, VehicleID::Invalid(), StationID::Invalid());
		}
	}
}

static void ImPreviewBody(const MiniWnd &mw)
{
	const Engine *e = Engine::GetIfValid(mw.eng);
	if (e == nullptr) return;

	ImWndText(StrMakeValid(GetString(STR_ENGINE_PREVIEW_MESSAGE, GetEngineCategoryName(mw.eng)), {}), COL_CH_TEXT);
	ImWndHeader(StrMakeValid(GetString(STR_ENGINE_NAME, PackEngineNameDParam(mw.eng, EngineNameContext::PreviewNews)), {}));
	ImWndText(StrMakeValid(GetEngineInfoString(mw.eng), {}), COL_CH_TEXT);
}

static void ImWaypointBody(const MiniWnd &mw)
{
	const Waypoint *wp = Waypoint::GetIfValid(mw.st);
	if (wp == nullptr) return;

	std::string_view kind = "부표";
	if (wp->facilities.Test(StationFacility::Train)) {
		kind = "철도 대기점";
	} else if (wp->facilities.Test(StationFacility::TruckStop) || wp->facilities.Test(StationFacility::BusStop)) {
		kind = "도로 대기점";
	}
	ImWndKV("종류", kind, COL_CH_TEXT);
	if (Company::IsValidID(wp->owner)) {
		ImWndKV("소유", StrMakeValid(GetString(STR_COMPANY_NAME, wp->owner), {}), COL_CH_TEXT);
	}
	if (wp->town != nullptr) {
		ImWndKV("도시", StrMakeValid(GetString(STR_TOWN_NAME, wp->town->index), {}), COL_CH_TEXT);
	}
	ImWndKV("좌표", fmt::format("{} · {}", TileX(wp->xy), TileY(wp->xy)), COL_CH_TEXT);
}

static Money TakeoverPrice(const MiniWnd &mw)
{
	const Company *c = Company::GetIfValid(mw.comp);
	if (c == nullptr) return 0;
	return mw.hostile ? CalculateHostileTakeoverValue(c) : c->bankrupt_value;
}

static void ImTakeoverBody(const MiniWnd &mw)
{
	const Company *c = Company::GetIfValid(mw.comp);
	if (c == nullptr) return;

	StringID str = mw.hostile ? STR_BUY_COMPANY_HOSTILE_TAKEOVER : STR_BUY_COMPANY_MESSAGE;
	ImWndText(StrMakeValid(GetString(str, c->index, TakeoverPrice(mw)), {}), COL_CH_TEXT);
	ImWndKV("성능 지수", fmt::format("{}/1000", c->old_economy[0].performance_history), COL_CH_TEXT);
	ImWndKV("보유 현금", StrMakeValid(GetString(STR_JUST_CURRENCY_LONG, c->money), {}), COL_CH_TEXT);
	ImWndKV("대출", StrMakeValid(GetString(STR_JUST_CURRENCY_LONG, c->current_loan), {}), c->current_loan > 0 ? COL_CH_YELLOW : COL_CH_TEXT);
}

enum class MiniMapMode : uint8_t {
	Contour,
	Vehicles,
	Industries,
	Routes,
	Owner,
};

static uint32_t MiniMapFaded(uint32_t c)
{
	return Mix(c, COL_CH_PANEL, 168);
}

static uint32_t MiniMapStationColour(TileIndex tile)
{
	switch (GetStationType(tile)) {
		case StationType::Rail:
		case StationType::RailWaypoint: return COL_ST_RAIL;
		case StationType::Airport: return COL_ST_AIR;
		case StationType::Truck:
		case StationType::Bus:
		case StationType::RoadWaypoint: return COL_ST_ROAD;
		case StationType::Dock: return COL_ST_DOCK;
		case StationType::Buoy: return COL_ST_BUOY;
		default: return COL_OBJ;
	}
}

static uint32_t MiniMapBaseColour(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_VOID: return COL_VOID;
		case MP_WATER: return COL_WATER;
		case MP_TREES: return COL_TREE;
		case MP_HOUSE: return COL_HOUSE;
		case MP_INDUSTRY: return COL_IND;
		case MP_RAILWAY: return COL_RAIL;
		case MP_ROAD: return COL_ROAD;
		case MP_STATION: return MiniMapStationColour(tile);
		case MP_TUNNELBRIDGE: return COL_BRIDGE;
		case MP_OBJECT: return COL_OBJ;
		default: return GroundColour(tile, TileHeight(tile));
	}
}

static uint32_t MiniMapOwnerColour(Owner o)
{
	if (Company::IsValidID(o)) return _company_rgb[_company_colours[o]];
	if (o == OWNER_TOWN) return COL_ROAD;
	return COL_OBJ;
}

static uint32_t MiniMapTileColour(TileIndex tile, MiniMapMode mode)
{
	TileType tt = GetTileType(tile);
	if (tt == MP_VOID) return COL_VOID;

	switch (mode) {
		case MiniMapMode::Industries:
			if (tt == MP_INDUSTRY) {
				const Industry *ind = Industry::GetByTile(tile);
				return PaletteRgb(GetIndustrySpec(ind->type)->map_colour);
			}
			return MiniMapFaded(MiniMapBaseColour(tile));

		case MiniMapMode::Routes:
			switch (tt) {
				case MP_RAILWAY: return COL_PAPER;
				case MP_ROAD: return COL_CATENARY;
				case MP_STATION: return MiniMapStationColour(tile);
				case MP_TUNNELBRIDGE: return COL_BRIDGE;
				default: return MiniMapFaded(MiniMapBaseColour(tile));
			}

		case MiniMapMode::Owner:
			switch (tt) {
				case MP_HOUSE: return COL_HOUSE;
				case MP_INDUSTRY: return COL_IND;
				case MP_CLEAR:
				case MP_TREES: return MiniMapFaded(MiniMapBaseColour(tile));
				case MP_WATER: {
					Owner o = GetTileOwner(tile);
					return Company::IsValidID(o) ? _company_rgb[_company_colours[o]] : COL_WATER;
				}
				default: return MiniMapOwnerColour(GetTileOwner(tile));
			}

		case MiniMapMode::Vehicles:
			return MiniMapFaded(MiniMapBaseColour(tile));

		default:
			return MiniMapBaseColour(tile);
	}
}

/* Scanning every tile costs too much to repeat per frame, so the overview
 * keeps a pixel buffer and refreshes it on a slow beat. */
struct MiniMapCache {
	int w = 0;
	int h = 0;
	uint8_t mode = 0xFF;
	float age = 0.0f;
	std::vector<uint32_t> px;
};

static MiniMapCache _map_cache;

static void RebuildMiniMap(int w, int h, MiniMapMode mode)
{
	int mx = (int)Map::SizeX();
	int my = (int)Map::SizeY();
	_map_cache.px.assign((size_t)w * h, COL_VOID);
	for (int y = 0; y < h; y++) {
		int tx = (int)((int64_t)y * mx / h);
		uint32_t *row = _map_cache.px.data() + (size_t)y * w;
		for (int x = 0; x < w; x++) {
			int ty = (int)((int64_t)x * my / w);
			row[x] = MiniMapTileColour(TileXY(tx, ty), mode);
		}
	}
	_map_cache.w = w;
	_map_cache.h = h;
	_map_cache.mode = (uint8_t)mode;
	_map_cache.age = 0.0f;
}

/* Town names are placed largest first and a name that would land on one
 * already placed is dropped, so a crowded map stays readable. */
static void DrawMiniMapTownNames(ImDrawList *dl, ImVec2 p, int w, int h)
{
	std::vector<const Town *> towns;
	for (const Town *t : Town::Iterate()) towns.push_back(t);
	std::sort(towns.begin(), towns.end(), [](const Town *a, const Town *b) { return a->cache.population > b->cache.population; });

	int mx = (int)Map::SizeX();
	int my = (int)Map::SizeY();
	std::vector<ImVec4> placed;
	for (const Town *t : towns) {
		std::string name = StrMakeValid(GetString(STR_TOWN_NAME, t->index), {});
		ImVec2 sz = ImGui::CalcTextSize(name.c_str());
		float cx = p.x + (float)TileY(t->xy) * w / my;
		float cy = p.y + (float)TileX(t->xy) * h / mx;
		ImVec4 r(cx - sz.x * 0.5f, cy - sz.y - 2.0f, cx + sz.x * 0.5f, cy - 2.0f);
		if (r.x < p.x || r.z > p.x + w || r.y < p.y) continue;
		bool hit = false;
		for (const ImVec4 &o : placed) {
			if (r.x < o.z && o.x < r.z && r.y < o.w && o.y < r.w) { hit = true; break; }
		}
		if (hit) continue;
		placed.push_back(r);
		dl->AddRectFilled(ImVec2(cx - 1.0f, cy - 1.0f), ImVec2(cx + 2.0f, cy + 2.0f), MiniImU32(COL_PAPER));
		dl->AddText(ImVec2(r.x + 1.0f, r.y + 1.0f), MiniImU32(COL_INK), name.c_str());
		dl->AddText(ImVec2(r.x, r.y), MiniImU32(COL_PAPER), name.c_str());
	}
}

static void ImMapBody(MiniWnd &mw)
{
	MiniMapMode mode = (MiniMapMode)mw.tab;
	int mx = (int)Map::SizeX();
	int my = (int)Map::SizeY();

	ImVec2 avail = ImGui::GetContentRegionAvail();
	float scale = std::min(avail.x / (float)my, avail.y / (float)mx);
	int w = std::max(1, (int)(my * scale));
	int h = std::max(1, (int)(mx * scale));

	ImVec2 p = ImGui::GetCursorScreenPos();
	p.x += std::floor((avail.x - w) * 0.5f);
	ImGui::SetCursorScreenPos(p);
	ImGui::InvisibleButton("map", ImVec2((float)w, (float)h));

	_map_cache.age += ImGui::GetIO().DeltaTime;
	if (_map_cache.w != w || _map_cache.h != h || _map_cache.mode != (uint8_t)mode || _map_cache.age > 0.5f) {
		RebuildMiniMap(w, h, mode);
	}

	ImDrawList *dl = ImGui::GetWindowDrawList();
	for (int y = 0; y < h; y++) {
		const uint32_t *row = _map_cache.px.data() + (size_t)y * w;
		int x = 0;
		while (x < w) {
			int e = x + 1;
			while (e < w && row[e] == row[x]) e++;
			dl->AddRectFilled(ImVec2(p.x + x, p.y + y), ImVec2(p.x + e, p.y + y + 1), MiniImU32(row[x]));
			x = e;
		}
	}

	if (mode == MiniMapMode::Vehicles) {
		for (const Vehicle *v : Vehicle::Iterate()) {
			if (!v->IsPrimaryVehicle() || v->vehstatus.Test(VehState::Hidden)) continue;
			float sx = p.x + (float)(v->y_pos / TILE_SIZE) * w / my;
			float sy = p.y + (float)(v->x_pos / TILE_SIZE) * h / mx;
			uint32_t c = Company::IsValidID(v->owner) ? _company_rgb[_company_colours[v->owner]] : COL_PAPER;
			dl->AddRectFilled(ImVec2(sx - 1.0f, sy - 1.0f), ImVec2(sx + 2.0f, sy + 2.0f), MiniImU32(c));
		}
	}

	if (mode == MiniMapMode::Contour) DrawMiniMapTownNames(dl, p, w, h);

	double half_y = _fbw * 0.5 / _cam_ppt;
	double half_x = _fbh * 0.5 / _cam_ppt;
	dl->AddRect(ImVec2(p.x + (float)((_cam_y - half_y) * w / my), p.y + (float)((_cam_x - half_x) * h / mx)),
			ImVec2(p.x + (float)((_cam_y + half_y) * w / my), p.y + (float)((_cam_x + half_x) * h / mx)),
			MiniImU32(COL_CH_ACCENT), 0.0f, 0, 1.5f);

	if (ImGui::IsItemActive()) {
		ImVec2 m = ImGui::GetIO().MousePos;
		int ty = Clamp((int)((m.x - p.x) * my / w), 0, my - 1);
		int tx = Clamp((int)((m.y - p.y) * mx / h), 0, mx - 1);
		MiniUiScrollTo(tx * TILE_SIZE, ty * TILE_SIZE);
	}
}

static int64_t GraphValue(const CompanyEconomyEntry &e, uint8_t tab)
{
	switch (tab) {
		case 1: return (int64_t)e.company_value;
		case 2: return e.performance_history;
		case 3: return (int64_t)e.delivered_cargo.GetSum<OverflowSafeInt64>();
		default: return (int64_t)(e.income + e.expenses);
	}
}

static std::string GraphValueStr(int64_t v, uint8_t tab)
{
	if (tab == 2 || tab == 3) return fmt::format("{}", v);
	return StrMakeValid(GetString(STR_JUST_CURRENCY_SHORT, v), {});
}

/* Quarters run oldest to newest from left to right; series shorter than the
 * widest one start further right so the newest quarter always lines up. */
static void ImGraphBody(MiniWnd &mw)
{
	struct Series {
		uint32_t colour;
		std::string name;
		std::vector<int64_t> vals;
		bool local;
	};

	std::vector<Series> series;
	size_t span = 0;
	for (const Company *c : Company::Iterate()) {
		Series s;
		s.colour = _company_rgb[_company_colours[c->index]];
		s.name = StrMakeValid(GetString(STR_COMPANY_NAME, c->index), {});
		s.local = c->index == _local_company;
		int cnt = std::min<int>(c->num_valid_stat_ent, MAX_HISTORY_QUARTERS);
		for (int j = cnt - 1; j >= 0; j--) s.vals.push_back(GraphValue(c->old_economy[j], mw.tab));
		if (s.vals.empty()) continue;
		span = std::max(span, s.vals.size());
		series.push_back(std::move(s));
	}

	if (span < 2) {
		ImWndText("기록이 쌓이면 그래프가 나타납니다", COL_CH_DIM);
		return;
	}

	int64_t lo = 0;
	int64_t hi = 0;
	for (const Series &s : series) {
		for (int64_t v : s.vals) {
			lo = std::min(lo, v);
			hi = std::max(hi, v);
		}
	}
	if (lo == hi) hi = lo + 1;

	float lh = ImGui::GetFontSize();
	float legend_h = (float)series.size() * ImGui::GetTextLineHeightWithSpacing();
	float w = ImGui::GetContentRegionAvail().x;
	float h = std::max(ImGui::GetContentRegionAvail().y - legend_h - lh * 2.0f, lh * 5.0f);
	ImVec2 p = ImGui::GetCursorScreenPos();
	ImGui::Dummy(ImVec2(w, h));

	ImDrawList *dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), MiniImU32(COL_CH_TILE));

	auto plot_y = [&](int64_t v) { return p.y + h - (float)((double)(v - lo) / (double)(hi - lo) * h); };
	float zero_y = plot_y(0);
	if (lo < 0) dl->AddLine(ImVec2(p.x, zero_y), ImVec2(p.x + w, zero_y), MiniImU32(COL_CH_DIM));

	float step = w / (float)(span - 1);
	for (const Series &s : series) {
		size_t off = span - s.vals.size();
		for (size_t i = 1; i < s.vals.size(); i++) {
			ImVec2 a(p.x + (float)(off + i - 1) * step, plot_y(s.vals[i - 1]));
			ImVec2 b(p.x + (float)(off + i) * step, plot_y(s.vals[i]));
			dl->AddLine(a, b, MiniImU32(s.colour), s.local ? 2.5f : 1.5f);
		}
	}

	dl->AddText(ImVec2(p.x + 3.0f, p.y + 2.0f), MiniImU32(COL_CH_DIM), GraphValueStr(hi, mw.tab).c_str());
	dl->AddText(ImVec2(p.x + 3.0f, p.y + h - lh - 2.0f), MiniImU32(COL_CH_DIM), GraphValueStr(lo, mw.tab).c_str());

	for (const Series &s : series) {
		ImWndKV(fmt::format("{}{}", s.local ? "▶ " : "· ", s.name), GraphValueStr(s.vals.back(), mw.tab), s.colour);
	}
}

static bool ImWndButton(std::string_view label, bool enabled)
{
	ImGui::BeginDisabled(!enabled);
	bool clicked = ImGui::Button(std::string(label).c_str());
	ImGui::EndDisabled();
	ImGui::SameLine();
	return clicked;
}

static void ImWndCommands(MiniWnd &mw, const Vehicle *v, const Station *st, const Town *t, const Industry *ind)
{
	ImGui::Separator();
	switch (mw.kind) {
		case MiniWndKind::Vehicle: {
			bool own = v != nullptr && v->owner == _local_company;
			bool stopped = v != nullptr && v->vehstatus.Test(VehState::Stopped);
			if (ImWndButton(stopped ? "출발" : "정지", own)) {
				Command<CMD_START_STOP_VEHICLE>::Post(STR_ERROR_CAN_T_STOP_START_TRAIN + v->type, v->tile, v->index, false);
			}
			if (ImWndButton("차고로", own)) {
				Command<CMD_SEND_VEHICLE_TO_DEPOT>::Post(GetCmdSendToDepotMsg(v), v->index, _ctrl_pressed ? DepotCommandFlag::Service : DepotCommandFlags{}, {});
			}
			if (ImWndButton("개조", own)) mw.want_tab = 1;
			if (ImWndButton("주문", own)) mw.want_tab = 2;
			bool following = v != nullptr && _follow_veh == v->index;
			if (ImWndButton(following ? "추적 해제" : "따라가기", v != nullptr)) {
				if (following) EnterIdleMode(); else EnterFollowMode(v->index);
			}
			break;
		}

		case MiniWndKind::Station: {
			bool own = st != nullptr && (st->owner == _local_company || st->owner == OWNER_NONE);
			extern const Station *_viewport_highlight_station;
			bool hl = st != nullptr && _viewport_highlight_station == st;
			if (ImWndButton(hl ? "범위 끄기" : "범위", own)) {
				SetViewportCatchmentStation(st, !hl);
			}
			if (ImWndButton("이동", st != nullptr)) {
				MiniUiScrollTo(TileX(st->xy) * TILE_SIZE, TileY(st->xy) * TILE_SIZE);
			}
			break;
		}

		case MiniWndKind::Town: {
			bool act = t != nullptr && mw.sel_ord >= 0 && mw.sel_ord < (int16_t)to_underlying(TownAction::End) &&
					GetMaskOfTownActions(_local_company, t).Test((TownAction)mw.sel_ord);
			if (ImWndButton("실행", act)) {
				Command<CMD_DO_TOWN_ACTION>::Post(STR_ERROR_CAN_T_DO_THIS, t->xy, t->index, (TownAction)mw.sel_ord);
				mw.sel_ord = -1;
			}
			if (ImWndButton("이동", t != nullptr)) MiniUiScrollTo(TileX(t->xy) * TILE_SIZE, TileY(t->xy) * TILE_SIZE);
			break;
		}

		case MiniWndKind::Industry: {
			extern void ShowIndustryCargoesWindow(IndustryType id);
			if (ImWndButton("계통", ind != nullptr)) ShowIndustryCargoesWindow(ind->type);
			if (ImWndButton("이동", ind != nullptr)) {
				TileIndex ct = ind->location.GetCenterTile();
				MiniUiScrollTo(TileX(ct) * TILE_SIZE, TileY(ct) * TILE_SIZE);
			}
			break;
		}

		case MiniWndKind::Fleet: {
			bool own = Company::IsValidID(_local_company);
			const Vehicle *sv = Vehicle::GetIfValid(mw.sel);
			if (ImWndButton("매각", own && sv != nullptr)) {
				bool chain = sv->type == VEH_TRAIN && sv->First() == sv;
				Command<CMD_SELL_VEHICLE>::Post(GetCmdSellVehMsg(sv->type), sv->tile, sv->index, chain, true, INVALID_CLIENT_ID);
				mw.sel = VehicleID::Invalid();
			}
			if (ImWndButton("설계 비우기", own && !_fleet_draft[mw.tab].empty())) {
				_fleet_draft[mw.tab].clear();
			}
			if (ImWndButton("복제", own && sv != nullptr)) {
				Command<CMD_CLONE_VEHICLE>::Post(GetCmdBuildVehMsg(sv->type), sv->tile, sv->First()->index, false);
			}
			break;
		}

		case MiniWndKind::Finance: {
			const Company *fc = Company::GetIfValid(_local_company);
			if (ImWndButton("빌리기", fc != nullptr && fc->current_loan < fc->GetMaxLoan())) {
				Command<CMD_INCREASE_LOAN>::Post(STR_ERROR_CAN_T_BORROW_ANY_MORE_MONEY, LoanCommand::Interval, 0);
			}
			if (ImWndButton("갚기", fc != nullptr && fc->current_loan > 0)) {
				Command<CMD_DECREASE_LOAN>::Post(STR_ERROR_CAN_T_REPAY_LOAN, LoanCommand::Interval, 0);
			}
			break;
		}

		case MiniWndKind::Takeover: {
			const Company *tc = Company::GetIfValid(mw.comp);
			const Company *own = Company::GetIfValid(_local_company);
			bool affordable = tc != nullptr && own != nullptr && own->money >= TakeoverPrice(mw);
			if (ImWndButton("매수", affordable)) {
				Command<CMD_BUY_COMPANY>::Post(STR_ERROR_CAN_T_BUY_COMPANY, mw.comp, mw.hostile);
				mw.want_close = true;
			}
			if (ImWndButton("거절", true)) mw.want_close = true;
			break;
		}

		case MiniWndKind::Waypoint: {
			const Waypoint *wp = Waypoint::GetIfValid(mw.st);
			if (ImWndButton("지도에서 보기", wp != nullptr)) {
				MiniUiScrollTo(TileX(wp->xy) * TILE_SIZE, TileY(wp->xy) * TILE_SIZE);
			}
			break;
		}

		case MiniWndKind::Preview:
			if (ImWndButton("수락", Engine::GetIfValid(mw.eng) != nullptr)) {
				Command<CMD_WANT_ENGINE_PREVIEW>::Post(mw.eng);
				mw.want_close = true;
			}
			if (ImWndButton("거절", true)) mw.want_close = true;
			break;

		case MiniWndKind::Company: {
			const Company *cc = Company::GetIfValid(_local_company);
			bool has_hq = cc != nullptr && cc->location_of_HQ != INVALID_TILE;
			if (ImWndButton("본사 보기", has_hq)) {
				MiniUiScrollTo(TileX(cc->location_of_HQ) * TILE_SIZE, TileY(cc->location_of_HQ) * TILE_SIZE);
			}
			break;
		}

		case MiniWndKind::Group: {
			bool own = Company::IsValidID(_local_company);
			VehicleListIdentifier vli(VL_GROUP_LIST, (VehicleType)mw.tab, _local_company, mw.sel_grp);
			if (ImWndButton("새 그룹", own)) {
				Command<CMD_CREATE_GROUP>::Post(STR_ERROR_GROUP_CAN_T_CREATE, (VehicleType)mw.tab, GroupID::Invalid());
			}
			if (ImWndButton("그룹 삭제", own && Group::IsValidID(mw.sel_grp))) {
				Command<CMD_DELETE_GROUP>::Post(STR_ERROR_GROUP_CAN_T_DELETE, mw.sel_grp);
				mw.sel_grp = ALL_GROUP;
			}
			if (ImWndButton("전체 출발", own)) {
				Command<CMD_MASS_START_STOP>::Post(TileIndex{}, true, true, vli);
			}
			if (ImWndButton("전체 정지", own)) {
				Command<CMD_MASS_START_STOP>::Post(TileIndex{}, false, true, vli);
			}
			if (ImWndButton("전체 차고로", own)) {
				Command<CMD_SEND_VEHICLE_TO_DEPOT>::Post(GetCmdSendToDepotMsg((VehicleType)mw.tab), VehicleID::Invalid(), DepotCommandFlag::MassSend, vli);
			}
			break;
		}

		case MiniWndKind::StationList:
		case MiniWndKind::TownList:
		case MiniWndKind::IndustryList:
		case MiniWndKind::NewsList:
		case MiniWndKind::SubsidyList:
		case MiniWndKind::GoalList:
		case MiniWndKind::League:
		case MiniWndKind::Graph:
		case MiniWndKind::Map:
			break;
	}
	ImGui::NewLine();
}

static bool WndRenamable(const MiniWnd &mw, const Vehicle *v, const Station *st, const Town *t)
{
	switch (mw.kind) {
		case MiniWndKind::Vehicle: return v != nullptr && v->owner == _local_company;
		case MiniWndKind::Station: return st != nullptr && st->owner == _local_company;
		case MiniWndKind::Town: return t != nullptr;
		case MiniWndKind::Company: return Company::IsValidID(_local_company);
		case MiniWndKind::Waypoint: {
			const Waypoint *wp = Waypoint::GetIfValid(mw.st);
			return wp != nullptr && wp->owner == _local_company;
		}
		default: return false;
	}
}

static void WndPostRename(const MiniWnd &mw, const Vehicle *v, const Station *st, const Town *t, std::string name)
{
	if (name.empty()) return;
	switch (mw.kind) {
		case MiniWndKind::Vehicle:
			if (v != nullptr) Command<CMD_RENAME_VEHICLE>::Post(STR_ERROR_CAN_T_RENAME_TRAIN + v->type, v->index, std::move(name));
			break;
		case MiniWndKind::Station:
			if (st != nullptr) Command<CMD_RENAME_STATION>::Post(STR_ERROR_CAN_T_RENAME_STATION, st->index, std::move(name));
			break;
		case MiniWndKind::Town:
			if (t != nullptr) Command<CMD_RENAME_TOWN>::Post(STR_ERROR_CAN_T_RENAME_TOWN, t->index, std::move(name));
			break;
		case MiniWndKind::Company:
			Command<CMD_RENAME_COMPANY>::Post(STR_ERROR_CAN_T_CHANGE_COMPANY_NAME, std::move(name));
			break;
		case MiniWndKind::Waypoint:
			Command<CMD_RENAME_WAYPOINT>::Post(STR_ERROR_CAN_T_CHANGE_WAYPOINT_NAME, mw.st, std::move(name));
			break;
		default: break;
	}
}

/* The caption doubles as the rename field: a double click swaps the label for
 * an input, Enter commits, anything else leaves the name alone. */
static void ImWndTitle(MiniWnd &mw, const std::string &title, bool renamable, bool &open,
		const Vehicle *v, const Station *st, const Town *t)
{
	if (mw.renaming && !renamable) mw.renaming = false;

	float bw = ImGui::GetFrameHeight();
	float right = ImGui::GetContentRegionMax().x - bw;

	/* The window carries no native title bar, so the caption row paints its
	 * own band to stay readable as a drag handle. */
	ImVec2 wp = ImGui::GetWindowPos();
	ImVec2 cp = ImGui::GetCursorScreenPos();
	float pad = ImGui::GetStyle().WindowPadding.y;
	ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(wp.x + 1.0f, cp.y - pad),
			ImVec2(wp.x + ImGui::GetWindowWidth() - 1.0f, cp.y + bw), MiniImU32(COL_CH_TILE),
			ImGui::GetStyle().WindowRounding, ImDrawFlags_RoundCornersTop);

	if (mw.renaming) {
		ImGui::PushID("title");
		int r = ImWndNameEdit(mw, std::max(right - ImGui::GetCursorPosX() - ImGui::GetStyle().ItemSpacing.x, 32.0f));
		ImGui::PopID();
		if (r == 1) WndPostRename(mw, v, st, t, mw.name_buf);
		if (r != 0) mw.renaming = false;
	} else {
		ImGui::AlignTextToFramePadding();
		ImWndText(title, COL_CH_ACCENT);
		if (renamable && ImGui::IsItemHovered()) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				ImWndNameEditBegin(mw, title);
				mw.renaming = true;
				mw.rename_grp = GroupID::Invalid();
			}
		}
	}

	ImGui::SameLine(right);
	ImGui::PushID("close");
	if (ImGui::Button("×", ImVec2(bw, bw))) open = false;
	ImGui::PopID();
	ImGui::Separator();
}

static bool DrawImGuiMiniWnd(MiniWnd &mw)
{
	int s = _ms.hud_scale;
	const Vehicle *v = mw.kind == MiniWndKind::Vehicle ? Vehicle::GetIfValid(mw.veh) : nullptr;
	const Station *st = mw.kind == MiniWndKind::Station ? (Station::IsValidID(mw.st) ? Station::Get(mw.st) : nullptr) : nullptr;
	const Town *t = mw.kind == MiniWndKind::Town ? Town::GetIfValid(mw.town) : nullptr;
	const Industry *ind = mw.kind == MiniWndKind::Industry ? Industry::GetIfValid(mw.ind) : nullptr;

	std::string title = "-";
	uint32_t idnum = 0;
	switch (mw.kind) {
		case MiniWndKind::Vehicle: if (v != nullptr) title = StrMakeValid(GetString(STR_VEHICLE_NAME, v->index), {}); idnum = mw.veh.base(); break;
		case MiniWndKind::Station: if (st != nullptr) title = StrMakeValid(GetString(STR_STATION_NAME, st->index), {}); idnum = mw.st.base(); break;
		case MiniWndKind::Town: if (t != nullptr) title = StrMakeValid(GetString(STR_TOWN_NAME, t->index), {}); idnum = mw.town.base(); break;
		case MiniWndKind::Fleet: title = "차고"; break;
		case MiniWndKind::Finance: title = "재정"; break;
		case MiniWndKind::Company: if (Company::IsValidID(_local_company)) title = StrMakeValid(GetString(STR_COMPANY_NAME, _local_company), {}); break;
		case MiniWndKind::Group: title = "차량군"; break;
		case MiniWndKind::StationList: title = "역 목록"; break;
		case MiniWndKind::TownList: title = "도시 목록"; break;
		case MiniWndKind::IndustryList: title = "산업 목록"; break;
		case MiniWndKind::NewsList: title = "소식"; break;
		case MiniWndKind::SubsidyList: title = "보조금"; break;
		case MiniWndKind::GoalList: title = "목표"; break;
		case MiniWndKind::League: title = "순위"; break;
		case MiniWndKind::Graph: title = "그래프"; break;
		case MiniWndKind::Map: title = "지도"; break;
		case MiniWndKind::Preview: title = "신형 차량"; idnum = mw.eng.base(); break;
		case MiniWndKind::Takeover:
			if (Company::IsValidID(mw.comp)) title = StrMakeValid(GetString(STR_COMPANY_NAME, mw.comp), {});
			idnum = mw.comp.base();
			break;
		case MiniWndKind::Waypoint:
			if (Waypoint::IsValidID(mw.st)) title = StrMakeValid(GetString(STR_WAYPOINT_NAME, mw.st), {});
			idnum = mw.st.base();
			break;
		default: if (ind != nullptr) title = StrMakeValid(GetString(STR_INDUSTRY_NAME, ind->index), {}); idnum = mw.ind.base(); break;
	}
	std::string wid = fmt::format("###mw{}_{}", (int)mw.kind, idnum);

	bool wide = WndWide(mw);
	ImVec2 def_size((float)((wide ? 560 : 250) * s), (float)(270 * s));
	ImGui::SetNextWindowPos(ImVec2((float)mw.x, (float)mw.y), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(def_size, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(def_size, ImVec2(FLT_MAX, FLT_MAX));
	if (mw.want_raise) {
		ImGui::SetNextWindowFocus();
		mw.want_raise = false;
	}
	bool open = true;
	if (!ImGui::Begin(wid.c_str(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse)) {
		ImGui::End();
		return open;
	}
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
		_front_wnd_veh = mw.kind == MiniWndKind::Vehicle ? mw.veh : VehicleID::Invalid();
	}

	ImWndTitle(mw, title, WndRenamable(mw, v, st, t), open, v, st, t);

	_imrow = 0;
	int ntab;
	std::string tl[6];
	switch (mw.kind) {
		case MiniWndKind::Fleet:
		case MiniWndKind::Group: {
			static const StringID type_strs[] = {STR_REPLACE_VEHICLE_TRAIN, STR_REPLACE_VEHICLE_ROAD_VEHICLE, STR_REPLACE_VEHICLE_SHIP, STR_REPLACE_VEHICLE_AIRCRAFT};
			ntab = 4;
			for (int ti = 0; ti < 4; ti++) tl[ti] = WndOfficial(type_strs[ti]);
			break;
		}
		case MiniWndKind::Finance:
			ntab = 2;
			tl[0] = "개요";
			tl[1] = "손익";
			break;
		case MiniWndKind::Company:
			ntab = 2;
			tl[0] = "개요";
			tl[1] = "자산";
			break;
		case MiniWndKind::Vehicle:
			ntab = 4;
			tl[0] = "상태";
			tl[1] = WndOfficial(STR_VEHICLE_DETAIL_TAB_CARGO);
			tl[2] = "주문";
			tl[3] = WndOfficial(STR_VEHICLE_DETAIL_TAB_INFORMATION);
			break;
		case MiniWndKind::Station:
			ntab = 4;
			tl[0] = "상태";
			tl[1] = WndOfficial(STR_SMALLMAP_TYPE_INDUSTRIES);
			tl[2] = WndOfficial(STR_SMALLMAP_TYPE_VEHICLES);
			tl[3] = WndOfficial(STR_VEHICLE_DETAIL_TAB_INFORMATION);
			break;
		case MiniWndKind::Town:
			ntab = 3;
			tl[0] = "상태";
			tl[1] = "당국";
			tl[2] = WndOfficial(STR_VEHICLE_DETAIL_TAB_INFORMATION);
			break;
		case MiniWndKind::StationList:
			ntab = 3;
			tl[0] = "이름";
			tl[1] = "화물";
			tl[2] = "평가";
			break;
		case MiniWndKind::TownList:
			ntab = 3;
			tl[0] = "이름";
			tl[1] = "인구";
			tl[2] = "평판";
			break;
		case MiniWndKind::IndustryList:
			ntab = 3;
			tl[0] = "이름";
			tl[1] = "생산";
			tl[2] = "수송";
			break;
		case MiniWndKind::NewsList:
			ntab = 2;
			tl[0] = "전체";
			tl[1] = "조언";
			break;
		case MiniWndKind::SubsidyList:
			ntab = 2;
			tl[0] = "제안";
			tl[1] = "수주";
			break;
		case MiniWndKind::GoalList:
			ntab = 2;
			tl[0] = "회사";
			tl[1] = "전체";
			break;
		case MiniWndKind::League:
			ntab = 1;
			tl[0] = "성능";
			break;
		case MiniWndKind::Preview:
			ntab = 1;
			tl[0] = "제안";
			break;
		case MiniWndKind::Takeover:
			ntab = 1;
			tl[0] = "인수";
			break;
		case MiniWndKind::Waypoint:
			ntab = 1;
			tl[0] = "상태";
			break;
		case MiniWndKind::Graph:
			ntab = 4;
			tl[0] = "수익";
			tl[1] = "가치";
			tl[2] = "성능";
			tl[3] = "화물";
			break;
		case MiniWndKind::Map:
			ntab = 5;
			tl[0] = WndOfficial(STR_SMALLMAP_TYPE_CONTOURS);
			tl[1] = WndOfficial(STR_SMALLMAP_TYPE_VEHICLES);
			tl[2] = WndOfficial(STR_SMALLMAP_TYPE_INDUSTRIES);
			tl[3] = WndOfficial(STR_SMALLMAP_TYPE_ROUTES);
			tl[4] = WndOfficial(STR_SMALLMAP_TYPE_OWNERS);
			break;
		default:
			ntab = 3;
			tl[0] = "상태";
			tl[1] = "역";
			tl[2] = WndOfficial(STR_VEHICLE_DETAIL_TAB_INFORMATION);
			break;
	}

	bool has_view = mw.kind == MiniWndKind::Vehicle || mw.kind == MiniWndKind::Station ||
			mw.kind == MiniWndKind::Town || mw.kind == MiniWndKind::Industry;
	if (ImGui::BeginTabBar("tabs")) {
		for (int ti = 0; ti < ntab; ti++) {
			ImGuiTabItemFlags fl = mw.want_tab == ti ? ImGuiTabItemFlags_SetSelected : 0;
			if (ImGui::BeginTabItem(fmt::format("{}###t{}", tl[ti], ti).c_str(), nullptr, fl)) {
				if (mw.tab != ti) {
					mw.tab = (uint8_t)ti;
					if (ti != 0) CloseMiniCarrier(mw);
				}
				bool has_cmds = !WndIsList(mw.kind);
				float cmd_h = has_cmds ? ImGui::GetFrameHeightWithSpacing() + 4.0f * s : 0.0f;
				ImGui::BeginChild("body", ImVec2(0.0f, -cmd_h));
				if (ti == 0 && has_view && (v != nullptr || st != nullptr || t != nullptr || ind != nullptr)) {
					ImWndViewSlot(mw);
				}
				switch (mw.kind) {
					case MiniWndKind::Vehicle: if (v != nullptr) ImVehicleBody(mw, v); break;
					case MiniWndKind::Station: if (st != nullptr) ImStationBody(mw, st); break;
					case MiniWndKind::Town: if (t != nullptr) ImTownBody(mw, t); break;
					case MiniWndKind::Industry: if (ind != nullptr) ImIndustryBody(mw, ind); break;
					case MiniWndKind::Fleet: ImFleetBody(mw); break;
					case MiniWndKind::Finance: ImFinanceBody(mw.tab); break;
					case MiniWndKind::Company: ImCompanyBody(mw); break;
					case MiniWndKind::Group: ImGroupBody(mw); break;
					case MiniWndKind::StationList: ImStationListBody(mw); break;
					case MiniWndKind::TownList: ImTownListBody(mw); break;
					case MiniWndKind::IndustryList: ImIndustryListBody(mw); break;
					case MiniWndKind::NewsList: ImNewsListBody(mw); break;
					case MiniWndKind::SubsidyList: ImSubsidyListBody(mw); break;
					case MiniWndKind::GoalList: ImGoalListBody(mw); break;
					case MiniWndKind::League: ImLeagueBody(); break;
					case MiniWndKind::Graph: ImGraphBody(mw); break;
					case MiniWndKind::Map: ImMapBody(mw); break;
					case MiniWndKind::Preview: ImPreviewBody(mw); break;
					case MiniWndKind::Takeover: ImTakeoverBody(mw); break;
					case MiniWndKind::Waypoint: ImWaypointBody(mw); break;
				}
				ImGui::EndChild();
				if (has_cmds) ImWndCommands(mw, v, st, t, ind);
				if (mw.want_close) open = false;
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}
	mw.want_tab = -1;

	ImGui::End();
	return open;
}

static void DrawMiniWndsImGui()
{
	for (size_t i = _wnds.size(); i-- > 0;) {
		bool alive;
		switch (_wnds[i].kind) {
			case MiniWndKind::Vehicle: alive = Vehicle::GetIfValid(_wnds[i].veh) != nullptr; break;
			case MiniWndKind::Station: alive = Station::IsValidID(_wnds[i].st); break;
			case MiniWndKind::Town: alive = Town::IsValidID(_wnds[i].town); break;
			case MiniWndKind::Industry: alive = Industry::IsValidID(_wnds[i].ind); break;
			case MiniWndKind::Finance: alive = Company::IsValidID(_local_company); break;
			case MiniWndKind::Company: alive = Company::IsValidID(_local_company); break;
			case MiniWndKind::Preview: {
				const Engine *pe = Engine::GetIfValid(_wnds[i].eng);
				alive = pe != nullptr && pe->preview_company == _local_company;
				break;
			}
			case MiniWndKind::Takeover: alive = Company::IsValidID(_wnds[i].comp) && Company::IsValidID(_local_company); break;
			case MiniWndKind::Waypoint: alive = Waypoint::IsValidID(_wnds[i].st); break;
			case MiniWndKind::Group: alive = Company::IsValidID(_local_company); break;
			case MiniWndKind::StationList: alive = Company::IsValidID(_local_company); break;
			default: alive = true; break;
		}
		if (!alive) CloseMiniWnd(i);
	}

	_wnds_drawing = true;
	std::vector<size_t> closed;
	for (size_t i = 0; i < _wnds.size(); i++) {
		if (!DrawImGuiMiniWnd(_wnds[i])) closed.push_back(i);
	}
	_wnds_drawing = false;

	for (size_t i = closed.size(); i-- > 0;) CloseMiniWnd(closed[i]);

	std::vector<MiniOpenReq> opens;
	opens.swap(_wnd_opens);
	for (const MiniOpenReq &r : opens) OpenMiniWnd(r.kind, r.veh, r.st, r.town, r.ind, r.eng, r.comp);
}

bool MiniUiShowError(std::string summary, std::string detail, bool warn)
{
	if (!_mini_active || summary.empty()) return false;

	uint life = std::max<uint>(_settings_client.gui.errmsg_duration, 1) * 1000;
	if (!_toasts.empty()) {
		MiniToast &last = _toasts.back();
		if (last.summary == summary && last.detail == detail) {
			last.repeat++;
			last.left_ms = life;
			last.full_ms = life;
			last.warn = last.warn || warn;
			return true;
		}
	}

	MiniToast t;
	t.summary = std::move(summary);
	t.detail = std::move(detail);
	t.left_ms = life;
	t.full_ms = life;
	t.warn = warn;
	_toasts.push_back(std::move(t));
	if (_toasts.size() > MINI_TOAST_MAX) _toasts.erase(_toasts.begin());
	return true;
}

/* The newspaper is replaced by a toast that keeps the item's reference, so a
 * click lands on what the message is about. The item itself stays in the news
 * history either way. */
bool MiniUiShowNews(const NewsItem *ni)
{
	if (!_mini_active || ni == nullptr) return false;

	std::string headline = StrMakeValid(ni->headline.GetDecodedString(), {});
	if (headline.empty()) return true;

	uint life = std::max<uint>(_settings_client.gui.errmsg_duration, 1) * 2000;
	MiniToast t;
	t.summary = std::move(headline);
	t.detail = StrMakeValid(GetString(STR_JUST_DATE_TINY, ni->date), {});
	t.left_ms = life;
	t.full_ms = life;
	t.warn = ni->type == NewsType::Advice;
	t.ref = ni->ref1;
	_toasts.push_back(std::move(t));
	if (_toasts.size() > MINI_TOAST_MAX) _toasts.erase(_toasts.begin());
	return true;
}

bool ShowMiniEnginePreview(EngineID engine)
{
	if (!_mini_active) return false;
	OpenMiniWnd(MiniWndKind::Preview, VehicleID::Invalid(), StationID::Invalid(), TownID::Invalid(), IndustryID::Invalid(), engine);
	return true;
}

bool ShowMiniBuyCompany(CompanyID company, bool hostile_takeover)
{
	if (!_mini_active) return false;
	OpenMiniWnd(MiniWndKind::Takeover, VehicleID::Invalid(), StationID::Invalid(), TownID::Invalid(), IndustryID::Invalid(), EngineID::Invalid(), company);
	for (MiniWnd &mw : _wnds) {
		if (mw.kind == MiniWndKind::Takeover && mw.comp == company) mw.hostile = hostile_takeover;
	}
	return true;
}

bool ShowMiniVehicleWindow(const Vehicle *v)
{
	if (!_mini_active) return false;
	OpenMiniWnd(MiniWndKind::Vehicle, v->First()->index, StationID::Invalid());
	return true;
}

bool ShowMiniWaypointWindow(StationID waypoint)
{
	if (!_mini_active || !Waypoint::IsValidID(waypoint)) return false;
	OpenMiniWnd(MiniWndKind::Waypoint, VehicleID::Invalid(), waypoint);
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
	DrawToasts();
	DrawBuildMenu();
	DrawCmdBar();
	DrawWinBar();
	DrawMiniWndsImGui();
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

/* A new or loaded world reuses pool IDs, so state naming entities from the old world must not survive the switch. */
void MiniUiResetGameState()
{
	CloseAllMiniWnds();
	EnterIdleMode();
	_dragging = false;
	ClearPlans();
	_veh_snap.clear();
	_stuck_long.clear();
	for (auto &l : _status_veh) l.clear();
	for (auto &d : _fleet_draft) d.clear();
	_deploy = FleetDeploy{};
	_toasts.clear();
	_toast_rows.clear();
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

void MiniUiOverlayRects(std::vector<RlwRectI> &rects)
{
	if (!_mini_active) return;
	for (const Window *w : Window::IterateFromBack()) {
		if (MiniUiHidesWindow(w->window_class)) continue;
		/* Carriers stay below the ImGui layer; their pixels surface through
		 * the view-slot image, so window z-order needs no rect clipping. */
		rects.push_back({w->left, w->top, w->width, w->height, CarrierOwner(w) != SIZE_MAX});
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

	if (!_dragging && !_middle_button_down) {
		if (native_capture) return false;
		/* Native windows float above the ImGui layer and take the click;
		 * carriers sit below it, so ImGui gets those instead. */
		Window *w = FindWindowFromPt(_cursor.pos.x, _cursor.pos.y);
		if (w != nullptr && !MiniUiHidesWindow(w->window_class) && CarrierOwner(w) == SIZE_MAX) return false;
		/* ImGui reads the wheel from the driver itself. Leaving the pending
		 * notch here would zoom the map the moment the cursor leaves the
		 * window and the map starts consuming events again. */
		if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse) {
			_cursor.wheel = 0;
			return true;
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
		if (_menu_open >= 0 && InRect(_menu_panel_rect, _cursor.pos.x, _cursor.pos.y)) {
			_menu_scroll += _cursor.wheel > 0 ? 1 : -1;
		} else {
			ZoomAt(_cursor.pos.x, _cursor.pos.y, _cursor.wheel < 0);
		}
		_cursor.wheel = 0;
	}

	if (_left_button_down && !_left_button_clicked) {
		_left_button_clicked = true;
		if (!HandleToastClick(_cursor.pos.x, _cursor.pos.y) && !HandleMenuClick(_cursor.pos.x, _cursor.pos.y) && !HandleCmdClick(_cursor.pos.x, _cursor.pos.y) && !HandleWinClick(_cursor.pos.x, _cursor.pos.y) && !HandleSpeedClick(_cursor.pos.x, _cursor.pos.y) && !HandleStatusClick(_cursor.pos.x, _cursor.pos.y)) {
			if (_tool == MiniTool::None) {
				if (_order_pick_veh != VehicleID::Invalid()) {
					OrderPickClick(_cursor.pos.x, _cursor.pos.y);
				} else if (!HandleLabelClick(_cursor.pos.x, _cursor.pos.y) && !OpenVehicleWndAt(_cursor.pos.x, _cursor.pos.y) &&
						!OpenDepotWndAt(_cursor.pos.x, _cursor.pos.y)) {
					OpenWaypointWndAt(_cursor.pos.x, _cursor.pos.y);
				}
			} else if (IsPointTool(_tool)) {
				_drag_remove = _ctrl_pressed;
				_drag_ax = MapXAt(_cursor.pos.y);
				_drag_ay = MapYAt(_cursor.pos.x);
				if (_tool == MiniTool::Signal) {
					_dragging = true;
					UpdateSignalPlan(_drag_ax, _drag_ay);
				} else {
					CommitPointTool();
				}
			} else {
				_dragging = true;
				_drag_remove = _ctrl_pressed && _tool != MiniTool::Convert && !IsBridgeTool(_tool);
				_drag_ax = MapXAt(_cursor.pos.y);
				_drag_ay = MapYAt(_cursor.pos.x);
				if (_tool == MiniTool::Rail) {
					_plan.path.clear();
					UpdateRailPlan(_drag_ax, _drag_ay);
				}
				if (_tool == MiniTool::Road || IsBridgeTool(_tool)) UpdateRoadPlan(_drag_ax, _drag_ay);
				if (IsRectTool(_tool)) UpdateRectPlan(_drag_ax, _drag_ay, RectPlanLimit());
			}
		}
	}

	if (!_left_button_down && _prev_left && _dragging) {
		_dragging = false;
		if (_tool == MiniTool::Rail) CommitRailPlan();
		if (_tool == MiniTool::Road) CommitRoadPlan();
		if (IsBridgeTool(_tool)) CommitBridgePlan();
		if (_tool == MiniTool::Signal) CommitSignalPlan();
		if (_tool == MiniTool::Station) CommitStationPlan();
		if (_tool == MiniTool::Demolish) CommitDemolishPlan();
		if (_tool == MiniTool::Terraform) CommitTerraformPlan();
		if (_tool == MiniTool::Canal) CommitCanalPlan();
		if (_tool == MiniTool::Convert) CommitConvertPlan();
		if (_tool == MiniTool::Trees) CommitTreePlan();
		if (_tool == MiniTool::BuyLand) CommitBuyLandPlan();
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

	/* An open rename field owns the keyboard; letting the shortcuts through
	 * would rotate blueprints while typing a name. */
	if (kc != WKC_F9 && ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput) return true;

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
			} else if (_tool == MiniTool::Rail || _tool == MiniTool::Convert) {
				CycleRailType(1);
			} else if (_tool == MiniTool::Signal) {
				CycleSignalType(1);
			} else if (_tool == MiniTool::Industry) {
				CycleIndustryType(1);
			} else if (IsBridgeTool(_tool)) {
				CycleBridgeType(1);
			} else if (IsDirPointTool(_tool)) {
				_point_dir = ChangeDiagDir(_point_dir, DIAGDIRDIFF_90RIGHT);
			}
			break;

		case 'Q':
			if (_tool == MiniTool::Airport) {
				CycleAirportType(-1);
			} else if (_tool == MiniTool::Rail || _tool == MiniTool::Convert) {
				CycleRailType(-1);
			} else if (_tool == MiniTool::Signal) {
				CycleSignalType(-1);
			} else if (_tool == MiniTool::Industry) {
				CycleIndustryType(-1);
			} else if (IsBridgeTool(_tool)) {
				CycleBridgeType(-1);
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
	UpdateToasts(delta_ms);
	ProcessFleetDeploy();
	RlwCmdClear();
	MiniImGuiEnsureSetup();
	RlwImGuiNewFrame();
	if (_ms.imgui_demo != 0) ImGui::ShowDemoWindow();

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
		} else if (IsBridgeTool(_tool)) {
			UpdateRoadPlan(MapXAt(_cursor.pos.y), MapYAt(_cursor.pos.x));
			DrawBridgePlan(ppt);
		} else if (_tool == MiniTool::Signal) {
			UpdateSignalPlan(MapXAt(_cursor.pos.y), MapYAt(_cursor.pos.x));
			DrawSignalPlan(ppt);
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
