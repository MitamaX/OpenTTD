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
#include "mini/dock/native_dock.h"
#include "mini/dock/native_window.h"
#include "mini/fleet/consist_draft.h"
#include "mini/fleet/fleet_deploy.h"
#include "mini/gpu/frame_capture.h"
#include "mini/gpu/gpu_frame.h"
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
#include "mini/input/map_pointer.h"
#include "mini/input/pointer_router.h"
#include "mini/map/map_labels.h"
#include "mini/map/map_overlay.h"
#include "mini/map/map_painter.h"
#include "mini/map/vehicle_motion.h"
#include "mini/map/vehicle_painter.h"
#include "mini/map/volume_painter.h"
#include "mini/map/world_tiles.h"
#include "mini/tools/blueprint.h"
#include "mini/tools/build_tool.h"
#include "mini/tools/clear_filter.h"
#include "mini/tools/command_probe.h"
#include "mini/tools/tile_pick.h"
#include "mini/tools/tool_choices.h"
#include "mini/tools/tool_estimate.h"
#include "mini/tools/tool_sites.h"
#include "mini/ui/shader_painter.h"
#include "mini/ui/ui_text.h"
#include "mini/ui/view_host.h"
#include "mini/world/world_painter.h"
#include "mini/windows/company_panel.h"
#include "mini/windows/depot_panel.h"
#include "mini/windows/engine_preview_panel.h"
#include "mini/windows/finance_panel.h"
#include "mini/windows/goal_list_panel.h"
#include "mini/windows/graph_panel.h"
#include "mini/windows/group_panel.h"
#include "mini/windows/industry_list_panel.h"
#include "mini/windows/industry_panel.h"
#include "mini/windows/map_panel.h"
#include "mini/windows/native_panel.h"
#include "mini/windows/news_list_panel.h"
#include "mini/windows/sign_list_panel.h"
#include "mini/windows/station_list_panel.h"
#include "mini/windows/station_panel.h"
#include "mini/windows/subsidy_list_panel.h"
#include "mini/windows/takeover_panel.h"
#include "mini/windows/town_list_panel.h"
#include "mini/windows/town_panel.h"
#include "mini/windows/vehicle_panel.h"
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
#include "viewport_func.h"
#include "water_map.h"
#include "window_func.h"
#include "window_gui.h"

#include "table/strings.h"

#include "safeguards.h"

static bool _mini_active = false;

/* Mini UI frame size in pixels; drawing goes through the map draw list, so
 * this only mirrors the screen dimensions. */
static int _fbw, _fbh;

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
		Point at = _camera.ScreenOf(_vehicle_motion.Position(v));
		int dx = at.x - sx;
		int dy = at.y - sy;
		int d2 = dx * dx + dy * dy;
		if (d2 < best_d2) {
			best_d2 = d2;
			best = v;
		}
	}
	if (best != nullptr) ShowVehicleViewWindow(best->First());
	return best != nullptr;
}

/* A building drawn over the point answers first, so a click on a depot's roof opens the depot. */
static std::optional<TileIndex> InspectedTile(int sx, int sy)
{
	std::optional<TileIndex> picked = _volume_painter.PickAt({sx, sy});
	return picked.has_value() ? picked : TileUnder(_camera.MapAt(sx, sy));
}

static bool OpenDepotWndAt(int sx, int sy)
{
	std::optional<TileIndex> tile = InspectedTile(sx, sy);
	if (!tile.has_value() || !IsDepotTile(*tile)) return false;
	ShowDepotWindow(*tile, GetDepotVehicleType(*tile));
	return true;
}

/* Waypoints and buoys carry no map label, so the tile itself is the way into
 * their window. */
static bool OpenWaypointWndAt(int sx, int sy)
{
	std::optional<TileIndex> tile = InspectedTile(sx, sy);
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

	std::vector<Point> stops;
	int cur_stop = -1;
	int i = 0;
	for (const Order &o : v->Orders()) {
		if (o.IsType(OT_GOTO_STATION)) {
			const Station *st = Station::GetIfValid(o.GetDestination().ToStationID());
			if (st != nullptr) {
				if (i == v->cur_real_order_index) cur_stop = (int)stops.size();
				stops.push_back(_camera.ScreenOfGround(TileX(st->xy) + 0.5, TileY(st->xy) + 0.5));
			}
		}
		i++;
	}
	if (stops.empty()) return;

	size_t legs = stops.size() > 2 ? stops.size() : stops.size() - 1;
	for (size_t n = 0; n < legs; n++) {
		const Point &from = stops[n];
		const Point &to = stops[(n + 1) % stops.size()];
		_canvas.ThickLine(from.x, from.y, to.x, to.y, 2, COL_BP);
	}
	for (const Point &stop : stops) _canvas.FillCircle(stop.x, stop.y, 4, COL_BP);

	if (cur_stop >= 0) {
		Point at = _camera.ScreenOf(_vehicle_motion.Position(v));
		_canvas.ThickLine(at.x, at.y, stops[cur_stop].x, stops[cur_stop].y, 2, COL_PAPER);
	}
}

static void DrawVehicleRing(int ppt)
{
	const Vehicle *v = Vehicle::GetIfValid(FrontWndVehicle());
	if (v == nullptr) return;
	auto [cx, cy] = _camera.ScreenOf(_vehicle_motion.Position(v));
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

/* A plain click on the map opens what lies under it: a label first, then a vehicle, a depot or a waypoint. */
static void InspectAt(Point at)
{
	if (HandleLabelClick(at.x, at.y) || OpenVehicleWndAt(at.x, at.y) || OpenDepotWndAt(at.x, at.y)) return;
	OpenWaypointWndAt(at.x, at.y);
}

static void OpenMiniWindow(MiniWin win);

static std::vector<std::unique_ptr<HudPart>> CreateHudParts()
{
	std::vector<std::unique_ptr<HudPart>> parts;
	parts.push_back(std::make_unique<MapPointer>(InspectAt));
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

template <class TPanel>
static void ShowPanelOnTab(int tab)
{
	if (Panel *panel = _views.Show(std::make_unique<TPanel>()); panel != nullptr) panel->SelectTab(tab);
}

static void OpenMiniWindow(MiniWin win)
{
	bool company = Company::IsValidID(_local_company);
	switch (win) {
		case MiniWin::Finances: if (company) _views.Show(std::make_unique<FinancePanel>()); break;
		case MiniWin::CompanyInfo: if (company) OpenCompanyWindow(); break;
		case MiniWin::Goals: _views.Show(std::make_unique<GoalListPanel>()); break;
		case MiniWin::League: _views.Show(GraphPanel::League()); break;
		case MiniWin::Graph: _views.Show(GraphPanel::CompanyGraphs()); break;
		case MiniWin::Stations: if (company) _views.Show(std::make_unique<StationListPanel>()); break;
		case MiniWin::Trains: if (company) ShowPanelOnTab<GroupPanel>(VEH_TRAIN); break;
		case MiniWin::RoadVehicles: if (company) ShowPanelOnTab<GroupPanel>(VEH_ROAD); break;
		case MiniWin::Ships: if (company) ShowPanelOnTab<GroupPanel>(VEH_SHIP); break;
		case MiniWin::Aircraft: if (company) ShowPanelOnTab<GroupPanel>(VEH_AIRCRAFT); break;
		case MiniWin::News: _views.Show(std::make_unique<NewsListPanel>()); break;
		case MiniWin::Towns: _views.Show(std::make_unique<TownListPanel>()); break;
		case MiniWin::Industries: _views.Show(std::make_unique<IndustryListPanel>()); break;
		case MiniWin::Subsidies: _views.Show(std::make_unique<SubsidyListPanel>()); break;
		case MiniWin::Buy: if (company) _views.Show(std::make_unique<DepotPanel>()); break;
		case MiniWin::Groups: if (company) _views.Show(std::make_unique<GroupPanel>()); break;
		case MiniWin::Map: _views.Show(std::make_unique<MapPanel>()); break;
		case MiniWin::Signs: OpenSignListMiniWnd(SignID::Invalid()); break;
		case MiniWin::Save: ShowSaveLoadDialog(FT_SAVEGAME, SLO_SAVE); break;
		case MiniWin::Load: ShowSaveLoadDialog(FT_SAVEGAME, SLO_LOAD); break;
		case MiniWin::Options: ShowGameOptions(); break;
		case MiniWin::Music: ShowMusicWindow(); break;
		case MiniWin::Abandon: AskExitToGameMenu(); break;
		case MiniWin::Quit: AskExitGame(); break;
	}
}

/* The front panel decides whose order route shows. */
static VehicleID FrontWndVehicle()
{
	const VehiclePanel *panel = dynamic_cast<const VehiclePanel *>(_views.Front());
	return panel != nullptr ? panel->Subject() : VehicleID::Invalid();
}

static void CloseAllMiniWnds()
{
	_dock.CloseAll();
	_views.CloseAll();
}

void OpenVehicleWindow(VehicleID vehicle)
{
	_views.Show(std::make_unique<VehiclePanel>(vehicle));
}

void OpenStationWindow(StationID station)
{
	_views.Show(std::make_unique<StationPanel>(station));
}

void OpenTownWindow(TownID town)
{
	_views.Show(std::make_unique<TownPanel>(town));
}

void OpenIndustryWindow(IndustryID industry)
{
	_views.Show(std::make_unique<IndustryPanel>(industry));
}

void ScrollToTile(TileIndex tile)
{
	MiniUiScrollTo(TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE);
}

void OpenCompanyWindow()
{
	_views.Show(std::make_unique<CompanyPanel>());
}

/* Clicking a sign on the map opens the list already editing that sign, which
 * is the only place a sign can be named. */
static void OpenSignListMiniWnd(SignID focus)
{
	Panel *list = _views.Show(std::make_unique<SignListPanel>());
	const Sign *sign = Sign::GetIfValid(focus);
	if (list != nullptr && sign != nullptr) list->BeginEdit(SignListPanel::EditKey(focus), sign->name);
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

/* Native windows under panels must stack the way the panels do, or the
 * slot of the front panel would sample a native lying under another. */
static std::vector<NativeKey> WantedNativeOrder()
{
	std::vector<NativeKey> want;
	_views.ListNatives(want);
	std::erase_if(want, [](const NativeKey &key) { return FindWindowById(key.wc, key.num) == nullptr; });
	return want;
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
	OpenVehicleWindow(v->First()->index);
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
	OpenStationWindow(station);
	return true;
}

bool ShowMiniTownWindow(TownID town)
{
	if (!_mini_active || !Town::IsValidID(town)) return false;
	OpenTownWindow(town);
	return true;
}

bool ShowMiniIndustryWindow(IndustryID industry)
{
	if (!_mini_active || !Industry::IsValidID(industry)) return false;
	OpenIndustryWindow(industry);
	return true;
}

bool ShowMiniDepotWindow(TileIndex tile, VehicleType type)
{
	if (!_mini_active || !IsDepotTile(tile)) return false;
	ShowPanelOnTab<DepotPanel>(type);
	return true;
}

/* A docked window reaches the screen only through its panel slot; showing it
 * whole would show the part that reaches past the slot through the chrome. A
 * window that wears mini chrome is in a panel by the time the frame is drawn,
 * even when this frame is the one that adopts it. */
static bool Floats(const Window *w)
{
	return !MiniUiHidesWindow(w->window_class) && !_dock.Docks(w) && !NativeWrappable(w);
}

static std::vector<Rect> FloatingNativeRects()
{
	std::vector<Rect> rects;
	for (const Window *w : Window::IterateFromBack()) {
		if (Floats(w)) rects.push_back({w->left, w->top, w->left + w->width - 1, w->top + w->height - 1});
	}
	return rects;
}

static void Present()
{
	_map_labels.Paint(_camera.TilePixels());
	_dock.Unmark();
	NativePanel::AdoptAll(_views);
	_views.Frame(_fbw, _fbh, (float)_tuning.hud_scale, WindowBarBottom(), FloatingNativeRects());
	/* An embed whose slot went away this frame has nothing left to draw into.
	 * A window the mini UI opened goes with it; an adopted one is handed back. */
	_dock.Sweep();
	_dock.Stack(WantedNativeOrder());
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
	_world_tiles.Reset();
}

void MiniUiTileChanged(TileIndex tile)
{
	if (_mini_active) _world_tiles.Touch(tile);
}

void MiniUiToggle()
{
	if (_mini_active) {
		Deactivate();
		return;
	}
	if (_game_mode != GM_NORMAL && _game_mode != GM_EDITOR) return;
	if (BlitterFactory::GetCurrentBlitter()->GetScreenDepth() != 32) return;
	if (!_gpu.Available()) return;

	_tuning.Load();
	MiniAtlasReload();
	RegisterShaderPainter(WorldPainter::NAME, _world_painter);
	_world_painter.Reload();
	_world_tiles.Reset();
	_views.ReloadDesign();
	UndrawMouseCursor();
	/* One palette-driven fill resets the 32bpp-anim mapping buffer, so later
	 * direct framebuffer writes are not overwritten by palette animation. */
	{
		AutoRestoreBackup dpi_backup(_cur_dpi, &_screen);
		GfxFillRect(0, 0, _screen.width - 1, _screen.height - 1, PC_BLACK);
	}

	_camera.FaceNorth();
	if (Window *w = GetMainWindow(); w != nullptr && w->viewport != nullptr) {
		/* The native centre is a level-0 plane point; the mini map turns about the ground there. */
		Point centre = InverseRemapCoords(w->viewport->virtual_left + w->viewport->virtual_width / 2, w->viewport->virtual_top + w->viewport->virtual_height / 2);
		_camera.CentreOn(centre.x / (double)TILE_SIZE, centre.y / (double)TILE_SIZE);
	} else {
		_camera.CentreOn(Map::SizeX() / 2.0, Map::SizeY() / 2.0);
	}
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

/* The game's OpenGL back-end paints its own screen between these two; while the
 * mini UI is up that paint lands in a texture the frame composes from. */
bool MiniUiBeginPaint()
{
	return _gpu.BeginPaint(Dimension(_screen.width, _screen.height), _mini_active);
}

void MiniUiEndPaint()
{
	_gpu.Compose();
}

void MiniUiReleaseGraphics()
{
	_gpu.Release();
}

void MiniUiScrollTo(int x, int y)
{
	_mode.Unfollow();
	if (!_mini_active) return;
	_camera.GlideTo(x / (double)TILE_SIZE, y / (double)TILE_SIZE);
}

/* An event the mini UI handled is spent: a native window the pointer reaches
 * later must not find its click latch, wheel or motion still pending. */
static void SpendPointerEvent()
{
	if (_left_button_down) _left_button_clicked = true;
	_right_button_clicked = false;
	_cursor.wheel = 0;
	_cursor.v_wheel = 0.0f;
	_cursor.h_wheel = 0.0f;
	_cursor.wheel_moved = false;
	_cursor.delta.x = 0;
	_cursor.delta.y = 0;
}

bool MiniUiHandleMouseEvents(bool native_capture)
{
	if (!_mini_active) return false;

	PointerLayer under = native_capture ? PointerLayer::Native : _views.LayerAt(_cursor.pos.x, _cursor.pos.y);
	if (_pointer.Route(under) == PointerLayer::Native) {
		_views.LeavePointer(_left_button_down || _right_button_down || _middle_button_down);
		return false;
	}

	_views.FeedPointer();
	SpendPointerEvent();
	return true;
}

/* Escape only unwinds mini UI state, one layer per press; leaving the mini UI is F9 alone. */
static void UnwindEscape()
{
	if (_mode.Unwind()) return;
	if (_views.CloseFront()) return;
	_build_shelf.Close();
	_window_shelf.Close();
}

bool MiniUiHandleKeypress(uint keycode, char32_t key)
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
	if (kc != WKC_F9 && _views.ProcessKey(keycode, key)) return true;

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

		/* R turns the blueprint and nothing else, Shift turning it back: every
		 * type choice belongs to the build panel. It is modal, not a global
		 * shortcut, so it only lives while a placement tool is in hand. */
		case 'R':
			_choices.Turn((keycode & WKC_SHIFT) != 0 ? DIAGDIRDIFF_90LEFT : DIAGDIRDIFF_90RIGHT);
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

/* Any text field, the game's or the mini UI's, that takes typed characters. */
bool TextInputFocused()
{
	return EditBoxInGlobalFocus() || MiniUiTyping();
}

/* The held direction keys pan the map unless a text field takes them as typing. */
uint8_t MiniUiPanKeys()
{
	return TextInputFocused() ? 0 : _dirkeys;
}

/* WASD pans the mini map the way the arrows do. */
static constexpr std::pair<char32_t, uint8_t> PAN_LETTERS[] = {
	{'A', DIRKEY_LEFT},
	{'W', DIRKEY_UP},
	{'D', DIRKEY_RIGHT},
	{'S', DIRKEY_DOWN},
};

/* The _dirkeys bit a key pans with, or none for a key that does not pan. */
uint8_t MiniUiPanBit(char32_t key)
{
	for (const auto &[letter, bit] : PAN_LETTERS) {
		if (key == letter) return bit;
	}
	return 0;
}

/* Drivers that poll their keys ask after each pan letter; the letters only pan while the mini UI is up. */
uint8_t MiniUiHeldPanBits(const std::function<bool(char32_t key)> &held)
{
	uint8_t bits = 0;
	if (!_mini_active) return bits;
	for (const auto &[letter, bit] : PAN_LETTERS) {
		if (held(letter)) bits |= bit;
	}
	return bits;
}

/* Q and E spin the view for as long as they are held, Q one way and E the other. */
static constexpr std::pair<char32_t, int> TURN_LETTERS[] = {
	{'Q', 1},
	{'E', -1},
};

static std::array<bool, std::size(TURN_LETTERS)> _turn_held{};

void MiniUiHoldTurnKeys(const std::function<bool(char32_t key)> &held)
{
	for (size_t i = 0; i < std::size(TURN_LETTERS); i++) _turn_held[i] = _mini_active && held(TURN_LETTERS[i].first);
}

void MiniUiTrackTurnKey(char32_t key, bool down)
{
	for (size_t i = 0; i < std::size(TURN_LETTERS); i++) {
		if (TURN_LETTERS[i].first == key) _turn_held[i] = down && _mini_active;
	}
}

/* The way the held turn keys spin the view, unless a text field takes them as typing. */
int MiniUiTurnKeys()
{
	if (TextInputFocused()) return 0;
	int turn = 0;
	for (size_t i = 0; i < std::size(TURN_LETTERS); i++) {
		if (_turn_held[i]) turn += TURN_LETTERS[i].second;
	}
	return turn;
}

bool MiniUiHandleTextInput(std::string_view text, bool marked)
{
	return _mini_active && _views.ProcessText(text, marked);
}

void MiniUiFrame(uint delta_ms)
{
	/* Entering a game activates the mini UI unless the config opts out. */
	static GameMode last_mode = GM_MENU;
	if (_game_mode != last_mode) {
		last_mode = _game_mode;
		if (!_mini_active && !_network_dedicated && (_game_mode == GM_NORMAL || _game_mode == GM_EDITOR)) {
			_tuning.Load();
			if (_tuning.start_active != 0 || _frame_capture.Active()) MiniUiToggle();
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
	_world_tiles.Sync();
	_camera.SetPeak(_world_tiles.Peak());
	_vehicle_motion.Advance(delta_ms);
	_toast_feed.Age(delta_ms);
	if (VehicleID built = _deploy.Step(); built != VehicleID::Invalid()) OpenVehicleWindow(built);
	_map_draw.Clear();

	_camera.Update(delta_ms, _mode.FollowTarget());
	if (std::optional<ViewAim> aim = _frame_capture.Aim(); aim.has_value()) _camera.Aim(*aim);
	_volume_painter.BeginFrame();

	int ppt = _camera.TilePixels();

	_tool.Follow(CursorPoint());
	_estimate.Update();
	_sites.Update();

	_overlay.FollowTool(ToolLayer(_tool.Kind()));

	_map_painter.PaintGround(ppt, _overlay.Filter());
	_vehicle_painter.Paint(ppt, _overlay.Filter(), VehicleTier::Grounded);
	_map_painter.PaintRaised();

	PaintBlueprint(ppt);

	DrawOrderRoute();
	_vehicle_painter.Paint(ppt, _overlay.Filter(), VehicleTier::Aloft);
	DrawVehicleRing(ppt);
	Present();
}
