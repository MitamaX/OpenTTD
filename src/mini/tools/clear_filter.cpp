/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file clear_filter.cpp Which kind of tile the clear tool takes off its drag. */

#include "../../stdafx.h"
#include "clear_filter.h"

#include <algorithm>

#include "../../command_func.h"
#include "../../landscape_cmd.h"
#include "../../rail_cmd.h"
#include "../../rail_map.h"
#include "../../road_cmd.h"
#include "../../road_map.h"
#include "../../station_cmd.h"
#include "../../station_map.h"
#include "../../tile_map.h"
#include "../../track_func.h"
#include "../../tunnelbridge_map.h"
#include "../../water_map.h"
#include "../../waypoint_cmd.h"
#include "tool_commit.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static const MiniClearItem _clear_any_items[] = {
	{"전부", MiniTool::Demolish, MiniClear::All},
};

static const MiniClearItem _clear_rail_items[] = {
	{"선로", MiniTool::Rail, MiniClear::RailTrack},
	{"신호", MiniTool::Signal, MiniClear::Signal},
	{"역", MiniTool::Station, MiniClear::RailStation},
	{"차고", MiniTool::TrainDepot, MiniClear::RailDepot},
	{"경유지", MiniTool::RailWaypoint, MiniClear::RailWaypoint},
	{"터널·다리", MiniTool::RailBridge, MiniClear::RailTunnelBridge},
};

static const MiniClearItem _clear_road_items[] = {
	{"도로", MiniTool::Road, MiniClear::Road},
	{"정류장", MiniTool::BusStop, MiniClear::RoadStop},
	{"차고", MiniTool::RoadDepot, MiniClear::RoadDepot},
	{"경유지", MiniTool::RoadWaypoint, MiniClear::RoadWaypoint},
	{"터널·다리", MiniTool::RoadBridge, MiniClear::RoadTunnelBridge},
};

static const MiniClearItem _clear_water_items[] = {
	{"운하", MiniTool::Canal, MiniClear::Canal},
	{"부두", MiniTool::Dock, MiniClear::Dock},
	{"부표", MiniTool::Buoy, MiniClear::Buoy},
	{"조선소", MiniTool::ShipDepot, MiniClear::ShipDepot},
	{"수로교", MiniTool::Lock, MiniClear::Aqueduct},
};

static const MiniClearItem _clear_air_items[] = {
	{"공항", MiniTool::Airport, MiniClear::Airport},
};

static const MiniClearItem _clear_land_items[] = {
	{"나무", MiniTool::Trees, MiniClear::Tree},
	{"건물", MiniTool::Headquarters, MiniClear::House},
	{"산업", MiniTool::Industry, MiniClear::Industry},
	{"소유지", MiniTool::BuyLand, MiniClear::Object},
};

static const MiniClearCategory _clear_cats[] = {
	{"전체", MiniTool::Demolish, MiniClear::All, _clear_any_items},
	{"철도", MiniTool::Rail, MiniClear::RailAny, _clear_rail_items},
	{"도로", MiniTool::Road, MiniClear::RoadAny, _clear_road_items},
	{"수상", MiniTool::Canal, MiniClear::WaterAny, _clear_water_items},
	{"항공", MiniTool::Airport, MiniClear::AirAny, _clear_air_items},
	{"지형", MiniTool::Trees, MiniClear::LandAny, _clear_land_items},
};

ClearFilter _clear_filter;

std::span<const MiniClearCategory> ClearCategories()
{
	return _clear_cats;
}

struct ClearChoice {
	std::string_view label;
	std::span<const MiniClearItem> kinds;
};

static ClearChoice LookUp(MiniClear mode)
{
	for (const MiniClearCategory &cat : _clear_cats) {
		if (cat.mode == mode) return {cat.label, cat.items};
		for (const MiniClearItem &it : cat.items) {
			if (it.mode == mode) return {it.label, {&it, 1}};
		}
	}
	return {};
}

static TransportType TunnelBridgeTransport(TileIndex tile)
{
	return IsTileType(tile, MP_TUNNELBRIDGE) ? GetTunnelBridgeTransportType(tile) : INVALID_TRANSPORT;
}

static bool ClearKindMatches(TileIndex tile, MiniClear kind)
{
	switch (kind) {
		case MiniClear::All: return true;
		/* A level crossing carries both, so it answers to the rail filter and
		 * the road one alike. */
		case MiniClear::RailTrack: return IsPlainRailTile(tile) || IsLevelCrossingTile(tile);
		case MiniClear::Signal: return IsPlainRailTile(tile) && HasSignals(tile);
		case MiniClear::RailStation: return IsRailStationTile(tile);
		case MiniClear::RailDepot: return IsRailDepotTile(tile);
		case MiniClear::RailWaypoint: return IsRailWaypointTile(tile);
		case MiniClear::RailTunnelBridge: return TunnelBridgeTransport(tile) == TRANSPORT_RAIL;
		case MiniClear::Road: return IsNormalRoadTile(tile) || IsLevelCrossingTile(tile);
		case MiniClear::RoadStop: return IsStationRoadStopTile(tile);
		case MiniClear::RoadDepot: return IsRoadDepotTile(tile);
		case MiniClear::RoadWaypoint: return IsRoadWaypointTile(tile);
		case MiniClear::RoadTunnelBridge: return TunnelBridgeTransport(tile) == TRANSPORT_ROAD;
		case MiniClear::Canal: return IsTileType(tile, MP_WATER) && (IsCanal(tile) || IsLock(tile));
		case MiniClear::Dock: return IsDockTile(tile);
		case MiniClear::Buoy: return IsBuoyTile(tile);
		case MiniClear::ShipDepot: return IsShipDepotTile(tile);
		case MiniClear::Aqueduct: return TunnelBridgeTransport(tile) == TRANSPORT_WATER;
		case MiniClear::Airport: return IsAirportTile(tile);
		case MiniClear::Tree: return IsTileType(tile, MP_TREES);
		case MiniClear::House: return IsTileType(tile, MP_HOUSE);
		case MiniClear::Industry: return IsTileType(tile, MP_INDUSTRY);
		case MiniClear::Object: return IsTileType(tile, MP_OBJECT);
		default: return false;
	}
}

/* Takes the chosen kind off the tile and leaves the rest standing, so a
 * crossing keeps its road when the rails go. Answers whether the tile was
 * asked about at all, which is what tells a skipped tile from a refused one. */
static bool PostClearKind(TileIndex tile, MiniClear kind)
{
	if (!ClearKindMatches(tile, kind)) return false;

	switch (kind) {
		case MiniClear::RailTrack: {
			TrackBits bits = IsLevelCrossingTile(tile) ? GetCrossingRailBits(tile) : GetTrackBits(tile);
			bool asked = false;
			for (Track t : SetTrackBitIterator(bits)) {
				Command<CMD_REMOVE_RAILROAD_TRACK>::Post(STR_ERROR_CAN_T_REMOVE_RAILROAD_TRACK, tile, tile, t);
				asked = true;
			}
			return asked;
		}

		case MiniClear::Signal: {
			bool asked = false;
			for (Track t : SetTrackBitIterator(GetTrackBits(tile))) {
				if (!HasSignalOnTrack(tile, t)) continue;
				Command<CMD_REMOVE_SINGLE_SIGNAL>::Post(STR_ERROR_CAN_T_REMOVE_SIGNALS_FROM, tile, t);
				asked = true;
			}
			return asked;
		}

		/* One tile of road can hold a road type and a tram type at once, and the
		 * command works along an axis, so only the axes the tile carries are
		 * asked for: an axis with no piece on it answers with an error. */
		case MiniClear::Road: {
			bool asked = false;
			for (RoadTramType rtt : {RTT_ROAD, RTT_TRAM}) {
				if (!HasTileRoadType(tile, rtt)) continue;
				RoadType rt = GetRoadType(tile, rtt);
				RoadBits bits = GetAnyRoadBits(tile, rtt);
				for (Axis a : {AXIS_X, AXIS_Y}) {
					if ((bits & AxisToRoadBits(a)) == ROAD_NONE) continue;
					Command<CMD_REMOVE_LONG_ROAD>::Post(STR_ERROR_CAN_T_REMOVE_ROAD_FROM, tile, tile, rt, a, false, false);
					asked = true;
				}
			}
			return asked;
		}

		case MiniClear::RailStation:
			Command<CMD_REMOVE_FROM_RAIL_STATION>::Post(STR_ERROR_CAN_T_REMOVE_PART_OF_STATION, tile, tile, false);
			return true;

		case MiniClear::RailWaypoint:
			Command<CMD_REMOVE_FROM_RAIL_WAYPOINT>::Post(STR_ERROR_CAN_T_REMOVE_RAIL_WAYPOINT, tile, tile, false);
			return true;

		case MiniClear::RoadWaypoint:
			Command<CMD_REMOVE_FROM_ROAD_WAYPOINT>::Post(STR_ERROR_CAN_T_REMOVE_ROAD_WAYPOINT, tile, tile);
			return true;

		case MiniClear::RoadStop:
			PostRemoveRoadStop(tile, GetRoadStopType(tile));
			return true;

		default:
			break;
	}

	Command<CMD_LANDSCAPE_CLEAR>::Post(STR_ERROR_CAN_T_CLEAR_THIS_AREA, tile);
	return true;
}

std::string_view ClearFilter::Label() const
{
	return LookUp(this->mode).label;
}

/* Whether the filter takes anything off the tile at all. The same question the
 * commit path asks, without asking the game, so the blueprint can answer it for
 * every tile on screen. */
bool ClearFilter::Matches(TileIndex tile) const
{
	return std::ranges::any_of(LookUp(this->mode).kinds, [tile](const MiniClearItem &it) { return ClearKindMatches(tile, it.mode); });
}

/* Every kind the filter holds is offered the tile in turn: a crossing under the
 * rail filter loses its rails and keeps its road. */
bool ClearFilter::Post(TileIndex tile) const
{
	std::span<const MiniClearItem> kinds = LookUp(this->mode).kinds;
	bool takes_track = std::ranges::any_of(kinds, [](const MiniClearItem &it) { return it.mode == MiniClear::RailTrack; });

	bool asked = false;
	for (const MiniClearItem &it : kinds) {
		/* Pulling the track takes its signals with it, so asking for both would
		 * charge the signals twice and refuse the second command. */
		if (it.mode == MiniClear::Signal && takes_track && ClearKindMatches(tile, MiniClear::RailTrack)) continue;
		if (PostClearKind(tile, it.mode)) asked = true;
	}
	return asked;
}
