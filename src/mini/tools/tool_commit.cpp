/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tool_commit.cpp The game commands the build tools post. */

#include "../../stdafx.h"
#include "tool_commit.h"

#include "../../command_func.h"
#include "../../core/random_func.hpp"
#include "../../industry_cmd.h"
#include "../../industrytype.h"
#include "../../landscape_cmd.h"
#include "../../map_func.h"
#include "../../newgrf_roadstop.h"
#include "../../newgrf_station.h"
#include "../../object_cmd.h"
#include "../../object_type.h"
#include "../../rail_cmd.h"
#include "../../road_cmd.h"
#include "../../settings_type.h"
#include "../../signs_base.h"
#include "../../signs_cmd.h"
#include "../../station_cmd.h"
#include "../../terraform_cmd.h"
#include "../../timer/timer_game_calendar.h"
#include "../../tree_cmd.h"
#include "../../tree_map.h"
#include "../../tunnelbridge_cmd.h"
#include "../../water_cmd.h"
#include "../../waypoint_cmd.h"
#include "../../waypoint_func.h"
#include "build_tool.h"
#include "clear_filter.h"
#include "tile_pick.h"
#include "tool_choices.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static constexpr uint8_t RAW_INDUSTRY_PROSPECTING = 2;

static uint8_t TransportSubtype(TransportType transport)
{
	return transport == TRANSPORT_RAIL ? static_cast<uint8_t>(_choices.Rail()) : static_cast<uint8_t>(_choices.Road());
}

static RoadStopType StopTypeOf(MiniTool kind)
{
	return kind == MiniTool::BusStop ? RoadStopType::Bus : RoadStopType::Truck;
}

void PostRemoveRoadStop(TileIndex tile, RoadStopType type)
{
	Command<CMD_REMOVE_ROAD_STOP>::Post(type == RoadStopType::Bus ? STR_ERROR_CAN_T_REMOVE_BUS_STATION : STR_ERROR_CAN_T_REMOVE_TRUCK_STATION, tile, 1, 1, type, false);
}

static void PostBridge(TileIndex from, TileIndex to, TransportType transport)
{
	uint len = DistanceManhattan(from, to) - 1;
	Command<CMD_BUILD_BRIDGE>::Post(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE, to, from, transport, _choices.Bridge(len), TransportSubtype(transport));
}

void PostRun(const RailPlan &plan, const PlanRun &run, bool remove)
{
	TileIndex from = plan.pieces[run.a].first;
	TileIndex to = plan.pieces[run.b].first;
	Track track = plan.pieces[run.a].second;
	if (remove) {
		Command<CMD_REMOVE_RAILROAD_TRACK>::Post(STR_ERROR_CAN_T_REMOVE_RAILROAD_TRACK, to, from, track);
	} else if (run.bridge) {
		PostBridge(from, to, TRANSPORT_RAIL);
	} else {
		Command<CMD_BUILD_RAILROAD_TRACK>::Post(STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK, to, from, _choices.Rail(), track, true, false);
	}
}

void PostRun(const LinePlan &plan, const PlanRun &run, bool remove)
{
	TileIndex from = plan.tiles[run.a];
	TileIndex to = plan.tiles[run.b];
	if (remove) {
		Command<CMD_REMOVE_LONG_ROAD>::Post(STR_ERROR_CAN_T_REMOVE_ROAD_FROM, to, from, _choices.Road(), plan.axis, false, false);
	} else if (run.bridge) {
		PostBridge(from, to, TRANSPORT_ROAD);
	} else {
		Command<CMD_BUILD_LONG_ROAD>::Post(STR_ERROR_CAN_T_BUILD_ROAD_HERE, to, from, _choices.Road(), plan.axis, DRD_NONE, false, false, false);
	}
}

void PostArea(MiniTool kind, TileIndex origin, TileIndex far, bool remove)
{
	if (remove || kind == MiniTool::Demolish) {
		Command<CMD_CLEAR_AREA>::Post(STR_ERROR_CAN_T_CLEAR_THIS_AREA, far, origin, false);
		return;
	}

	switch (kind) {
		case MiniTool::Canal: Command<CMD_BUILD_CANAL>::Post(STR_ERROR_CAN_T_BUILD_CANALS, far, origin, WaterClass::Canal, false); break;
		case MiniTool::Trees: Command<CMD_PLANT_TREE>::Post(STR_ERROR_CAN_T_PLANT_TREE_HERE, far, origin, TREE_INVALID, false); break;
		case MiniTool::BuyLand: Command<CMD_BUILD_OBJECT_AREA>::Post(STR_ERROR_CAN_T_PURCHASE_THIS_LAND, far, origin, OBJECT_OWNED_LAND, 0, false); break;
		case MiniTool::Convert: Command<CMD_CONVERT_RAIL>::Post(STR_ERROR_CAN_T_CONVERT_RAIL, far, origin, _choices.Rail(), false); break;
		case MiniTool::RoadConvert: Command<CMD_CONVERT_ROAD>::Post(STR_ERROR_CAN_T_CONVERT_ROAD, far, origin, _choices.Road(), false); break;
		default: break;
	}
}

static void FundIndustry(TileIndex tile)
{
	IndustryType it = _choices.Industry();
	if (it == IT_INVALID) return;

	const IndustrySpec *indsp = GetIndustrySpec(it);
	/* Raw industries are prospected under the funding setting that hides
	 * their location, so the click only pays for the search. */
	bool prospect = _settings_game.construction.raw_industry_construction == RAW_INDUSTRY_PROSPECTING && indsp->IsRawIndustry();
	uint32_t seed = InteractiveRandom();
	uint32_t layout = InteractiveRandomRange((uint32_t)indsp->layouts.size());
	Command<CMD_BUILD_INDUSTRY>::Post(STR_ERROR_CAN_T_CONSTRUCT_THIS_INDUSTRY, prospect ? TileIndex{} : tile, it, prospect ? 0 : layout, false, seed);
}

static void BuildAt(MiniTool kind, TileIndex tile)
{
	switch (kind) {
		case MiniTool::BusStop:
		case MiniTool::TruckStop: {
			StringID error = kind == MiniTool::BusStop ? STR_ERROR_CAN_T_BUILD_BUS_STATION : STR_ERROR_CAN_T_BUILD_TRUCK_STATION;
			Command<CMD_BUILD_ROAD_STOP>::Post(error, tile, 1, 1, StopTypeOf(kind), _choices.StopThrough(), _choices.StopFacing(), _choices.Road(), ROADSTOP_CLASS_DFLT, 0, StationID::Invalid(), false);
			break;
		}

		/* The waypoint must follow the track under it, so the axis comes from
		 * the tile rather than the tool rotation. */
		case MiniTool::RailWaypoint: {
			Axis axis = GetAxisForNewRailWaypoint(tile);
			Command<CMD_BUILD_RAIL_WAYPOINT>::Post(STR_ERROR_CAN_T_BUILD_RAIL_WAYPOINT, tile, IsValidAxis(axis) ? axis : AXIS_X, 1, 1, STAT_CLASS_WAYP, 0, StationID::Invalid(), false);
			break;
		}

		case MiniTool::RoadWaypoint: {
			Axis axis = GetAxisForNewRoadWaypoint(tile);
			Command<CMD_BUILD_ROAD_WAYPOINT>::Post(STR_ERROR_CAN_T_BUILD_ROAD_WAYPOINT, tile, IsValidAxis(axis) ? axis : AXIS_X, 1, 1, ROADSTOP_CLASS_WAYP, 0, StationID::Invalid(), false);
			break;
		}

		/* A fresh sign carries a placeholder so it is visible at once; the sign
		 * list is where it gets its real name. */
		case MiniTool::Sign: Command<CMD_PLACE_SIGN>::Post(STR_ERROR_CAN_T_PLACE_SIGN_HERE, tile, std::string("표지판")); break;
		case MiniTool::TrainDepot: Command<CMD_BUILD_TRAIN_DEPOT>::Post(STR_ERROR_CAN_T_BUILD_TRAIN_DEPOT, tile, _choices.Rail(), _choices.PointFacing()); break;
		case MiniTool::RoadDepot: Command<CMD_BUILD_ROAD_DEPOT>::Post(STR_ERROR_CAN_T_BUILD_ROAD_DEPOT, tile, _choices.Road(), _choices.PointFacing()); break;
		case MiniTool::ShipDepot: Command<CMD_BUILD_SHIP_DEPOT>::Post(STR_ERROR_CAN_T_BUILD_SHIP_DEPOT, tile, DiagDirToAxis(_choices.PointFacing())); break;
		case MiniTool::Dock: Command<CMD_BUILD_DOCK>::Post(STR_ERROR_CAN_T_BUILD_DOCK_HERE, tile, StationID::Invalid(), false); break;
		case MiniTool::Buoy: Command<CMD_BUILD_BUOY>::Post(STR_ERROR_CAN_T_POSITION_BUOY_HERE, tile); break;
		case MiniTool::Airport: Command<CMD_BUILD_AIRPORT>::Post(STR_ERROR_CAN_T_BUILD_AIRPORT_HERE, tile, _choices.Airport(), 0, StationID::Invalid(), false); break;
		case MiniTool::Lock: Command<CMD_BUILD_LOCK>::Post(STR_ERROR_CAN_T_BUILD_LOCKS, tile); break;
		case MiniTool::Headquarters: Command<CMD_BUILD_OBJECT>::Post(STR_ERROR_CAN_T_BUILD_COMPANY_HEADQUARTERS, tile, OBJECT_HQ, 0); break;
		case MiniTool::Industry: FundIndustry(tile); break;

		case MiniTool::RailTunnel:
		case MiniTool::RoadTunnel: {
			TransportType transport = kind == MiniTool::RailTunnel ? TRANSPORT_RAIL : TRANSPORT_ROAD;
			Command<CMD_BUILD_TUNNEL>::Post(STR_ERROR_CAN_T_BUILD_TUNNEL_HERE, tile, transport, TransportSubtype(transport));
			break;
		}

		default:
			break;
	}
}

static void RemoveAt(MiniTool kind, TileIndex tile)
{
	switch (kind) {
		case MiniTool::BusStop:
		case MiniTool::TruckStop:
			PostRemoveRoadStop(tile, StopTypeOf(kind));
			return;

		case MiniTool::RailWaypoint:
			Command<CMD_REMOVE_FROM_RAIL_WAYPOINT>::Post(STR_ERROR_CAN_T_REMOVE_RAIL_WAYPOINT, tile, tile, true);
			return;

		case MiniTool::RoadWaypoint:
			Command<CMD_REMOVE_FROM_ROAD_WAYPOINT>::Post(STR_ERROR_CAN_T_REMOVE_ROAD_WAYPOINT, tile, tile);
			return;

		/* Renaming a sign to nothing removes it. */
		case MiniTool::Sign:
			for (const Sign *si : Sign::Iterate()) {
				if (TileVirtXY((uint)si->x, (uint)si->y) != tile) continue;
				Command<CMD_RENAME_SIGN>::Post(si->index, std::string{});
				return;
			}
			return;

		case MiniTool::Industry:
			if (_choices.Industry() == IT_INVALID) return;
			break;

		default:
			break;
	}

	Command<CMD_LANDSCAPE_CLEAR>::Post(STR_ERROR_CAN_T_CLEAR_THIS_AREA, tile);
}

/* Point tools place on click: the blueprint floats on the hover tile and
 * R turns its facing, so no drag gesture is involved. */
void CommitClick(MiniTool kind, TileIndex tile, bool remove)
{
	if (!IsClickTool(kind)) return;

	if (remove) {
		RemoveAt(kind, tile);
	} else {
		BuildAt(kind, tile);
	}
}

template <typename Plan>
static void PostRuns(const Plan &plan, bool remove)
{
	for (const PlanRun &run : plan.Runs(remove)) PostRun(plan, run, remove);
}

static void CommitBridge(MiniTool kind, const LinePlan &line)
{
	if (line.BridgeLength() == 0) return;
	PostBridge(line.tiles.front(), line.tiles.back(), kind == MiniTool::RailBridge ? TRANSPORT_RAIL : TRANSPORT_ROAD);
}

static void CommitSignals(const SignalPlan &plan, bool remove)
{
	if (plan.tiles.empty()) return;

	TileIndex start = plan.tiles.front();
	TileIndex end = plan.tiles.back();
	SignalVariant sigvar = TimerGameCalendar::year < _settings_client.gui.semaphore_build_before ? SIG_SEMAPHORE : SIG_ELECTRIC;
	if (plan.tiles.size() == 1) {
		if (remove) {
			Command<CMD_REMOVE_SINGLE_SIGNAL>::Post(STR_ERROR_CAN_T_REMOVE_SIGNALS_FROM, start, plan.track);
		} else {
			Command<CMD_BUILD_SINGLE_SIGNAL>::Post(STR_ERROR_CAN_T_BUILD_SIGNALS_HERE, start, plan.track, _choices.Signal(), sigvar, false, false, false, SIGTYPE_PBS, SIGTYPE_LAST, 0, 0);
		}
	} else if (remove) {
		Command<CMD_REMOVE_SIGNAL_TRACK>::Post(STR_ERROR_CAN_T_REMOVE_SIGNALS_FROM, start, end, plan.track, false);
	} else {
		Command<CMD_BUILD_SIGNAL_TRACK>::Post(STR_ERROR_CAN_T_BUILD_SIGNALS_HERE, start, end, plan.track, _choices.Signal(), sigvar,
				false, false, !_settings_client.gui.drag_signals_fixed_distance, _settings_client.gui.drag_signals_density);
	}
}

static void CommitStation(const AreaPlan &area, bool remove)
{
	if (remove) {
		Command<CMD_REMOVE_FROM_RAIL_STATION>::Post(STR_ERROR_CAN_T_REMOVE_PART_OF_STATION, area.Origin(), area.Far(), true);
		return;
	}

	Axis axis = _choices.StationAxis();
	uint8_t plat_len = (uint8_t)(axis == AXIS_X ? area.Width() : area.Height());
	uint8_t numtracks = (uint8_t)(axis == AXIS_X ? area.Height() : area.Width());
	Command<CMD_BUILD_RAIL_STATION>::Post(STR_ERROR_CAN_T_BUILD_RAILROAD_STATION, area.Origin(), _choices.Rail(), axis, numtracks, plat_len, STAT_CLASS_DFLT, 0, StationID::Invalid(), false);
}

static void CommitClear(const AreaPlan &area, bool remove)
{
	if (_clear_filter.TakesAll()) {
		PostArea(MiniTool::Demolish, area.Origin(), area.Far(), remove);
		return;
	}
	area.ForEach([](int tx, int ty) { _clear_filter.Post(TileXY(tx, ty)); });
}

/* Levelling copies the anchor tile's height, so the anchor corner is passed
 * as the reference tile rather than the normalised rectangle origin. */
static void CommitTerraform(const AreaPlan &area, TileIndex anchor, bool remove)
{
	int ax = TileX(anchor);
	int ay = TileY(anchor);
	TileIndex end = TileXY(area.x0 == ax ? area.x1 : area.x0, area.y0 == ay ? area.y1 : area.y0);
	LevelMode lm = remove ? LM_LOWER : (anchor == end ? LM_RAISE : LM_LEVEL);
	Command<CMD_LEVEL_LAND>::Post(STR_ERROR_CAN_T_LEVEL_LAND_HERE, end, anchor, false, lm);
}

void CommitDrag(const BuildTool &tool)
{
	MiniTool kind = tool.Kind();
	const ToolPlans &plans = tool.Plans();
	bool remove = tool.Removing();
	if (IsRectTool(kind) && !plans.area.valid) return;

	switch (kind) {
		case MiniTool::Rail: PostRuns(plans.rail, remove); break;
		case MiniTool::Road: PostRuns(plans.line, remove); break;
		case MiniTool::RailBridge:
		case MiniTool::RoadBridge: CommitBridge(kind, plans.line); break;
		case MiniTool::Signal: CommitSignals(plans.signal, remove); break;
		case MiniTool::Station: CommitStation(plans.area, remove); break;
		case MiniTool::Demolish: CommitClear(plans.area, remove); break;
		case MiniTool::Terraform: CommitTerraform(plans.area, SiteTileAt(tool.Anchor()), remove); break;

		case MiniTool::Canal:
		case MiniTool::Convert:
		case MiniTool::RoadConvert:
		case MiniTool::Trees:
		case MiniTool::BuyLand:
			PostArea(kind, plans.area.Origin(), plans.area.Far(), remove);
			break;

		default:
			break;
	}
}
