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
#include "airport.h"
#include "autoreplace_cmd.h"
#include "autoreplace_func.h"
#include "command_func.h"
#include "company_base.h"
#include "company_cmd.h"
#include "company_func.h"
#include "company_gui.h"
#include "engine_base.h"
#include "engine_cmd.h"
#include "engine_gui.h"
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
#include "fios.h"
#include "gfx_func.h"
#include "goal_base.h"
#include "graph_gui.h"
#include "ground_vehicle.hpp"
#include "group.h"
#include "group_cmd.h"
#include "gui.h"
#include "industry.h"
#include "industry_map.h"
#include "industrytype.h"
#include "ini_type.h"
#include "landscape.h"
#include "league_gui.h"
#include "linkgraph/linkgraph.h"
#include "mini_atlas.h"
#include "mini/core/camera.h"
#include "mini/core/canvas.h"
#include "mini/core/tones.h"
#include "mini/core/tuning.h"
#include "mini/dock/carrier.h"
#include "mini/dock/native_dock.h"
#include "mini/dock/native_window.h"
#include "mini/fleet/consist_draft.h"
#include "mini/fleet/fleet_deploy.h"
#include "mini/hud/build_dock.h"
#include "mini/hud/clear_panel.h"
#include "mini/hud/colony_panel.h"
#include "mini/hud/command_bar.h"
#include "mini/hud/note_layer.h"
#include "mini/hud/status_board.h"
#include "mini/hud/status_stream.h"
#include "mini/hud/toast_feed.h"
#include "mini/hud/toast_stack.h"
#include "mini/hud/window_bar.h"
#include "mini/input/input_mode.h"
#include "mini/input/press_owner.h"
#include "mini/map/ground.h"
#include "mini/map/map_labels.h"
#include "mini/map/map_overlay.h"
#include "mini/map/map_painter.h"
#include "mini/map/tile_shapes.h"
#include "mini/map/vehicle_motion.h"
#include "mini/map/vehicle_painter.h"
#include "mini/tools/blueprint.h"
#include "mini/tools/build_tool.h"
#include "mini/tools/clear_filter.h"
#include "mini/tools/command_probe.h"
#include "mini/tools/tile_pick.h"
#include "mini/tools/tool_choices.h"
#include "mini/tools/tool_estimate.h"
#include "mini/tools/tool_sites.h"
#include "mini/ui/fonts.h"
#include "mini/ui/ui_text.h"
#include "mini/ui/view_host.h"
#include "mini/windows/engine_preview_panel.h"
#include "mini/windows/finance_panel.h"
#include "mini/windows/goal_list_panel.h"
#include "mini/windows/grades.h"
#include "mini/windows/news_list_panel.h"
#include "mini/windows/sign_list_panel.h"
#include "mini/windows/station_list_panel.h"
#include "mini/windows/subsidy_list_panel.h"
#include "mini/windows/takeover_panel.h"
#include "mini/windows/town_list_panel.h"
#include "mini/windows/waypoint_panel.h"
#include "mini/windows/window_links.h"
#include "economy_cmd.h"
#include "economy_func.h"
#include "misc_cmd.h"
#include "network/network.h"
#include "network/network_type.h"
#include "newgrf_airport.h"
#include "news_gui.h"
#include "openttd.h"
#include "order_base.h"
#include "order_cmd.h"
#include "order_func.h"
#include "rail.h"
#include "palette_func.h"
#include "rail_gui.h"
#include "rail_map.h"
#include "road.h"
#include "road_map.h"
#include "roadveh_cmd.h"
#include "settings_type.h"
#include "signs_base.h"
#include "signs_cmd.h"
#include "station_base.h"
#include "station_cmd.h"
#include "station_func.h"
#include "string_func.h"
#include "strings_func.h"
#include "station_map.h"
#include "subsidy_base.h"
#include "town.h"
#include "town_cmd.h"
#include "tile_map.h"
#include "train.h"
#include "tree_map.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_economy.h"
#include "timer/timer_game_tick.h"
#include "tunnelbridge_map.h"
#include "vehicle_base.h"
#include "vehicle_cmd.h"
#include "waypoint_base.h"
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

#include "imgui.h"

#include "safeguards.h"

static bool _mini_active = false;

/* Mini UI frame size in pixels; drawing goes through the raylib command
 * buffer, so this only mirrors the screen dimensions. */
static int _fbw, _fbh;

static bool _prev_left = false;
static PressOwner _press_owner;

/* Screen chrome follows the reference HUD: warm dark panels with a thin
 * darker edge, teal active state, cyan accent, warm off-white text. */
static const uint32_t COL_CH_PANEL = MINI_CH_PANEL;
static const uint32_t COL_CH_EDGE = MINI_CH_EDGE;
static const uint32_t COL_CH_TILE = MINI_CH_TILE;
static const uint32_t COL_CH_ACTIVE = MINI_CH_ACTIVE;
static const uint32_t COL_CH_TEXT = MINI_CH_TEXT;
static const uint32_t COL_CH_DIM = MINI_CH_DIM;
static const uint32_t COL_CH_ACCENT = MINI_CH_ACCENT;

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
	if (FileExists(MINI_FONT_REGULAR)) {
		ImFont *font = io.Fonts->AddFontFromFileTTF(MINI_FONT_REGULAR, (float)std::max(13, GetCharacterHeight(FS_NORMAL)));
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

/* Any unit of a consist opens its head's window, so the window always
 * describes the whole vehicle. */
static bool OpenVehicleWndAt(int sx, int sy)
{
	const Vehicle *best = nullptr;
	int best_d2 = 15 * 15;
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->type > VEH_AIRCRAFT) continue;
		if (v->vehstatus.Test(VehState::Hidden)) continue;
		int dx = _camera.ScreenX(v->y_pos / (double)TILE_SIZE) - sx;
		int dy = _camera.ScreenY(v->x_pos / (double)TILE_SIZE) - sy;
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
	std::optional<TileIndex> tile = TileUnder(_camera.MapAt(sx, sy));
	if (!tile.has_value() || !IsDepotTile(*tile)) return false;
	ShowDepotWindow(*tile, GetDepotVehicleType(*tile));
	return true;
}

/* Waypoints and buoys carry no map label, so the tile itself is the way into
 * their window. */
static bool OpenWaypointWndAt(int sx, int sy)
{
	std::optional<TileIndex> tile = TileUnder(_camera.MapAt(sx, sy));
	if (!tile.has_value() || (!IsRailWaypointTile(*tile) && !IsRoadWaypointTile(*tile) && !IsBuoyTile(*tile))) return false;
	return ShowMiniWaypointWindow(GetStationIndex(*tile));
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
				stops.emplace_back(_camera.ScreenX(TileY(st->xy) + 0.5), _camera.ScreenY(TileX(st->xy) + 0.5));
			}
		}
		i++;
	}
	if (stops.empty()) return;

	size_t legs = stops.size() > 2 ? stops.size() : stops.size() - 1;
	for (size_t n = 0; n < legs; n++) {
		auto [x0, y0] = stops[n];
		auto [x1, y1] = stops[(n + 1) % stops.size()];
		_canvas.ThickLine(x0, y0, x1, y1, 2, COL_BP);
	}
	for (auto [x, y] : stops) _canvas.FillCircle(x, y, 4, COL_BP);

	if (cur_stop >= 0) {
		auto [wx, wy] = _vehicle_motion.Position(v);
		int vx = _camera.ScreenX(wy);
		int vy = _camera.ScreenY(wx);
		_canvas.ThickLine(vx, vy, stops[cur_stop].first, stops[cur_stop].second, 2, COL_PAPER);
	}
}

static void DrawVehicleRing(int ppt)
{
	const Vehicle *v = Vehicle::GetIfValid(FrontWndVehicle());
	if (v == nullptr) return;
	auto [wx, wy] = _vehicle_motion.Position(v);
	int cx = _camera.ScreenX(wy);
	int cy = _camera.ScreenY(wx);
	int r = std::max(6, ppt / 2 + 3);
	_canvas.Frame({cx - r, cy - r, cx + r, cy + r}, 2, COL_PAPER);
}

void ShowIndustryViewWindow(IndustryID industry);

static void OpenSignListMiniWnd(SignID focus);

struct LabelOpener {
	void operator()(SignID sign) const { OpenSignListMiniWnd(sign); }
	void operator()(StationID station) const { ShowStationViewWindow(station); }
	void operator()(IndustryID industry) const { ShowIndustryViewWindow(industry); }
	void operator()(TownID town) const { ShowTownViewWindow(town); }
};

static bool HandleLabelClick(int x, int y)
{
	std::optional<LabelTarget> target = _map_labels.HitAt(x, y);
	if (!target.has_value()) return false;
	std::visit(LabelOpener{}, *target);
	return true;
}

static void OpenFleetMiniWnd(int vt);
static void OpenFinanceMiniWnd();
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
		case MiniWin::CompanyInfo: if (company) OpenCompanyWindow(); break;
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
		case MiniWin::Signs: OpenSignListMiniWnd(SignID::Invalid()); break;
		case MiniWin::Save: ShowSaveLoadDialog(FT_SAVEGAME, SLO_SAVE); break;
		case MiniWin::Load: ShowSaveLoadDialog(FT_SAVEGAME, SLO_LOAD); break;
		case MiniWin::Options: ShowGameOptions(); break;
		case MiniWin::Music: ShowMusicWindow(); break;
		case MiniWin::Abandon: AskExitToGameMenu(); break;
		case MiniWin::Quit: AskExitGame(); break;
	}
}

/* Mini windows: ONI-structured chrome windows on the GPU layer. A window has
 * a title bar with a rename pen and a close box, a uniform-width tab strip,
 * label-value body rows and square icon commands at the bottom. Every
 * window may embed a live native viewport through a frameless carrier
 * window that is kept aligned with its slot; carriers under higher mini
 * windows are clipped out of the native overlay. */

static const uint32_t COL_CH_RED = 0xFFE05F4AU;
static const uint32_t COL_CH_YELLOW = 0xFFE0B64AU;

enum class MiniWndKind : uint8_t {
	Vehicle,
	Station,
	Town,
	Industry,
	Fleet,
	Company,
	Group,
	IndustryList,
	League,
	Graph,
	Map,
	Native,
};

/* Kinds that carry no entity and take no commands: they read the world each
 * frame and act on a click in the body. */
static bool WndIsList(MiniWndKind kind)
{
	return kind == MiniWndKind::IndustryList || kind == MiniWndKind::League ||
			kind == MiniWndKind::Graph || kind == MiniWndKind::Map;
}

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
	bool ord_refit = false;
	bool show_hidden = false;
	int x = 0, y = 0;
	uint8_t tab = 0;
	bool want_raise = false;
	int8_t want_tab = -1;
	bool renaming = false;
	bool focus_name = false;
	GroupID rename_grp = GroupID::Invalid();
	TileIndex rename_depot = INVALID_TILE;
	TileIndex sell_arm = INVALID_TILE;
	int embed_w = 0;
	int embed_h = 0;
	bool embed_fix_w = false;
	bool embed_fix_h = false;
	int embed_step_w = 1;
	int embed_step_h = 1;
	WindowClass nat_wc = WC_NONE;
	int32_t nat_num = 0;
	uint32_t focus_seq = 0;
	Rect shell = {};
	char name_buf[128] = {};
};

static std::vector<MiniWnd> _wnds;

static std::vector<std::unique_ptr<HudPart>> CreateHudParts()
{
	std::vector<std::unique_ptr<HudPart>> parts;
	parts.push_back(std::make_unique<ColonyPanel>());
	parts.push_back(std::make_unique<StatusStream>());
	parts.push_back(std::make_unique<BuildDock>());
	parts.push_back(std::make_unique<ToastStack>(FollowNews));
	parts.push_back(std::make_unique<ClearPanel>());
	parts.push_back(std::make_unique<CommandBar>());
	parts.push_back(std::make_unique<WindowBar>(OpenMiniWindow));
	parts.push_back(std::make_unique<NoteLayer>());
	return parts;
}

static ViewHost _views(CreateHudParts());

/* Mini windows stack by ImGui focus, not by list order, so the native windows
 * under them are ordered by when each shell last held focus. */
static uint32_t _wnd_focus_tick = 0;

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
static int WndW(const MiniWnd &mw) { return std::min((WndWide(mw) ? 560 : 250) * _tuning.hud_scale, _fbw - 12 * _tuning.hud_scale); }
static int WndTitleH() { return GetCharacterHeight(FS_NORMAL) + 8 * _tuning.hud_scale; }
static int WndTabH() { return GetCharacterHeight(FS_NORMAL) + 8 * _tuning.hud_scale; }
static int WndRowH() { return GetCharacterHeight(FS_NORMAL) + 5 * _tuning.hud_scale; }
static int WndViewH() { return 100 * _tuning.hud_scale; }
static int WndCmdS() { return 26 * _tuning.hud_scale; }
static int WndPad() { return 6 * _tuning.hud_scale; }
static int WndBodyH(const MiniWnd &mw) { return WndViewH() + WndPad() + (WndWide(mw) ? 10 : 6) * WndRowH(); }
static int WndH(const MiniWnd &mw) { return WndTitleH() + WndTabH() + WndBodyH(mw) + WndCmdS() + 3 * WndPad(); }

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
	return CarrierNumber((int)mw.kind, id);
}

static std::optional<CarrierFocus> MiniCarrierFocus(const MiniWnd &mw)
{
	switch (mw.kind) {
		case MiniWndKind::Vehicle: {
			const Vehicle *v = Vehicle::GetIfValid(mw.veh);
			if (v == nullptr) return std::nullopt;
			return v->index;
		}
		case MiniWndKind::Station: {
			const Station *st = Station::GetIfValid(mw.st);
			if (st == nullptr) return std::nullopt;
			return st->rect.IsEmpty() ? st->xy : TileXY((st->rect.left + st->rect.right) / 2, (st->rect.top + st->rect.bottom) / 2);
		}
		case MiniWndKind::Town: {
			const Town *t = Town::GetIfValid(mw.town);
			if (t == nullptr) return std::nullopt;
			return t->xy;
		}
		case MiniWndKind::Group:
			return TileXY(Map::SizeX() / 2, Map::SizeY() / 2);
		default: {
			const Industry *i = Industry::GetIfValid(mw.ind);
			if (i == nullptr) return std::nullopt;
			return i->location.GetCenterTile();
		}
	}
}

static void EnsureMiniCarrier(const MiniWnd &mw, int x, int y, int w, int h)
{
	Window *cw = FindCarrier(MiniCarrierNum(mw));
	if (cw == nullptr) {
		std::optional<CarrierFocus> focus = MiniCarrierFocus(mw);
		if (!focus.has_value()) return;
		cw = OpenCarrier(MiniCarrierNum(mw), *focus);
	}
	FitCarrier(cw, x, y, w, h);
}

static void CloseMiniCarrier(const MiniWnd &mw)
{
	CloseCarrier(MiniCarrierNum(mw));
}

static void EmbedOpenCompany(WindowNumber) { ShowCompany(_local_company); }
static void EmbedOpenOperatingProfit(WindowNumber) { ShowOperatingProfitGraph(); }
static void EmbedOpenIncome(WindowNumber) { ShowIncomeGraph(); }
static void EmbedOpenCompanyValue(WindowNumber) { ShowCompanyValueGraph(); }
static void EmbedOpenPerformance(WindowNumber) { ShowPerformanceHistoryGraph(); }
static void EmbedOpenDeliveredCargo(WindowNumber) { ShowDeliveredCargoGraph(); }
static void EmbedOpenPaymentRates(WindowNumber) { ShowCargoPaymentRates(); }
static void EmbedOpenLeague(WindowNumber) { ShowPerformanceLeagueTable(); }
static void EmbedOpenRatingDetail(WindowNumber) { ShowPerformanceRatingDetail(); }
static void EmbedOpenIndustryChain(WindowNumber) { ShowIndustryCargoesWindow(); }
static void EmbedOpenIndustryProduction(WindowNumber num) { ShowIndustryProductionGraph(num); }
static void EmbedOpenTownCargo(WindowNumber num) { ShowTownCargoGraph(num); }

/* Tabs the official window fills better than a rewrite would: the manager
 * face, the plotted histories, the rating breakdown, the cargo chain. */
static bool WndEmbedTarget(const MiniWnd &mw, DockSpec &spec, WindowNumber &num)
{
	static const DockSpec graphs[] = {
		{WC_OPERATING_PROFIT, EmbedOpenOperatingProfit},
		{WC_INCOME_GRAPH, EmbedOpenIncome},
		{WC_COMPANY_VALUE, EmbedOpenCompanyValue},
		{WC_PERFORMANCE_HISTORY, EmbedOpenPerformance},
		{WC_DELIVERED_CARGO, EmbedOpenDeliveredCargo},
		{WC_PAYMENT_RATES, EmbedOpenPaymentRates},
	};

	num = 0;
	switch (mw.kind) {
		case MiniWndKind::Native:
			if (mw.nat_wc == WC_NONE) return false;
			spec = {mw.nat_wc, nullptr};
			num = mw.nat_num;
			return true;

		case MiniWndKind::Company:
			if (mw.tab != 0 || !Company::IsValidID(_local_company)) return false;
			spec = {WC_COMPANY, EmbedOpenCompany};
			num = _local_company;
			return true;

		case MiniWndKind::Graph:
			if ((size_t)mw.tab >= lengthof(graphs)) return false;
			spec = graphs[mw.tab];
			return true;

		case MiniWndKind::League:
			spec = mw.tab == 1 ? DockSpec{WC_PERFORMANCE_DETAIL, EmbedOpenRatingDetail}
					: DockSpec{WC_COMPANY_LEAGUE, EmbedOpenLeague};
			return true;

		case MiniWndKind::IndustryList:
			if (mw.tab != 3) return false;
			spec = {WC_INDUSTRY_CARGOES, EmbedOpenIndustryChain};
			return true;

		case MiniWndKind::Industry:
			if (mw.tab != 3 || !Industry::IsValidID(mw.ind)) return false;
			spec = {WC_INDUSTRY_PRODUCTION, EmbedOpenIndustryProduction};
			num = mw.ind;
			return true;

		case MiniWndKind::Town:
			if (mw.tab != 3 || !Town::IsValidID(mw.town)) return false;
			spec = {WC_TOWN_CARGO_GRAPH, EmbedOpenTownCargo};
			num = mw.town;
			return true;

		default:
			return false;
	}
}

static void CloseMiniWnd(size_t i)
{
	if (_wnds[i].kind == MiniWndKind::Native) {
		Window *nw = FindWindowById(_wnds[i].nat_wc, _wnds[i].nat_num);
		if (nw != nullptr) nw->Close();
	}
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
	int8_t tab;
};

static std::vector<MiniOpenReq> _wnd_opens;
static bool _wnds_drawing = false;

static void OpenMiniWnd(MiniWndKind kind, VehicleID veh, StationID st, TownID town = TownID::Invalid(), IndustryID ind = IndustryID::Invalid(), int8_t tab = -1)
{
	if (_wnds_drawing) {
		_wnd_opens.push_back({kind, veh, st, town, ind, tab});
		return;
	}
	for (size_t i = 0; i < _wnds.size(); i++) {
		if (_wnds[i].kind == kind && _wnds[i].veh == veh && _wnds[i].st == st && _wnds[i].town == town && _wnds[i].ind == ind) {
			RaiseMiniWnd(i);
			if (tab >= 0) _wnds.back().tab = (uint8_t)tab;
			return;
		}
	}
	int s = _tuning.hud_scale;
	MiniWnd mw;
	mw.kind = kind;
	mw.veh = veh;
	mw.st = st;
	mw.town = town;
	mw.ind = ind;
	if (tab >= 0) mw.tab = (uint8_t)tab;
	mw.x = Clamp(_fbw - 6 * s - WndW(mw) - (int)_wnds.size() * 20 * s, 0, std::max(0, _fbw - WndW(mw)));
	mw.y = Clamp(WindowBarBottom() + 6 * s + (int)_wnds.size() * 20 * s, 0, std::max(0, _fbh - WndH(mw)));
	_wnds.push_back(mw);
}

static void CloseAllMiniWnds()
{
	for (const MiniWnd &mw : _wnds) CloseMiniCarrier(mw);
	_dock.CloseAll();
	_wnds.clear();
	_wnd_opens.clear();
	_views.CloseAll();
}

static void OpenFleetMiniWnd(int vt)
{
	OpenMiniWnd(MiniWndKind::Fleet, VehicleID::Invalid(), StationID::Invalid(), TownID::Invalid(), IndustryID::Invalid(), (int8_t)vt);
}

void OpenVehicleWindow(VehicleID vehicle)
{
	OpenMiniWnd(MiniWndKind::Vehicle, vehicle, StationID::Invalid());
}

void OpenStationWindow(StationID station)
{
	OpenMiniWnd(MiniWndKind::Station, VehicleID::Invalid(), station);
}

void OpenTownWindow(TownID town)
{
	OpenMiniWnd(MiniWndKind::Town, VehicleID::Invalid(), StationID::Invalid(), town);
}

void OpenIndustryWindow(IndustryID industry)
{
	OpenMiniWnd(MiniWndKind::Industry, VehicleID::Invalid(), StationID::Invalid(), TownID::Invalid(), industry);
}

void ScrollToTile(TileIndex tile)
{
	MiniUiScrollTo(TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE);
}

static void OpenFinanceMiniWnd()
{
	_views.Show(std::make_unique<FinancePanel>());
}

void OpenCompanyWindow()
{
	OpenMiniWnd(MiniWndKind::Company, VehicleID::Invalid(), StationID::Invalid());
}

static void OpenGroupMiniWnd(int vt)
{
	OpenMiniWnd(MiniWndKind::Group, VehicleID::Invalid(), StationID::Invalid(), TownID::Invalid(), IndustryID::Invalid(), (int8_t)vt);
}

static void OpenStationListMiniWnd()
{
	_views.Show(std::make_unique<StationListPanel>());
}

static void OpenTownListMiniWnd()
{
	_views.Show(std::make_unique<TownListPanel>());
}

static void OpenIndustryListMiniWnd()
{
	OpenMiniWnd(MiniWndKind::IndustryList, VehicleID::Invalid(), StationID::Invalid());
}

static void OpenNewsListMiniWnd()
{
	_views.Show(std::make_unique<NewsListPanel>());
}

static void OpenSubsidyListMiniWnd()
{
	_views.Show(std::make_unique<SubsidyListPanel>());
}

static void OpenGoalListMiniWnd()
{
	_views.Show(std::make_unique<GoalListPanel>());
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

/* ImGui mini windows: layout, clipping, scroll and input routing come from
 * ImGui; rows execute their commands at the click site. */

static int _imrow;

/* Outer window box of the mini window being drawn; a native slot needs it to
 * report how much window the chrome takes on top of the native size. */
static ImVec2 _imwnd_outer;

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
	float vh = (float)(100 * _tuning.hud_scale);
	ImVec2 pos = ImGui::GetCursorScreenPos();
	float vw = ImGui::GetContentRegionAvail().x;
	if (vw < 32.0f || _fbw <= 0 || _fbh <= 0) return;
	EnsureMiniCarrier(mw, (int)pos.x, (int)pos.y, (int)vw, (int)vh);
	uintptr_t tid = RlwScreenTexture().id;
	if (tid != 0) {
		ImVec2 uv0(pos.x / (float)_fbw, pos.y / (float)_fbh);
		ImVec2 uv1((pos.x + vw) / (float)_fbw, (pos.y + vh) / (float)_fbh);
		ImGui::Image((ImTextureID)tid, ImVec2(vw, vh), uv0, uv1);
	} else {
		ImGui::Dummy(ImVec2(vw, vh));
	}
}

/* The native grows in whole resize steps while the shell would drag pixel by
 * pixel; snapping the shell onto the same grid keeps the two flush instead of
 * leaving a strip of the body cut off or a strip of empty shell. */
struct MiniSizeGrid {
	float base_x, base_y, step_x, step_y;
};

static MiniSizeGrid _size_grid;

static void RecordShellRect(MiniWnd &mw)
{
	ImVec2 p = ImGui::GetWindowPos();
	ImVec2 sz = ImGui::GetWindowSize();
	mw.shell = {(int)p.x, (int)p.y, (int)(p.x + sz.x) - 1, (int)(p.y + sz.y) - 1};
}

static void MiniSizeSnap(ImGuiSizeCallbackData *data)
{
	const MiniSizeGrid *g = (const MiniSizeGrid *)data->UserData;
	if (g->step_x > 1.0f) data->DesiredSize.x = g->base_x + std::floor(std::max(0.0f, data->DesiredSize.x - g->base_x) / g->step_x) * g->step_x;
	if (g->step_y > 1.0f) data->DesiredSize.y = g->base_y + std::floor(std::max(0.0f, data->DesiredSize.y - g->base_y) / g->step_y) * g->step_y;
}

/* A native window keeps its own minimum, so the shell grows to what last
 * frame's slot reported instead of cropping the content. Axes the native cannot
 * resize are pinned to that size, which the return value reports. */
static bool MiniShellConstraints(MiniWnd &mw, ImVec2 def_size)
{
	ImVec2 min_size(mw.embed_fix_w ? (float)mw.embed_w : std::max(def_size.x, (float)mw.embed_w),
			mw.embed_fix_h ? (float)mw.embed_h : std::max(def_size.y, (float)mw.embed_h));
	min_size.x = std::min(min_size.x, (float)_fbw);
	min_size.y = std::min(min_size.y, (float)_fbh);
	_size_grid = {min_size.x, min_size.y, (float)mw.embed_step_w, (float)mw.embed_step_h};
	ImGui::SetNextWindowSizeConstraints(min_size,
			ImVec2(mw.embed_fix_w ? min_size.x : FLT_MAX, mw.embed_fix_h ? min_size.y : FLT_MAX),
			(mw.embed_step_w > 1 || mw.embed_step_h > 1) ? MiniSizeSnap : nullptr, &_size_grid);

	bool pinned = mw.embed_fix_w && mw.embed_fix_h;
	mw.embed_step_w = 1;
	mw.embed_step_h = 1;
	mw.embed_w = 0;
	mw.embed_h = 0;
	mw.embed_fix_w = false;
	mw.embed_fix_h = false;
	return pinned;
}

/* An embed that grew the window can push it past the framebuffer, where the
 * native paint under the slot would be clipped away. */
static void ClampShellToScreen(const MiniWnd &mw)
{
	if (mw.embed_w <= 0) return;
	ImVec2 wp = ImGui::GetWindowPos();
	ImVec2 ws = ImGui::GetWindowSize();
	ImVec2 np(Clamp(wp.x, 0.0f, std::max(0.0f, (float)_fbw - ws.x)), Clamp(wp.y, 0.0f, std::max(0.0f, (float)_fbh - ws.y)));
	if (np.x != wp.x || np.y != wp.y) ImGui::SetWindowPos(np);
}

static void ImWndNativeSlot(MiniWnd &mw, const DockSpec &spec, WindowNumber num)
{
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImVec2 avail = ImGui::GetContentRegionAvail();
	if (avail.x < 32.0f || avail.y < 32.0f || _fbw <= 0 || _fbh <= 0) return;

	Window *w = _dock.Open(spec, num);
	if (w == nullptr) {
		if (spec.open != nullptr) ImWndText("표시할 내용이 없습니다", COL_CH_DIM);
		return;
	}

	NativeSizing sizing = SizingOf(w);
	mw.embed_fix_w = sizing.fix_w;
	mw.embed_fix_h = sizing.fix_h;
	mw.embed_step_w = sizing.step_w;
	mw.embed_step_h = sizing.step_h;
	/* The floor is the native minimum, not its current size, or every widening
	 * drag would ratchet the mini window and never let it back. An axis the
	 * native cannot resize becomes a hard size, so a drag cannot open dead
	 * space the native will never fill. */
	mw.embed_w = sizing.min_w + (int)(_imwnd_outer.x - avail.x);
	mw.embed_h = sizing.min_h + (int)(_imwnd_outer.y - avail.y);

	int grip = sizing.Pinned() ? 0 : (int)ImGui::GetStyle().WindowPadding.x + 8 * _tuning.hud_scale;
	Rect slot = {(int)pos.x, (int)pos.y, (int)pos.x + (int)avail.x - 1, (int)pos.y + (int)avail.y - 1};
	Rect vis = _dock.Pin(w, spec.open != nullptr, slot, sizing, grip);
	ImVec2 size((float)vis.Width(), (float)vis.Height());

	/* An item under the cursor keeps ImGui from dragging the mini window, so
	 * a press inside the slot is free to reach the native widget. */
	ImGui::InvisibleButton("##native", size);
	uintptr_t tid = RlwScreenTexture().id;
	if (tid == 0) return;
	ImVec2 uv0((float)vis.left / (float)_fbw, (float)vis.top / (float)_fbh);
	ImVec2 uv1((float)(vis.left + vis.Width()) / (float)_fbw, (float)(vis.top + vis.Height()) / (float)_fbh);
	ImGui::GetWindowDrawList()->AddImage((ImTextureID)tid, pos, ImVec2(pos.x + size.x, pos.y + size.y), uv0, uv1);
}

struct ImStripUnit {
	int len8 = 8;
	uint32_t fill = 0;
	bool engine = false;
	bool selected = false;
};

static int ImWndUnitStrip(const std::vector<ImStripUnit> &units)
{
	int s = _tuning.hud_scale;
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
	ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + bh + 2.0f * _tuning.hud_scale));
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

/* Clicking a sign on the map opens the list already editing that sign, which
 * is the only place a sign can be named. */
static void OpenSignListMiniWnd(SignID focus)
{
	Panel *list = _views.Show(std::make_unique<SignListPanel>());
	const Sign *sign = Sign::GetIfValid(focus);
	if (list != nullptr && sign != nullptr) list->BeginEdit(SignListPanel::EditKey(focus), sign->name);
}

static bool _order_clone_share = false;

static void ImVehicleBody(MiniWnd &mw, const Vehicle *v)
{
	bool own = v->owner == _local_company;
	switch (mw.tab) {
		case 0: {
			if (v->vehstatus.Test(VehState::Crashed)) {
				ImWndText(GameText(STR_VEHICLE_STATUS_CRASHED), COL_CH_RED);
			} else if (v->vehstatus.Test(VehState::Stopped)) {
				ImWndText(GameText(STR_VEHICLE_STATUS_STOPPED), COL_CH_RED);
			} else if (v->current_order.IsType(OT_GOTO_STATION)) {
				ImWndText(GameText(STR_STATION_NAME, v->current_order.GetDestination().ToStationID()), COL_CH_ACCENT);
			} else if (v->current_order.IsType(OT_GOTO_DEPOT)) {
				ImWndText("차고로 이동 중", COL_CH_ACCENT);
			} else {
				ImWndText("-", COL_CH_DIM);
			}
			ImWndKV("속도", fmt::format("{} / {}", v->GetDisplaySpeed(), v->GetDisplayMaxSpeed()), COL_CH_TEXT);
			ImWndText(GameText(STR_VEHICLE_INFO_RELIABILITY_BREAKDOWNS, v->reliability * 100 >> 16, v->breakdowns_since_last_service), COL_CH_TEXT);
			/* Unsticking a vehicle has no other home in the mini UI: a train
			 * held at a danger signal or facing the wrong way can only be
			 * fixed from here. */
			if (own && v->type == VEH_TRAIN) {
				if (ImWndLink("방향 전환", COL_CH_TEXT)) {
					Command<CMD_REVERSE_TRAIN_DIRECTION>::Post(STR_ERROR_CAN_T_REVERSE_DIRECTION_TRAIN, v->tile, v->index, false);
				}
				if (Train::From(v)->flags.Test(VehicleRailFlag::Stuck) && ImWndLink("신호 강행", COL_CH_YELLOW)) {
					Command<CMD_FORCE_TRAIN_PROCEED>::Post(STR_ERROR_CAN_T_MAKE_TRAIN_PASS_SIGNAL, v->tile, v->index);
				}
			}
			if (own && v->type == VEH_ROAD && ImWndLink("회차", COL_CH_TEXT)) {
				Command<CMD_TURN_ROADVEH>::Post(STR_ERROR_CAN_T_MAKE_ROAD_VEHICLE_TURN, v->tile, v->index);
			}
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
				ImWndKV(GameText(CargoSpec::Get(ct)->name), fmt::format("{} / {}", stored, cap), COL_CH_TEXT);
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
					if (ImWndLink(fmt::format("{}{}", cur ? "▶ " : "· ", GameText(cs->name)), cur ? COL_CH_ACCENT : COL_CH_TEXT)) {
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
					case OT_GOTO_STATION: label = GameText(STR_STATION_NAME, o.GetDestination().ToStationID()); break;
					case OT_GOTO_WAYPOINT: label = GameText(STR_WAYPOINT_NAME, o.GetDestination().ToStationID()); break;
					/* A route with several depot stops read as a list of
					 * identical rows, so the row names the depot. */
					case OT_GOTO_DEPOT:
						label = o.GetDepotActionType().Test(OrderDepotActionFlag::NearestDepot)
								? std::string("가까운 차고")
								: GameText(STR_DEPOT_NAME, v->type, o.GetDestination());
						break;
					case OT_CONDITIONAL: label = fmt::format("조건 {}번", o.GetConditionSkipToOrder() + 1); break;
					default: break;
				}
				if (!label.empty()) {
					if (o.IsType(OT_GOTO_STATION)) {
						if (o.GetNonStopType().Test(OrderNonStopFlag::NoIntermediate)) label += " · 비정차";
						if (o.GetNonStopType().Test(OrderNonStopFlag::NoDestination)) label += " · 경유";
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
					} else if (o.IsType(OT_GOTO_DEPOT)) {
						if (o.GetDepotActionType().Test(OrderDepotActionFlag::Unbunch)) label += " · 간격 조정";
						if (o.GetDepotActionType().Test(OrderDepotActionFlag::Halt)) label += " · 정지";
						if (o.GetDepotOrderType().Test(OrderDepotTypeFlag::Service)) label += " · 필요할 때만";
					}
					if (o.IsRefit()) {
						label += fmt::format(" · 개조 {}", o.IsAutoRefit() ? std::string("자동") : GameText(CargoSpec::Get(o.GetRefitCargo())->name));
					}
					bool cur = oi == v->cur_real_order_index;
					if (ImWndLink(fmt::format("{}{}. {}", cur ? "▶ " : "", oi + 1, label), mw.sel_ord == oi ? COL_CH_ACCENT : (cur ? COL_CH_YELLOW : COL_CH_TEXT))) {
						mw.sel_ord = mw.sel_ord == oi ? -1 : (int16_t)oi;
						mw.ord_refit = false;
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
						if (ImWndKVLink("적재", GameText(OrderLoadStr(so->GetLoadType())), COL_CH_DIM, COL_CH_TEXT)) {
							OrderLoadType next;
							switch (so->GetLoadType()) {
								case OrderLoadType::LoadIfPossible: next = OrderLoadType::FullLoad; break;
								case OrderLoadType::FullLoad: next = OrderLoadType::FullLoadAny; break;
								case OrderLoadType::FullLoadAny: next = OrderLoadType::NoLoad; break;
								default: next = OrderLoadType::LoadIfPossible; break;
							}
							Command<CMD_MODIFY_ORDER>::Post(STR_ERROR_CAN_T_MODIFY_THIS_ORDER, v->tile, v->index, (VehicleOrderID)mw.sel_ord, MOF_LOAD, to_underlying(next));
						}
						if (ImWndKVLink("하차", GameText(OrderUnloadStr(so->GetUnloadType())), COL_CH_DIM, COL_CH_TEXT)) {
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
					/* Whether a train stops at what it passes is a per-order
					 * choice on a shared line, not a global setting. */
					if (so->IsType(OT_GOTO_STATION) && v->IsGroundVehicle()) {
						OrderNonStopFlags ns = so->GetNonStopType();
						int at = (ns.Test(OrderNonStopFlag::NoIntermediate) ? 1 : 0) | (ns.Test(OrderNonStopFlag::NoDestination) ? 2 : 0);
						static const std::string_view ns_names[] = {"모든 역 정차", "비정차", "경유", "비정차 경유"};
						if (ImWndKVLink("정차", ns_names[at], COL_CH_DIM, COL_CH_TEXT)) {
							int nx = (at + 1) % 4;
							OrderNonStopFlags next{};
							if ((nx & 1) != 0) next.Set(OrderNonStopFlag::NoIntermediate);
							if ((nx & 2) != 0) next.Set(OrderNonStopFlag::NoDestination);
							Command<CMD_MODIFY_ORDER>::Post(STR_ERROR_CAN_T_MODIFY_THIS_ORDER, v->tile, v->index, (VehicleOrderID)mw.sel_ord, MOF_NON_STOP, next.base());
						}
					}
					/* Spacing a fleet out is what a depot order is normally for,
					 * so the row cycles the depot action rather than hiding
					 * unbunching behind a timetable. */
					if (so->IsType(OT_GOTO_DEPOT)) {
						OrderDepotActionFlags da = so->GetDepotActionType();
						OrderDepotAction cur;
						if (da.Test(OrderDepotActionFlag::Unbunch)) {
							cur = OrderDepotAction::Unbunch;
						} else if (da.Test(OrderDepotActionFlag::Halt)) {
							cur = OrderDepotAction::Stop;
						} else if (so->GetDepotOrderType().Test(OrderDepotTypeFlag::Service)) {
							cur = OrderDepotAction::Service;
						} else {
							cur = OrderDepotAction::AlwaysGo;
						}
						static const std::string_view da_names[] = {"항상 입고", "필요할 때만", "입고 후 정지", "입고 후 간격 조정"};
						if (ImWndKVLink("차고 동작", da_names[(int)cur], COL_CH_DIM, COL_CH_TEXT)) {
							OrderDepotAction next = (OrderDepotAction)(((int)cur + 1) % (int)OrderDepotAction::End);
							Command<CMD_MODIFY_ORDER>::Post(STR_ERROR_CAN_T_MODIFY_THIS_ORDER, v->tile, v->index, (VehicleOrderID)mw.sel_ord, MOF_DEPOT_ACTION, to_underlying(next));
						}
					}
					/* A route that runs loaded both ways needs the vehicle to
					 * change what it carries along the way, so the refit rides on
					 * the order instead of on the vehicle. */
					if ((so->IsType(OT_GOTO_STATION) || so->IsType(OT_GOTO_DEPOT)) && so->GetLoadType() != OrderLoadType::NoLoad) {
						CargoTypes mask = 0;
						for (const Vehicle *u = v; u != nullptr; u = u->Next()) mask |= u->GetEngine()->info.refit_mask;
						if (mask != 0) {
							CargoType rc = so->GetRefitCargo();
							std::string cur = rc == CARGO_NO_REFIT ? std::string("안 함")
									: (rc == CARGO_AUTO_REFIT ? std::string("자동") : GameText(CargoSpec::Get(rc)->name));
							if (ImWndKVLink("개조", cur, COL_CH_DIM, mw.ord_refit ? COL_CH_ACCENT : COL_CH_TEXT)) mw.ord_refit = !mw.ord_refit;
							if (mw.ord_refit) {
								auto set_refit = [&](CargoType ct) {
									Command<CMD_ORDER_REFIT>::Post(STR_ERROR_CAN_T_MODIFY_THIS_ORDER, v->tile, v->index, (VehicleOrderID)mw.sel_ord, ct);
									mw.ord_refit = false;
								};
								bool none = rc == CARGO_NO_REFIT;
								if (ImWndLink(none ? "▶ 안 함" : "· 안 함", none ? COL_CH_ACCENT : COL_CH_TEXT)) set_refit(CARGO_NO_REFIT);
								if (so->IsType(OT_GOTO_STATION)) {
									bool automatic = rc == CARGO_AUTO_REFIT;
									if (ImWndLink(automatic ? "▶ 자동" : "· 자동", automatic ? COL_CH_ACCENT : COL_CH_TEXT)) set_refit(CARGO_AUTO_REFIT);
								}
								for (const CargoSpec *cs : _sorted_cargo_specs) {
									if (!HasBit(mask, cs->Index())) continue;
									bool sel = rc == cs->Index();
									if (ImWndLink(fmt::format("{}{}", sel ? "▶ " : "· ", GameText(cs->name)), sel ? COL_CH_ACCENT : COL_CH_TEXT)) set_refit(cs->Index());
								}
							}
						}
					}
					if (ImWndLink("삭제", COL_CH_RED)) {
						Command<CMD_DELETE_ORDER>::Post(STR_ERROR_CAN_T_DELETE_THIS_ORDER, v->tile, v->index, (VehicleOrderID)mw.sel_ord);
						mw.sel_ord = -1;
					}
				}
			}
			if (own) {
				bool picking = _mode.OrderVehicle() == v->index;
				if (ImWndLink(picking ? "추가 중. 지도에서 목적지 클릭, ESC 종료" : "+ 목적지 추가", COL_CH_ACCENT)) _mode.TogglePickOrders(v->index);

				/* A fleet runs one route, so taking the list off a vehicle that
				 * already has it beats typing the stops in again per vehicle. */
				if (v->orders != nullptr && v->orders->GetNumVehicles() > 1) {
					if (ImWndLink("주문 공유 해제", COL_CH_YELLOW)) {
						Command<CMD_CLONE_ORDER>::Post(STR_ERROR_CAN_T_STOP_SHARING_ORDER_LIST, v->tile, CO_UNSHARE, v->index, VehicleID::Invalid());
					}
				}
				std::vector<const Vehicle *> srcs;
				for (const Vehicle *o : Vehicle::Iterate()) {
					if (o->type != v->type || !o->IsPrimaryVehicle() || o->owner != _local_company) continue;
					if (o->index == v->index || o->GetNumOrders() == 0) continue;
					if (v->orders != nullptr && o->orders == v->orders) continue;
					srcs.push_back(o);
				}
				if (!srcs.empty()) {
					ImWndHeader("주문 가져오기");
					if (ImWndKVLink("방식", _order_clone_share ? "공유" : "복사", COL_CH_DIM, COL_CH_ACCENT)) {
						_order_clone_share = !_order_clone_share;
					}
					for (const Vehicle *o : srcs) {
						std::string label = fmt::format("· {} · {}개", GameText(STR_VEHICLE_NAME, o->index), o->GetNumOrders());
						if (ImWndLink(label, COL_CH_TEXT)) {
							Command<CMD_CLONE_ORDER>::Post(_order_clone_share ? STR_ERROR_CAN_T_SHARE_ORDER_LIST : STR_ERROR_CAN_T_COPY_ORDER_LIST,
									v->tile, _order_clone_share ? CO_SHARE : CO_COPY, v->index, o->index);
						}
					}
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
			ImWndText(GameText(STR_VEHICLE_INFO_PROFIT_THIS_YEAR_LAST_YEAR, v->GetDisplayProfitThisYear(), v->GetDisplayProfitLastYear()), COL_CH_TEXT);
			if (v->type == VEH_TRAIN) {
				ImWndKV("총길이", fmt::format("{:.1f}타일", Train::From(v)->gcache.cached_total_length / (double)TILE_SIZE), COL_CH_TEXT);
			}
			/* How often a vehicle services decides how often it breaks down, so
			 * the interval is the one setting that belongs on the vehicle. */
			if (own) {
				bool pct = v->ServiceIntervalIsPercent();
				bool wall = TimerGameEconomy::UsingWallclockUnits();
				uint si = v->GetServiceInterval();
				uint lo = pct ? MIN_SERVINT_PERCENT : (wall ? MIN_SERVINT_MINUTES : MIN_SERVINT_DAYS);
				uint hi = pct ? MAX_SERVINT_PERCENT : (wall ? MAX_SERVINT_MINUTES : MAX_SERVINT_DAYS);
				uint step = pct ? 5 : (wall ? 1 : 10);
				std::string val = pct ? fmt::format("{}%", si) : fmt::format("{}", si);
				if (!v->ServiceIntervalIsCustom()) val += " 기본";
				ImWndKV("정비 간격", val, COL_CH_TEXT);
				if (ImWndLink("간격 늘리기", COL_CH_TEXT)) {
					Command<CMD_CHANGE_SERVICE_INT>::Post(v->index, (uint16_t)std::min(si + step, hi), true, pct);
				}
				if (ImWndLink("간격 줄이기", COL_CH_TEXT)) {
					Command<CMD_CHANGE_SERVICE_INT>::Post(v->index, (uint16_t)std::max(si > step ? si - step : lo, lo), true, pct);
				}
				if (v->ServiceIntervalIsCustom() && ImWndLink("간격 기본값", COL_CH_TEXT)) {
					Command<CMD_CHANGE_SERVICE_INT>::Post(v->index, (uint16_t)si, false, pct);
				}
			}
			if (v->type == VEH_TRAIN || (v->type == VEH_ROAD && _settings_game.vehicle.roadveh_acceleration_model != AM_ORIGINAL)) {
				const GroundVehicleCache *gc = v->GetGroundVehicleCache();
				int64_t ms = PackVelocity(v->GetDisplayMaxSpeed(), v->type);
				if (v->type == VEH_TRAIN && (_settings_game.vehicle.train_acceleration_model == AM_ORIGINAL ||
						Train::From(v)->GetAccelerationType() == VehicleAccelerationModel::Maglev)) {
					ImWndText(GameText(STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED, gc->cached_weight, gc->cached_power, ms), COL_CH_TEXT);
				} else {
					ImWndText(GameText(STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED_MAX_TE, gc->cached_weight, gc->cached_power, ms, gc->cached_max_te), COL_CH_TEXT);
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
				/* The rating decides how much of what waits actually gets
				 * picked up, so it belongs beside the count. */
				uint pct = ToPercent8(ge.rating);
				ImWndKV(GameText(cs->name), fmt::format("{}  {}%", ge.TotalCount(), pct),
						pct < 40 ? COL_CH_RED : (pct < 70 ? COL_CH_YELLOW : COL_CH_TEXT));
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
					if (ImWndLink(GameText(STR_INDUSTRY_NAME, i->index), COL_CH_TEXT)) {
						MiniUiScrollTo(TileX(i->location.tile) * TILE_SIZE, TileY(i->location.tile) * TILE_SIZE);
					}
				}
			}
			if (!st->industries_near.empty()) {
				any = true;
				ImWndHeader("납품처");
				for (const IndustryListEntry &e : st->industries_near) {
					if (ImWndLink(GameText(STR_INDUSTRY_NAME, e.industry->index), COL_CH_TEXT)) {
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
					if (ImWndLink(GameText(STR_VEHICLE_NAME, lv->index), here ? COL_CH_ACCENT : COL_CH_TEXT)) {
						OpenMiniWnd(MiniWndKind::Vehicle, lv->First()->index, StationID::Invalid());
					}
				}
			}
			if (!any) ImWndText("이 역에 오는 차량 없음", COL_CH_DIM);
			break;
		}

		case 3: {
			if (st->town != nullptr) {
				if (ImWndKVLink("도시", GameText(STR_TOWN_NAME, st->town->index), COL_CH_DIM, COL_CH_TEXT)) {
					OpenMiniWnd(MiniWndKind::Town, VehicleID::Invalid(), StationID::Invalid(), st->town->index);
				}
			}
			if (Company::IsValidID(st->owner)) {
				ImWndKV("소유", GameText(STR_COMPANY_NAME, st->owner), COL_CH_TEXT);
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
			ImWndText(GameText(STR_LAND_AREA_INFORMATION_BUILD_DATE, st->build_date), COL_CH_TEXT);
			if (st->facilities.Test(StationFacility::Train) && st->train_station.tile != INVALID_TILE) {
				uint longest = 0;
				for (TileIndex ti : st->train_station) {
					if (!st->TileBelongsToRailStation(ti)) continue;
					longest = std::max(longest, st->GetPlatformLength(ti));
				}
				ImWndKV(GameText(STR_STATION_BUILD_PLATFORM_LENGTH), fmt::format("{}칸", longest), COL_CH_TEXT);
			}
			ImWndText(GameText(STR_STATION_VIEW_ACCEPTS_CARGO, GetAcceptanceMask(st)), COL_CH_TEXT);
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
				ImWndKV(GameText(cs->name), fmt::format("{} {}%", GameText(STR_CARGO_RATING_APPALLING + (ge.rating >> 5)), pct), tint);
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
			ImWndText(GameText(STR_TOWN_VIEW_POPULATION_HOUSES, t->cache.population, t->cache.num_houses), COL_CH_TEXT);
			if (t->flags.Test(TownFlag::IsGrowing)) {
				StringID str = t->fund_buildings_months == 0 ? STR_TOWN_VIEW_TOWN_GROWS_EVERY : STR_TOWN_VIEW_TOWN_GROWS_EVERY_FUNDED;
				ImWndText(GameText(str, RoundDivSU(t->growth_rate + 1, Ticks::DAY_TICKS)), COL_CH_TEXT);
			} else {
				ImWndText(GameText(STR_TOWN_VIEW_TOWN_GROW_STOPPED), COL_CH_YELLOW);
			}
			if (t->larger_town) ImWndText("대도시", COL_CH_ACCENT);
			if (_settings_game.economy.station_noise_level) {
				ImWndText(GameText(STR_TOWN_VIEW_NOISE_IN_TOWN, t->noise_reached, t->MaxTownNoise()), COL_CH_TEXT);
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
				std::string name = GameText(STR_COMPANY_NAME, c->index);
				if (t->exclusivity == c->index) name += " · 독점";
				ImWndKV(name, GameText(TownRatingString(rating)), tint);
			}
			if (!any) ImWndText("회사 평가 없음", COL_CH_DIM);

			ImWndHeader(GameText(STR_LOCAL_AUTHORITY_ACTIONS_TITLE));
			TownActions enabled = MiniEnabledTownActions();
			TownActions avail = GetMaskOfTownActions(_local_company, t);
			for (TownAction a = {}; a != TownAction::End; ++a) {
				if (!enabled.Test(a)) continue;
				Money price = _price[PR_TOWN_ACTION] * GetTownActionCost(a) >> 8;
				bool can = avail.Test(a);
				bool sel = mw.sel_ord == (int16_t)to_underlying(a);
				uint32_t tint = sel ? COL_CH_ACCENT : can ? COL_CH_TEXT : COL_CH_DIM;
				std::string label = GameText(STR_LOCAL_AUTHORITY_ACTION_SMALL_ADVERTISING_CAMPAIGN + to_underlying(a));
				std::string cost = GameText(STR_JUST_CURRENCY_LONG, price);
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
					ImWndText(GameText(str_last, 1ULL << ct, transported, production), COL_CH_TEXT);
				}
			}
			bool first = true;
			for (int i = TAE_BEGIN; i < TAE_END; i++) {
				if (t->goal[i] == 0) continue;
				if (t->goal[i] == TOWN_GROWTH_WINTER && (TileHeight(t->xy) < LowestSnowLine() || t->cache.population <= 90)) continue;
				if (t->goal[i] == TOWN_GROWTH_DESERT && (GetTropicZone(t->xy) != TROPICZONE_DESERT || t->cache.population <= 60)) continue;
				if (first) {
					ImWndHeader(GameText(STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH));
					first = false;
				}
				const CargoSpec *cargo = FindFirstCargoWithTownAcceptanceEffect((TownAcceptanceEffect)i);
				if (cargo == nullptr) continue;
				if (t->goal[i] == TOWN_GROWTH_DESERT || t->goal[i] == TOWN_GROWTH_WINTER) {
					bool done = t->received[i].old_act > 0;
					ImWndKV(GameText(cargo->name), done ? "공급됨" : "필요", done ? COL_CH_TEXT : COL_CH_YELLOW);
				} else {
					bool done = t->received[i].old_act >= t->goal[i];
					ImWndKV(GameText(cargo->name), fmt::format("{} / {}", t->received[i].old_act, t->goal[i]), done ? COL_CH_TEXT : COL_CH_YELLOW);
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
			if (i->prod_level == PRODLEVEL_CLOSURE) ImWndText(GameText(STR_INDUSTRY_VIEW_INDUSTRY_ANNOUNCED_CLOSURE), COL_CH_RED);
			bool any = false;
			for (const auto &p : i->produced) {
				if (!IsValidCargoType(p.cargo)) continue;
				any = true;
				uint pct = ToPercent8(p.history[LAST_MONTH].PctTransported());
				uint32_t tint = pct < 25 ? COL_CH_RED : pct < 50 ? COL_CH_YELLOW : COL_CH_TEXT;
				ImWndKV(GameText(CargoSpec::Get(p.cargo)->name), fmt::format("{} · {}%", p.history[LAST_MONTH].production, pct), tint);
			}
			if (!any) ImWndText("생산 없음", COL_CH_DIM);
			if (i->prod_level != PRODLEVEL_DEFAULT && i->prod_level != PRODLEVEL_CLOSURE) {
				ImWndText(GameText(STR_INDUSTRY_VIEW_PRODUCTION_LEVEL, RoundDivSU(i->prod_level * 100, PRODLEVEL_DEFAULT)), COL_CH_TEXT);
			}
			break;
		}

		case 1: {
			if (i->stations_near.empty()) {
				ImWndText("주변 역 없음", COL_CH_DIM);
				break;
			}
			for (const Station *st : i->stations_near) {
				if (ImWndLink(GameText(STR_STATION_NAME, st->index), COL_CH_TEXT)) {
					OpenMiniWnd(MiniWndKind::Station, VehicleID::Invalid(), st->index);
				}
			}
			break;
		}

		case 2: {
			ImWndText(GameText(STR_LAND_AREA_INFORMATION_BUILD_DATE, i->construction_date), COL_CH_TEXT);
			bool first = true;
			for (const auto &a : i->accepted) {
				if (!IsValidCargoType(a.cargo)) continue;
				if (first) {
					ImWndHeader(GameText(STR_INDUSTRY_VIEW_REQUIRES));
					first = false;
				}
				ImWndKV(GameText(CargoSpec::Get(a.cargo)->name), a.waiting > 0 ? fmt::format("{}", a.waiting) : std::string("-"), COL_CH_TEXT);
			}
			break;
		}
	}
}

static void ImFleetBody(MiniWnd &mw)
{
	VehicleType vt = (VehicleType)mw.tab;
	ConsistDraft &draft = FleetDraft(vt);
	draft.Prune();

	const Vehicle *sv = Vehicle::GetIfValid(mw.sel);
	if (sv != nullptr && (sv->type != vt || !sv->First()->IsChainInDepot())) {
		mw.sel = VehicleID::Invalid();
		sv = nullptr;
	}
	if (mw.rename_depot != INVALID_TILE && !IsDepotTile(mw.rename_depot)) mw.rename_depot = INVALID_TILE;

	float lw = ImGui::GetContentRegionAvail().x * 0.45f;
	ImGui::BeginChild("buy", ImVec2(lw, 0.0f));
	{
		struct BuyRow {
			EngineID eid;
			std::string name;
			Money cost;
			int64_t score;
			bool hidden;
		};
		int nhidden = 0;
		std::vector<BuyRow> locos, wags;
		for (const Engine *e : Engine::IterateType(vt)) {
			if (!e->IsEnabled() || !e->company_avail.Test(_local_company)) continue;
			bool hidden = e->IsHidden(_local_company);
			if (hidden) {
				nhidden++;
				if (!mw.show_hidden) continue;
			}
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
			r.name = GameText(STR_ENGINE_NAME, e->index);
			r.cost = e->GetCost();
			r.score = score;
			r.hidden = hidden;
			(wagon ? wags : locos).push_back(std::move(r));
		}
		auto by_score = [](const BuyRow &a, const BuyRow &b) { return a.score > b.score; };
		std::sort(locos.begin(), locos.end(), by_score);
		std::sort(wags.begin(), wags.end(), by_score);
		if (nhidden > 0 && ImWndLink(mw.show_hidden ? fmt::format("숨긴 엔진 {}종 감추기", nhidden) : fmt::format("숨긴 엔진 {}종 보기", nhidden), COL_CH_DIM)) {
			mw.show_hidden = !mw.show_hidden;
		}
		auto engine_rows = [&](const std::vector<BuyRow> &list) {
			for (const BuyRow &r : list) {
				bool clicked = ImWndKVLink(r.name, GetString(STR_JUST_CURRENCY_LONG, r.cost), r.hidden ? COL_CH_DIM : COL_CH_TEXT, r.hidden ? COL_CH_DIM : COL_CH_TEXT);
				if (ImGui::IsItemHovered()) mw.sel_eng = r.eid;
				if (clicked) draft.Add(r.eid);
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
		/* The buy row under the cursor decides what the spec panel shows, and it
		 * keeps the last one so the panel does not blank while the cursor leaves
		 * the list. */
		const Engine *he = Engine::GetIfValid(mw.sel_eng);
		if (he != nullptr && (he->type != vt || !he->IsEnabled())) {
			mw.sel_eng = EngineID::Invalid();
			he = nullptr;
		}
		if (he != nullptr) {
			ImWndHeader(GameText(STR_ENGINE_NAME, he->index));
			ImWndText(StrMakeValid(GetEngineInfoString(he->index), {}), COL_CH_TEXT);
			bool wagon = vt == VEH_TRAIN && he->VehInfo<RailVehicleInfo>().railveh_type == RAILVEH_WAGON;
			if (!wagon) ImWndKV("신뢰도", fmt::format("{}%", ToPercent16(he->reliability)), COL_CH_TEXT);
			bool hidden = he->IsHidden(_local_company);
			if (ImWndLink(hidden ? "· 숨김 해제" : "· 구매 목록에서 숨기기", COL_CH_DIM)) {
				Command<CMD_SET_VEHICLE_VISIBILITY>::Post(he->index, !hidden);
			}
		}

		ImWndHeader("설계");
		if (draft.Empty()) {
			ImWndText(vt == VEH_TRAIN ? "엔진 목록을 눌러 편성 구성" : "엔진 목록을 눌러 선택", COL_CH_DIM);
		} else {
			std::vector<ImStripUnit> units;
			for (EngineID id : draft.Units()) {
				const Engine *e = Engine::Get(id);
				ImStripUnit su;
				su.engine = vt == VEH_TRAIN && e->VehInfo<RailVehicleInfo>().railveh_type != RAILVEH_WAGON;
				su.len8 = UnitLength(e);
				CargoType dc = e->GetDefaultCargoType();
				su.fill = e->GetDisplayDefaultCapacity() > 0 && IsValidCargoType(dc) ? CargoRgb(dc) : COL_CH_TILE;
				units.push_back(su);
			}
			int del = ImWndUnitStrip(units);
			if (del >= 0) draft.Remove(del);
			DraftSummary sum = draft.Summary();
			ImWndKV("합계", GetString(STR_JUST_CURRENCY_LONG, sum.cost), COL_CH_ACCENT);
			if (vt == VEH_TRAIN) {
				ImWndText(GameText(STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED, sum.weight, sum.power, PackVelocity(sum.speed, vt)), COL_CH_TEXT);
				ImWndKV("길이", fmt::format("{:.1f}타일", sum.length / 16.0), COL_CH_TEXT);
			}
			if (sum.capacity > 0) ImWndKV("용량", fmt::format("{}", sum.capacity), COL_CH_TEXT);
			ImWndText("차고 행 클릭으로 생산 · 블록 클릭으로 제외", COL_CH_DIM);
		}
		if (_deploy.Running(vt)) ImWndText(fmt::format("생산 중 {} / {}", _deploy.Unit(), _deploy.Units()), COL_CH_ACCENT);

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
					? GameText(STR_VEHICLE_NAME, head->index)
					: GameText(STR_ENGINE_NAME, head->engine_type);
			if (len > 1) label = fmt::format("{} · {}량", label, len);
			label = fmt::format("{}{}", mw.sel == head->index ? "▶ " : "· ", label);
			bool stopped = head->vehstatus.Test(VehState::Stopped);
			if (ImWndLink(label, mw.sel == head->index ? COL_CH_ACCENT : (stopped ? COL_CH_TEXT : COL_CH_YELLOW))) {
				/* With a unit marked the row is a coupling target; otherwise it
				 * is the only way from the yard to the consist's own window. */
				const Vehicle *marked = Vehicle::GetIfValid(mw.sel);
				bool couple = marked != nullptr && marked->type == VEH_TRAIN && head->type == VEH_TRAIN &&
						marked->First() != head->First() && marked->tile == head->tile;
				if (!couple && head->IsPrimaryVehicle()) {
					OpenVehicleWindow(head->index);
				} else {
					FleetMarkUnit(mw, head->index, true);
				}
			}
			if (vt == VEH_TRAIN) {
				std::vector<VehicleID> ids;
				int hit = ImTrainStrip(Train::From(head), mw.sel, ids);
				if (hit >= 0 && (size_t)hit < ids.size()) FleetMarkUnit(mw, ids[hit], false);
			}
		};
		/* Several depots in one town share a generated name, so the row renames
		 * in place like a group row does. A hangar belongs to its station and
		 * has no depot of its own to rename. */
		auto depot_block = [&](TileIndex tile, uint dest, DepotID did) {
			anydep = true;
			VehicleList chains, wagons;
			BuildDepotVehicleList(vt, tile, &chains, &wagons);
			std::string dn = GameText(STR_DEPOT_NAME, vt, dest);
			if (mw.rename_depot == tile) {
				ImGui::PushID((int)tile.base());
				int r = ImWndNameEdit(mw, ImGui::GetContentRegionAvail().x);
				ImGui::PopID();
				if (r == 1 && mw.name_buf[0] != '\0') {
					Command<CMD_RENAME_DEPOT>::Post(STR_ERROR_CAN_T_RENAME_DEPOT, did, mw.name_buf);
				}
				if (r != 0) mw.rename_depot = INVALID_TILE;
			} else {
				if (ImWndLink(draft.Empty() ? dn : fmt::format("▶ {} 생산", dn), draft.Empty() ? COL_CH_TEXT : COL_CH_ACCENT)) {
					if (!IsDepotTile(tile)) {
						/* stale row */
					} else if (draft.Empty()) {
						MiniUiScrollTo(TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE);
					} else {
						_deploy.Start(tile, vt, draft.Units());
					}
				}
				if (did != DepotID::Invalid() && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					ImWndNameEditBegin(mw, dn);
					mw.rename_depot = tile;
					mw.renaming = false;
				}
			}
			/* Emptying a depot cannot be taken back, so the row arms first and
			 * sells on the next click. */
			if (!chains.empty() || !wagons.empty()) {
				bool armed = mw.sell_arm == tile;
				if (ImWndLink(armed ? "· 전체 매각 · 다시 눌러 확정" : "· 전체 매각", armed ? COL_CH_RED : COL_CH_DIM)) {
					if (armed) {
						Command<CMD_DEPOT_SELL_ALL_VEHICLES>::Post(GetCmdSellAllVehMsg(vt), tile, vt);
						mw.sell_arm = INVALID_TILE;
						mw.sel = VehicleID::Invalid();
					} else {
						mw.sell_arm = tile;
					}
				}
				if (ImWndLink("· 전체 교체", COL_CH_DIM)) {
					Command<CMD_DEPOT_MASS_AUTOREPLACE>::Post(GetCmdAutoreplaceVehMsg(vt), tile, vt);
				}
			}
			for (const Vehicle *head : chains) chain_rows(head);
			for (const Vehicle *head : wagons) chain_rows(head);
		};
		if (vt == VEH_AIRCRAFT) {
			for (const Station *st : Station::Iterate()) {
				if (st->owner != _local_company || !st->facilities.Test(StationFacility::Airport) || !st->airport.HasHangar()) continue;
				depot_block(st->airport.GetHangarTile(0), st->index.base(), DepotID::Invalid());
			}
		} else {
			for (const Depot *d : Depot::Iterate()) {
				if (!IsDepotTile(d->xy) || GetDepotVehicleType(d->xy) != vt) continue;
				if (GetTileOwner(d->xy) != _local_company) continue;
				depot_block(d->xy, d->index.base(), d->index);
			}
		}
		if (!anydep) ImWndText("차고 없음", COL_CH_DIM);
	}
	ImGui::EndChild();
}

/* The overview tab embeds the official company window, so only the asset
 * roll-up is drawn here. */
static void ImCompanyBody(MiniWnd &)
{
	const Company *c = Company::GetIfValid(_local_company);
	if (c == nullptr) return;

	static const StringID veh_strs[] = {STR_REPLACE_VEHICLE_TRAIN, STR_REPLACE_VEHICLE_ROAD_VEHICLE, STR_REPLACE_VEHICLE_SHIP, STR_REPLACE_VEHICLE_AIRCRAFT};
	ImWndHeader(GameText(STR_COMPANY_VIEW_VEHICLES_TITLE));
	uint fleet = 0;
	for (VehicleType vt = VEH_BEGIN; vt < VEH_COMPANY_END; vt++) {
		uint amount = c->group_all[vt].num_vehicle;
		fleet += amount;
		ImWndKV(GameText(veh_strs[vt]), fmt::format("{}대", amount), amount > 0 ? COL_CH_TEXT : COL_CH_DIM);
	}
	if (fleet == 0) ImWndText(GameText(STR_COMPANY_VIEW_VEHICLES_NONE), COL_CH_DIM);

	ImWndHeader(GameText(STR_COMPANY_VIEW_INFRASTRUCTURE));
	uint rail = c->infrastructure.GetRailTotal() + c->infrastructure.signal;
	uint road = c->infrastructure.GetRoadTotal() + c->infrastructure.GetTramTotal();
	ImWndKV("선로", fmt::format("{}", rail), rail > 0 ? COL_CH_TEXT : COL_CH_DIM);
	ImWndKV("도로", fmt::format("{}", road), road > 0 ? COL_CH_TEXT : COL_CH_DIM);
	ImWndKV("수로", fmt::format("{}", c->infrastructure.water), c->infrastructure.water > 0 ? COL_CH_TEXT : COL_CH_DIM);
	ImWndKV("역 타일", fmt::format("{}", c->infrastructure.station), c->infrastructure.station > 0 ? COL_CH_TEXT : COL_CH_DIM);
	ImWndKV("공항", fmt::format("{}", c->infrastructure.airport), c->infrastructure.airport > 0 ? COL_CH_TEXT : COL_CH_DIM);
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
		group_row(ALL_GROUP, GameText(STR_GROUP_ALL_TRAINS + vt), GetGroupNumVehicle(_local_company, ALL_GROUP, vt));
		group_row(DEFAULT_GROUP, GameText(STR_GROUP_DEFAULT_TRAINS + vt), GetGroupNumVehicle(_local_company, DEFAULT_GROUP, vt));
		std::vector<const Group *> groups;
		for (const Group *g : Group::Iterate()) {
			if (g->owner != _local_company || g->vehicle_type != vt) continue;
			groups.push_back(g);
		}
		std::sort(groups.begin(), groups.end(), [](const Group *a, const Group *b) { return a->number < b->number; });
		for (const Group *g : groups) {
			group_row(g->index, GameText(STR_GROUP_NAME, g->index), GetGroupNumVehicle(_local_company, g->index, vt));
		}

		ImWndHeader("자동 교체");
		if (sg != nullptr) {
			bool prot = sg->flags.Test(GroupFlag::ReplaceProtection);
			if (ImWndKVLink("교체 보호", prot ? "켜짐" : "꺼짐", COL_CH_DIM, prot ? COL_CH_YELLOW : COL_CH_TEXT)) {
				Command<CMD_SET_GROUP_FLAG>::Post(mw.sel_grp, GroupFlag::ReplaceProtection, !prot, false);
			}
			if (vt == VEH_TRAIN) {
				bool wr = sg->flags.Test(GroupFlag::ReplaceWagonRemoval);
				if (ImWndKVLink("화차 제거", wr ? "켜짐" : "꺼짐", COL_CH_DIM, wr ? COL_CH_YELLOW : COL_CH_TEXT)) {
					Command<CMD_SET_GROUP_FLAG>::Post(mw.sel_grp, GroupFlag::ReplaceWagonRemoval, !wr, false);
				}
			}
		}
		const Company *comp = Company::Get(_local_company);
		bool any_used = false;
		for (const Engine *e : Engine::IterateType(vt)) {
			uint num = GetGroupNumEngines(_local_company, mw.sel_grp, e->index);
			EngineID repl = EngineReplacementForCompany(comp, e->index, mw.sel_grp);
			if (num == 0 && repl == EngineID::Invalid()) continue;
			any_used = true;
			std::string label = fmt::format("{}{} · {}대", mw.sel_eng == e->index ? "▶ " : "· ",
					GameText(STR_ENGINE_NAME, e->index), num);
			if (repl != EngineID::Invalid()) label += fmt::format(" → {}", GameText(STR_ENGINE_NAME, repl));
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
				if (ImWndLink(fmt::format("· {}", GameText(STR_ENGINE_NAME, e->index)),
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
			/* A fleet is managed by finding the losers, so the list carries this
			 * year's profit and puts the worst first. */
			std::vector<const Vehicle *> list;
			for (const Vehicle *v : Vehicle::Iterate()) {
				if (v->type != vt || !v->IsPrimaryVehicle() || v->owner != _local_company) continue;
				if (mw.sel_grp == DEFAULT_GROUP && v->group_id != DEFAULT_GROUP) continue;
				list.push_back(v);
			}
			std::sort(list.begin(), list.end(), [](const Vehicle *a, const Vehicle *b) {
				return a->GetDisplayProfitThisYear() < b->GetDisplayProfitThisYear();
			});
			for (const Vehicle *v : list) {
				Money profit = v->GetDisplayProfitThisYear();
				if (ImWndKVLink(GameText(STR_VEHICLE_NAME, v->index),
						GameText(STR_JUST_CURRENCY_SHORT, profit),
						COL_CH_TEXT, profit < 0 ? COL_CH_RED : COL_CH_TEXT)) {
					OpenMiniWnd(MiniWndKind::Vehicle, v->First()->index, StationID::Invalid());
				}
			}
			if (list.empty()) ImWndText("차량 없음", COL_CH_DIM);
		} else {
			ImWndHeader("소속 차량. 클릭으로 제외");
			bool anyin = false;
			for (const Vehicle *v : Vehicle::Iterate()) {
				if (v->type != vt || !v->IsPrimaryVehicle() || v->owner != _local_company || v->group_id != mw.sel_grp) continue;
				anyin = true;
				if (ImWndLink(GameText(STR_VEHICLE_NAME, v->index), COL_CH_TEXT)) {
					Command<CMD_ADD_VEHICLE_GROUP>::Post(STR_ERROR_GROUP_CAN_T_ADD_VEHICLE, DEFAULT_GROUP, v->index, false, VehicleListIdentifier{});
				}
			}
			if (!anyin) ImWndText("소속 차량 없음", COL_CH_DIM);
			ImWndHeader("클릭으로 추가");
			bool anyout = false;
			for (const Vehicle *v : Vehicle::Iterate()) {
				if (v->type != vt || !v->IsPrimaryVehicle() || v->owner != _local_company || v->group_id == mw.sel_grp) continue;
				anyout = true;
				if (ImWndLink(GameText(STR_VEHICLE_NAME, v->index), COL_CH_TEXT)) {
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
		Row r{i->index, GameText(STR_INDUSTRY_NAME, i->index), 0, 0, i->prod_level == PRODLEVEL_CLOSURE};
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
void FollowNews(const NewsReference &ref)
{
	struct visitor {
		void operator()(const std::monostate &) {}
		void operator()(const EngineID) {}
		void operator()(const TileIndex t) { ScrollToTile(t); }
		void operator()(const VehicleID v)
		{
			const Vehicle *veh = Vehicle::GetIfValid(v);
			if (veh != nullptr) OpenVehicleWindow(veh->First()->index);
		}
		void operator()(const StationID s)
		{
			if (Station::IsValidID(s)) OpenStationWindow(s);
		}
		void operator()(const IndustryID i)
		{
			if (Industry::IsValidID(i)) OpenIndustryWindow(i);
		}
		void operator()(const TownID t)
		{
			if (Town::IsValidID(t)) OpenTownWindow(t);
		}
	};
	std::visit(visitor{}, ref);
}

enum class MiniMapMode : uint8_t {
	Contour,
	Vehicles,
	Industries,
	Routes,
	Flow,
	Vegetation,
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

		/* Which ground a tile carries decides what can be built on it, and in
		 * the tropical climate the zone decides it outright, so the zone wins
		 * over the ground colour there. */
		case MiniMapMode::Vegetation:
			switch (tt) {
				case MP_WATER: return COL_WATER;
				case MP_TREES: return Mix(COL_TREE, COL_PAPER, 60 - std::min(GetTreeCount(tile), 4U) * 15);
				case MP_CLEAR:
					if (_settings_game.game_creation.landscape == LandscapeType::Tropic) {
						switch (GetTropicZone(tile)) {
							case TROPICZONE_DESERT: return COL_DESERT;
							case TROPICZONE_RAINFOREST: return Mix(COL_TREE, COL_PAPER, 40);
							default: break;
						}
					}
					return GroundColour(tile, TileHeight(tile));
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
		case MiniMapMode::Flow:
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
		std::string name = GameText(STR_TOWN_NAME, t->index);
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

	/* One line per link the company's stations carry, thickened by how much of
	 * the link is in use, so a saturated leg reads at a glance. */
	if (mode == MiniMapMode::Flow) {
		for (const LinkGraph *lg : LinkGraph::Iterate()) {
			if (!IsValidCargoType(lg->Cargo())) continue;
			uint32_t col = CargoRgb(lg->Cargo());
			for (NodeID i = 0; i < lg->Size(); i++) {
				const LinkGraph::BaseNode &n = (*lg)[i];
				if (!Station::IsValidID(n.station) || Station::Get(n.station)->owner != _local_company) continue;
				for (const LinkGraph::BaseEdge &e : n.edges) {
					if (e.capacity == 0 || e.dest_node >= lg->Size()) continue;
					const LinkGraph::BaseNode &d = (*lg)[e.dest_node];
					if (n.xy == INVALID_TILE || d.xy == INVALID_TILE) continue;
					ImVec2 a(p.x + (float)TileY(n.xy) * w / my, p.y + (float)TileX(n.xy) * h / mx);
					ImVec2 b(p.x + (float)TileY(d.xy) * w / my, p.y + (float)TileX(d.xy) * h / mx);
					float load = std::min(1.0f, (float)e.usage / (float)e.capacity);
					dl->AddLine(a, b, MiniImU32(col), 1.0f + 2.0f * load);
				}
			}
		}
	}

	if (mode == MiniMapMode::Contour) DrawMiniMapTownNames(dl, p, w, h);

	double half_y = _fbw * 0.5 / _camera.Ppt();
	double half_x = _fbh * 0.5 / _camera.Ppt();
	dl->AddRect(ImVec2(p.x + (float)((_camera.Y() - half_y) * w / my), p.y + (float)((_camera.X() - half_x) * h / mx)),
			ImVec2(p.x + (float)((_camera.Y() + half_y) * w / my), p.y + (float)((_camera.X() + half_x) * h / mx)),
			MiniImU32(COL_CH_ACCENT), 0.0f, 0, 1.5f);

	if (ImGui::IsItemActive()) {
		ImVec2 m = ImGui::GetIO().MousePos;
		int ty = Clamp((int)((m.x - p.x) * my / w), 0, my - 1);
		int tx = Clamp((int)((m.y - p.y) * mx / h), 0, mx - 1);
		MiniUiScrollTo(tx * TILE_SIZE, ty * TILE_SIZE);
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
			bool following = v != nullptr && _mode.FollowedVehicle() == v->index;
			if (ImWndButton(following ? "추적 해제" : "따라가기", v != nullptr)) _mode.ToggleFollow(v->index);
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
			ConsistDraft &draft = FleetDraft((VehicleType)mw.tab);
			if (ImWndButton("설계 비우기", own && !draft.Empty())) draft.Clear();
			if (ImWndButton("복제", own && sv != nullptr)) {
				Command<CMD_CLONE_VEHICLE>::Post(GetCmdBuildVehMsg(sv->type), sv->tile, sv->First()->index, false);
			}
			break;
		}

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

		case MiniWndKind::IndustryList:
		case MiniWndKind::League:
		case MiniWndKind::Graph:
		case MiniWndKind::Map:
		case MiniWndKind::Native:
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

/* An adopted native window wears mini chrome and nothing else: its own caption
 * with the close box on the left is cropped away and this bar replaces it. */
static bool DrawImGuiNativeWnd(MiniWnd &mw)
{
	Window *nw = FindWindowById(mw.nat_wc, mw.nat_num);
	if (nw == nullptr) return false;

	int s = _tuning.hud_scale;
	std::string title = NativeCaption(nw);
	if (title.empty()) title = "창";
	std::string wid = fmt::format("###nat{}_{}", (int)mw.nat_wc, mw.nat_num);

	ImVec2 def_size((float)(260 * s), (float)(200 * s));
	ImGui::SetNextWindowPos(ImVec2((float)mw.x, (float)mw.y), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(def_size, ImGuiCond_FirstUseEver);
	bool pinned = MiniShellConstraints(mw, def_size);
	if (mw.want_raise) {
		ImGui::SetNextWindowFocus();
		mw.want_raise = false;
	}

	bool open = true;
	if (!ImGui::Begin(wid.c_str(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | (pinned ? ImGuiWindowFlags_NoResize : 0))) {
		ImGui::End();
		return open;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) mw.focus_seq = ++_wnd_focus_tick;
	RecordShellRect(mw);

	_imrow = 0;
	ImWndTitle(mw, title, false, open, nullptr, nullptr, nullptr);

	DockSpec spec;
	WindowNumber wnum;
	if (WndEmbedTarget(mw, spec, wnum)) {
		_imwnd_outer = ImGui::GetWindowSize();
		ImGui::BeginChild("body", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
				ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImWndNativeSlot(mw, spec, wnum);
		ImGui::EndChild();
	}

	ClampShellToScreen(mw);
	ImGui::End();
	return open;
}

static bool DrawImGuiMiniWnd(MiniWnd &mw)
{
	if (mw.kind == MiniWndKind::Native) return DrawImGuiNativeWnd(mw);

	int s = _tuning.hud_scale;
	const Vehicle *v = mw.kind == MiniWndKind::Vehicle ? Vehicle::GetIfValid(mw.veh) : nullptr;
	const Station *st = mw.kind == MiniWndKind::Station ? (Station::IsValidID(mw.st) ? Station::Get(mw.st) : nullptr) : nullptr;
	const Town *t = mw.kind == MiniWndKind::Town ? Town::GetIfValid(mw.town) : nullptr;
	const Industry *ind = mw.kind == MiniWndKind::Industry ? Industry::GetIfValid(mw.ind) : nullptr;

	std::string title = "-";
	uint32_t idnum = 0;
	switch (mw.kind) {
		case MiniWndKind::Vehicle: if (v != nullptr) title = GameText(STR_VEHICLE_NAME, v->index); idnum = mw.veh.base(); break;
		case MiniWndKind::Station: if (st != nullptr) title = GameText(STR_STATION_NAME, st->index); idnum = mw.st.base(); break;
		case MiniWndKind::Town: if (t != nullptr) title = GameText(STR_TOWN_NAME, t->index); idnum = mw.town.base(); break;
		case MiniWndKind::Fleet: title = "차고"; break;
		case MiniWndKind::Company: if (Company::IsValidID(_local_company)) title = GameText(STR_COMPANY_NAME, _local_company); break;
		case MiniWndKind::Group: title = "차량군"; break;
		case MiniWndKind::IndustryList: title = "산업 목록"; break;
		case MiniWndKind::League: title = "순위"; break;
		case MiniWndKind::Graph: title = "그래프"; break;
		case MiniWndKind::Map: title = "지도"; break;
		default: if (ind != nullptr) title = GameText(STR_INDUSTRY_NAME, ind->index); idnum = mw.ind.base(); break;
	}
	std::string wid = fmt::format("###mw{}_{}", (int)mw.kind, idnum);

	bool wide = WndWide(mw);
	ImVec2 def_size((float)((wide ? 560 : 250) * s), (float)(270 * s));
	ImGui::SetNextWindowPos(ImVec2((float)mw.x, (float)mw.y), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(def_size, ImGuiCond_FirstUseEver);
	bool pinned = MiniShellConstraints(mw, def_size);
	if (mw.want_raise) {
		ImGui::SetNextWindowFocus();
		mw.want_raise = false;
	}
	bool open = true;
	if (!ImGui::Begin(wid.c_str(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | (pinned ? ImGuiWindowFlags_NoResize : 0))) {
		ImGui::End();
		return open;
	}
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
		_front_wnd_veh = mw.kind == MiniWndKind::Vehicle ? mw.veh : VehicleID::Invalid();
		mw.focus_seq = ++_wnd_focus_tick;
	}
	RecordShellRect(mw);

	ImWndTitle(mw, title, WndRenamable(mw, v, st, t), open, v, st, t);

	_imrow = 0;
	int ntab;
	std::string tl[8];
	switch (mw.kind) {
		case MiniWndKind::Fleet:
		case MiniWndKind::Group: {
			static const StringID type_strs[] = {STR_REPLACE_VEHICLE_TRAIN, STR_REPLACE_VEHICLE_ROAD_VEHICLE, STR_REPLACE_VEHICLE_SHIP, STR_REPLACE_VEHICLE_AIRCRAFT};
			ntab = 4;
			for (int ti = 0; ti < 4; ti++) tl[ti] = GameText(type_strs[ti]);
			break;
		}
		case MiniWndKind::Company:
			ntab = 2;
			tl[0] = "개요";
			tl[1] = "자산";
			break;
		case MiniWndKind::Vehicle:
			ntab = 4;
			tl[0] = "상태";
			tl[1] = GameText(STR_VEHICLE_DETAIL_TAB_CARGO);
			tl[2] = "주문";
			tl[3] = GameText(STR_VEHICLE_DETAIL_TAB_INFORMATION);
			break;
		case MiniWndKind::Station:
			ntab = 4;
			tl[0] = "상태";
			tl[1] = GameText(STR_SMALLMAP_TYPE_INDUSTRIES);
			tl[2] = GameText(STR_SMALLMAP_TYPE_VEHICLES);
			tl[3] = GameText(STR_VEHICLE_DETAIL_TAB_INFORMATION);
			break;
		case MiniWndKind::Town:
			ntab = 4;
			tl[0] = "상태";
			tl[1] = "당국";
			tl[2] = GameText(STR_VEHICLE_DETAIL_TAB_INFORMATION);
			tl[3] = "화물";
			break;
		case MiniWndKind::IndustryList:
			ntab = 4;
			tl[0] = "이름";
			tl[1] = "생산";
			tl[2] = "수송";
			tl[3] = "연쇄";
			break;
		case MiniWndKind::League:
			ntab = 2;
			tl[0] = "순위";
			tl[1] = "상세";
			break;
		case MiniWndKind::Graph:
			ntab = 6;
			tl[0] = "영업 이익";
			tl[1] = "수입";
			tl[2] = "가치";
			tl[3] = "성능";
			tl[4] = "화물";
			tl[5] = "지급률";
			break;
		case MiniWndKind::Map:
			ntab = 7;
			tl[0] = GameText(STR_SMALLMAP_TYPE_CONTOURS);
			tl[1] = GameText(STR_SMALLMAP_TYPE_VEHICLES);
			tl[2] = GameText(STR_SMALLMAP_TYPE_INDUSTRIES);
			tl[3] = GameText(STR_SMALLMAP_TYPE_ROUTES);
			tl[4] = GameText(STR_SMALLMAP_TYPE_ROUTEMAP);
			tl[5] = GameText(STR_SMALLMAP_TYPE_VEGETATION);
			tl[6] = GameText(STR_SMALLMAP_TYPE_OWNERS);
			break;
		default:
			ntab = 4;
			tl[0] = "상태";
			tl[1] = "역";
			tl[2] = GameText(STR_VEHICLE_DETAIL_TAB_INFORMATION);
			tl[3] = "생산";
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
				DockSpec spec;
				WindowNumber wnum;
				bool embed = WndEmbedTarget(mw, spec, wnum);
				/* A slot fills the body exactly; a scrollbar appearing on the
				 * rounding would shrink it and oscillate. */
				_imwnd_outer = ImGui::GetWindowSize();
				ImGui::BeginChild("body", ImVec2(0.0f, -cmd_h), ImGuiChildFlags_None,
						embed ? (ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse) : 0);
				if (ti == 0 && has_view && (v != nullptr || st != nullptr || t != nullptr || ind != nullptr)) {
					ImWndViewSlot(mw);
				}
				if (embed) {
					ImWndNativeSlot(mw, spec, wnum);
				} else switch (mw.kind) {
					case MiniWndKind::Vehicle: if (v != nullptr) ImVehicleBody(mw, v); break;
					case MiniWndKind::Station: if (st != nullptr) ImStationBody(mw, st); break;
					case MiniWndKind::Town: if (t != nullptr) ImTownBody(mw, t); break;
					case MiniWndKind::Industry: if (ind != nullptr) ImIndustryBody(mw, ind); break;
					case MiniWndKind::Fleet: ImFleetBody(mw); break;
					case MiniWndKind::Company: ImCompanyBody(mw); break;
					case MiniWndKind::Group: ImGroupBody(mw); break;
					case MiniWndKind::IndustryList: ImIndustryListBody(mw); break;
					case MiniWndKind::League:
					case MiniWndKind::Graph: break;
					case MiniWndKind::Map: ImMapBody(mw); break;
					case MiniWndKind::Native: break;
				}
				ImGui::EndChild();
				if (has_cmds) ImWndCommands(mw, v, st, t, ind);
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}
	mw.want_tab = -1;

	ClampShellToScreen(mw);
	ImGui::End();
	return open;
}

static std::vector<NativeKey> WantedNativeOrder()
{
	std::vector<std::pair<uint32_t, size_t>> order;
	for (size_t i = 0; i < _wnds.size(); i++) order.emplace_back(_wnds[i].focus_seq, i);
	std::sort(order.begin(), order.end());

	std::vector<NativeKey> want;
	for (const auto &[seq, i] : order) {
		const MiniWnd &mw = _wnds[i];
		WindowNumber cnum = MiniCarrierNum(mw);
		if (FindCarrier(cnum) != nullptr) want.push_back({WC_EXTRA_VIEWPORT, cnum});
		DockSpec spec;
		WindowNumber num;
		if (WndEmbedTarget(mw, spec, num) && FindWindowById(spec.wc, num) != nullptr) want.push_back({spec.wc, num});
	}

	std::vector<NativeKey> held;
	_views.ListNatives(held);
	for (const NativeKey &key : held) {
		if (FindWindowById(key.wc, key.num) != nullptr) want.push_back(key);
	}
	return want;
}

/* Two shells that overlap sample each other's pixels out of the shared screen
 * buffer, so a new one is dropped where it covers none of the others. */
static void PlaceNativeShell(MiniWnd &mw, int cw, int ch)
{
	int gap = 6 * _tuning.hud_scale;
	int right = std::max(0, _fbw - cw - gap);
	int top = WindowBarBottom() + gap;

	mw.x = right;
	mw.y = top;
	for (int x = right; x >= 0; x -= cw + gap) {
		int y = top;
		bool moved = true;
		while (moved) {
			moved = false;
			for (const MiniWnd &o : _wnds) {
				if (o.shell.right <= o.shell.left) continue;
				if (x <= o.shell.right && o.shell.left <= x + cw && y <= o.shell.bottom && o.shell.top <= y + ch) {
					y = o.shell.bottom + 1 + gap;
					moved = true;
				}
			}
		}
		if (y + ch <= _fbh) {
			mw.x = std::max(0, x);
			mw.y = y;
			break;
		}
	}

	/* The guessed rect stands in until the shell first draws itself, so shells
	 * adopted in the same frame do not all land on the same spot. */
	mw.shell = {mw.x, mw.y, mw.x + cw - 1, mw.y + ch - 1};
}

/* Every standalone native window on screen gets a mini shell, so nothing is
 * left wearing the official frame while the mini UI is up. */
static void AdoptNativeWnds()
{
	static std::vector<std::pair<WindowClass, int32_t>> taken;
	taken.clear();
	for (const MiniWnd &mw : _wnds) {
		DockSpec spec;
		WindowNumber num;
		if (WndEmbedTarget(mw, spec, num)) taken.emplace_back(spec.wc, (int32_t)num);
	}

	int s = _tuning.hud_scale;
	for (Window *w : Window::Iterate()) {
		if (!NativeWrappable(w)) continue;
		/* A shell closed this frame leaves its window slotted until the sweep
		 * runs; adopting it here would rebuild the shell the player dismissed. */
		if (_dock.Find(w) != nullptr) continue;
		bool held = false;
		for (const auto &t : taken) {
			if (t.first == w->window_class && t.second == (int32_t)w->window_number) {
				held = true;
				break;
			}
		}
		if (held) continue;

		MiniWnd mw;
		mw.kind = MiniWndKind::Native;
		mw.nat_wc = w->window_class;
		mw.nat_num = w->window_number;
		int chrome_x = 2 * (int)ImGui::GetStyle().WindowPadding.x + 2;
		int chrome_y = 3 * (int)ImGui::GetStyle().WindowPadding.y + GetCharacterHeight(FS_NORMAL) + 8 * s;
		PlaceNativeShell(mw, w->width + chrome_x, w->height - CaptionCrop(w) + chrome_y);
		mw.want_raise = true;
		_wnds.push_back(mw);
		taken.emplace_back(mw.nat_wc, mw.nat_num);
	}
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
			case MiniWndKind::Company: alive = Company::IsValidID(_local_company); break;
			case MiniWndKind::Group: alive = Company::IsValidID(_local_company); break;
			case MiniWndKind::Native: alive = FindWindowById(_wnds[i].nat_wc, _wnds[i].nat_num) != nullptr; break;
			default: alive = true; break;
		}
		if (!alive) CloseMiniWnd(i);
	}

	AdoptNativeWnds();

	/* Mark before drawing: a shell whose Begin is skipped still owns its
	 * window, and releasing it here would re-adopt it the very next frame. */
	for (const MiniWnd &mw : _wnds) {
		DockSpec spec;
		WindowNumber num;
		if (WndEmbedTarget(mw, spec, num)) _dock.Mark({spec.wc, num});
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
	for (const MiniOpenReq &r : opens) OpenMiniWnd(r.kind, r.veh, r.st, r.town, r.ind, r.tab);
}

bool MiniUiCatchEstimate(Money cost)
{
	return CommandProbe::Catch(cost);
}

bool MiniUiShowError(std::string summary, std::string detail, bool warn)
{
	/* A probe asks what a plan would cost; a refusal is the answer, not news. */
	if (CommandProbe::Open()) return true;
	if (!_mini_active || summary.empty()) return false;

	_toast_feed.Report(std::move(summary), std::move(detail), warn);
	return true;
}

/* The newspaper is replaced by a toast; the item itself stays in the news
 * history either way. */
bool MiniUiShowNews(const NewsItem *ni)
{
	if (!_mini_active || ni == nullptr) return false;

	std::string headline = StrMakeValid(ni->headline.GetDecodedString(), {});
	if (headline.empty()) return true;

	_toast_feed.Announce(std::move(headline), GameText(STR_JUST_DATE_TINY, ni->date), ni->type == NewsType::Advice, ni->ref1);
	return true;
}

bool ShowMiniEnginePreview(EngineID engine)
{
	if (!_mini_active) return false;
	_views.Show(std::make_unique<EnginePreviewPanel>(engine));
	return true;
}

bool ShowMiniBuyCompany(CompanyID company, bool hostile_takeover)
{
	if (!_mini_active) return false;
	_views.Show(std::make_unique<TakeoverPanel>(company, hostile_takeover));
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
	_views.Show(std::make_unique<WaypointPanel>(waypoint));
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
	_map_labels.Paint(_camera.TilePixels());
	_dock.Unmark();
	DrawMiniWndsImGui();
	_views.Frame(_fbw, _fbh, (float)_tuning.hud_scale, WindowBarBottom());
	/* An embed whose slot went away this frame has nothing left to draw into.
	 * A window the mini UI opened goes with it; an adopted one is handed back. */
	_dock.Sweep();
	_dock.Stack(WantedNativeOrder());
	VideoDriver::GetInstance()->MakeDirty(0, 0, _fbw, _fbh);
}

static void Deactivate()
{
	CloseAllMiniWnds();
	_mini_active = false;
	_mode.Idle();
	_overlay.Reset();
	_build_shelf.Close();
	_window_shelf.Close();
	_tool.Abort();
	_camera.Halt();
	_vehicle_motion.Clear();
	MarkWholeScreenDirty();
}

/* A new or loaded world reuses pool IDs, so state naming entities from the old world must not survive the switch. */
void MiniUiResetGameState()
{
	CloseAllMiniWnds();
	_mode.Idle();
	_tool.Abort();
	_vehicle_motion.Clear();
	_status_board.Clear();
	ClearFleetDrafts();
	_deploy.Reset();
	_toast_feed.Clear();
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

	_tuning.Load();
	MiniAtlasReload();
	_views.ReloadDesign();
	UndrawMouseCursor();
	/* One palette-driven fill resets the 32bpp-anim mapping buffer, so later
	 * direct framebuffer writes are not overwritten by palette animation. */
	{
		AutoRestoreBackup dpi_backup(_cur_dpi, &_screen);
		GfxFillRect(0, 0, _screen.width - 1, _screen.height - 1, PC_BLACK);
	}

	if (Window *w = GetMainWindow(); w != nullptr && w->viewport != nullptr) {
		Point centre = InverseRemapCoords(w->viewport->virtual_left + w->viewport->virtual_width / 2, w->viewport->virtual_top + w->viewport->virtual_height / 2);
		_camera.CentreOn(centre.x / (double)TILE_SIZE, centre.y / (double)TILE_SIZE);
	} else {
		_camera.CentreOn(Map::SizeX() / 2.0, Map::SizeY() / 2.0);
	}
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
	int y0 = WindowBarBottom() + 1;
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
		/* Carriers and embeds stay below the ImGui layer; their pixels surface
		 * through the slot image. An embed is never blitted on its own: the
		 * part of it that reaches past its slot would show through the chrome. */
		bool under = _dock.Docks(w);
		rects.push_back({w->left, w->top, w->width, w->height, under, under});
	}
}

void MiniUiScrollTo(int x, int y)
{
	_mode.Unfollow();
	if (!_mini_active) return;
	_camera.GlideTo(x / (double)TILE_SIZE, y / (double)TILE_SIZE);
}

/* Native windows float above the ImGui layer and take the click; carriers
 * sit below it, so ImGui gets those instead. An embed takes the click only
 * inside its visible slot, so the cropped caption hiding under the mini tab
 * strip cannot start a native drag, and only where no panel covers the slot. */
static bool NativeWindowTakesPointer()
{
	Window *w = FindWindowFromPt(_cursor.pos.x, _cursor.pos.y);
	if (w == nullptr || MiniUiHidesWindow(w->window_class) || IsCarrier(w)) return false;

	const DockedWindow *e = _dock.Find(w);
	if (e == nullptr) return true;
	if (!e->vis.Contains({_cursor.pos.x, _cursor.pos.y})) return false;
	bool on_grip = e->grip > 0 && _cursor.pos.x > e->vis.right - e->grip && _cursor.pos.y > e->vis.bottom - e->grip;
	return !on_grip && (!_views.PointerOverLayer() || _views.PointerOverSlot());
}

static bool MiniLayerTakesPointer()
{
	return _views.CapturePointer() || (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse);
}

bool MiniUiHandleMouseEvents(bool native_capture)
{
	if (!_mini_active) return false;

	PressSide held = _press_owner.Held();
	_views.TrackPointer();

	if (held == PressSide::Native) return false;
	if (held == PressSide::Mini) {
		_cursor.wheel = 0;
		return true;
	}

	if (held == PressSide::None && !_tool.Dragging() && !_middle_button_down) {
		if (native_capture) return false;
		if (NativeWindowTakesPointer()) {
			_press_owner.Claim(PressSide::Native);
			return false;
		}
		/* The panels and ImGui have the wheel already. Leaving the pending
		 * notch here would zoom the map the moment the cursor leaves the
		 * window and the map starts consuming events again. */
		if (MiniLayerTakesPointer()) {
			_cursor.wheel = 0;
			_press_owner.Claim(PressSide::Mini);
			return true;
		}
	}

	_press_owner.Claim(PressSide::Map);

	if (_middle_button_down && (_cursor.delta.x != 0 || _cursor.delta.y != 0)) _camera.Drag(_cursor.delta.x, _cursor.delta.y);

	if (_cursor.wheel != 0) {
		if (_mode.Following()) {
			/* While following, zooming keeps the vehicle centred instead of
			 * anchoring the cursor point. */
			_camera.Zoom(_cursor.wheel < 0);
		} else {
			_camera.ZoomAt(_cursor.pos.x, _cursor.pos.y, _cursor.wheel < 0);
		}
		_cursor.wheel = 0;
	}

	if (_left_button_down && !_left_button_clicked) {
		_left_button_clicked = true;
		if (_tool.Kind() == MiniTool::None) {
			if (_mode.PickingOrders()) {
				_mode.PickOrderAt(CursorPoint());
			} else if (!HandleLabelClick(_cursor.pos.x, _cursor.pos.y) && !OpenVehicleWndAt(_cursor.pos.x, _cursor.pos.y) &&
					!OpenDepotWndAt(_cursor.pos.x, _cursor.pos.y)) {
				OpenWaypointWndAt(_cursor.pos.x, _cursor.pos.y);
			}
		} else {
			_tool.Press(CursorPoint(), _ctrl_pressed);
		}
	}

	if (!_left_button_down && _prev_left) _tool.Release();
	_prev_left = _left_button_down;

	if (_right_button_clicked) {
		_right_button_clicked = false;
		_mode.Unwind();
	}

	_cursor.delta.x = 0;
	_cursor.delta.y = 0;
	_cursor.wheel_moved = false;
	return true;
}

/* Escape only unwinds mini UI state, one layer per press; leaving the mini UI is F9 alone. */
static void UnwindEscape()
{
	if (_mode.Unwind()) return;
	if (_views.CloseFront()) return;
	if (!_wnds.empty()) {
		/* Windows stack by focus, so the one on top is the one the
		 * player last worked in, not the one opened last. */
		size_t top = _wnds.size() - 1;
		for (size_t i = 0; i < _wnds.size(); i++) {
			if (_wnds[i].focus_seq > _wnds[top].focus_seq) top = i;
		}
		CloseMiniWnd(top);
		return;
	}
	_build_shelf.Close();
	_window_shelf.Close();
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
	if (kc != WKC_F9 && _views.ProcessKey(keycode)) return true;
	if (kc != WKC_F9 && ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput) return true;

	/* A native edit box holding the game focus owns the keyboard too, or a
	 * wrapped popup could never be typed into. */
	if (kc != WKC_F9 && EditBoxInGlobalFocus()) return false;

	switch (kc) {
		case WKC_F9:
			Deactivate();
			break;

		case WKC_ESC:
			UnwindEscape();
			break;

		/* The pair turns the blueprint and nothing else: every type choice
		 * belongs to the build panel. It is modal, not a global shortcut, so
		 * it only lives while a placement tool is in hand. */
		case 'E':
			_choices.Turn(DIAGDIRDIFF_90RIGHT);
			break;

		case 'Q':
			_choices.Turn(DIAGDIRDIFF_90LEFT);
			break;

		default:
			break;
	}
	return true;
}

bool MiniUiTyping()
{
	return _mini_active && _views.IsTyping();
}

bool MiniUiHandleTextInput(char32_t character)
{
	return _mini_active && _views.ProcessText(character);
}

void MiniUiFrame(uint delta_ms)
{
	/* Entering a game activates the mini UI unless the config opts out. */
	static GameMode last_mode = GM_MENU;
	if (_game_mode != last_mode) {
		last_mode = _game_mode;
		if (!_mini_active && !_network_dedicated && (_game_mode == GM_NORMAL || _game_mode == GM_EDITOR)) {
			_tuning.Load();
			if (_tuning.start_active != 0) MiniUiToggle();
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
	_camera.SetViewport(_fbw, _fbh);

	_canvas.BeginFrame();
	MiniAtlasEnsure();
	_vehicle_motion.Advance(delta_ms);
	_toast_feed.Age(delta_ms);
	if (VehicleID built = _deploy.Step(); built != VehicleID::Invalid()) OpenVehicleWindow(built);
	RlwCmdClear();
	MiniImGuiEnsureSetup();
	RlwImGuiNewFrame();
	if (_tuning.imgui_demo != 0) ImGui::ShowDemoWindow();

	_camera.Update(delta_ms, _mode.FollowTarget());

	int ppt = _camera.TilePixels();

	_tool.Follow(CursorPoint());
	_estimate.Update();
	_sites.Update();

	_overlay.FollowTool(ToolLayer(_tool.Kind()));

	_map_painter.Paint(ppt, _overlay.Filter());

	PaintBlueprint(ppt);

	DrawOrderRoute();
	_vehicle_painter.Paint(ppt, _overlay.Filter());
	DrawVehicleRing(ppt);
	Present();
}
