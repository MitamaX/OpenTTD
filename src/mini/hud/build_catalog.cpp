/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file build_catalog.cpp The build tools as the menus list them, and what each one is called. */

#include "../../stdafx.h"
#include "build_catalog.h"

#include "../ui/ui_text.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

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
	{STR_LAI_STATION_DESCRIPTION_WAYPOINT, "WAYPOINT", MiniTool::RoadWaypoint},
	{STR_LAI_ROAD_DESCRIPTION_ROAD_VEHICLE_DEPOT, "DEPOT", MiniTool::RoadDepot},
	{STR_LAI_TUNNEL_DESCRIPTION_ROAD, "TUNNEL", MiniTool::RoadTunnel},
	{INVALID_STRING_ID, "BRIDGE", MiniTool::RoadBridge},
	{INVALID_STRING_ID, "CONVERT", MiniTool::RoadConvert},
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
	{INVALID_STRING_ID, "SIGN", MiniTool::Sign},
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

std::span<const MiniMenuCategory> BuildCategories()
{
	return _menu_cats;
}

std::span<const MiniMenuItem> CommandItems()
{
	return _cmd_items;
}

static const MiniMenuItem *FindItem(MiniTool tool)
{
	for (const MiniMenuCategory &cat : _menu_cats) {
		for (const MiniMenuItem &it : cat.items) {
			if (it.tool == tool) return &it;
		}
	}
	for (const MiniMenuItem &it : _cmd_items) {
		if (it.tool == tool) return &it;
	}
	return nullptr;
}

std::string ToolLabel(MiniTool tool)
{
	const MiniMenuItem *item = FindItem(tool);
	return item == nullptr ? std::string() : GameTextOr(item->str, item->fallback);
}
