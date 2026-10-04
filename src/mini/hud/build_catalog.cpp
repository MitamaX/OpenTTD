/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file build_catalog.cpp The build tools as the menus list them, and what each one is called. */

#include "../../stdafx.h"
#include "build_catalog.h"

#include "../tools/build_tool.h"
#include "../ui/ui_text.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static const MiniMenuItem _menu_rail_items[] = {
	{STR_LAI_RAIL_DESCRIPTION_TRACK, "TRACK", MiniTool::Rail, "rail"},
	{STR_LAI_STATION_DESCRIPTION_RAILROAD_STATION, "STATION", MiniTool::Station, "station"},
	{STR_LAI_STATION_DESCRIPTION_WAYPOINT, "WAYPOINT", MiniTool::RailWaypoint, "rail-waypoint"},
	{STR_COMPANY_INFRASTRUCTURE_VIEW_SIGNALS, "SIGNAL", MiniTool::Signal, "signal"},
	{STR_LAI_RAIL_DESCRIPTION_TRAIN_DEPOT, "DEPOT", MiniTool::TrainDepot, "train-depot"},
	{STR_LAI_TUNNEL_DESCRIPTION_RAILROAD, "TUNNEL", MiniTool::RailTunnel, "rail-tunnel"},
	{INVALID_STRING_ID, "BRIDGE", MiniTool::RailBridge, "rail-bridge"},
	{INVALID_STRING_ID, "CONVERT", MiniTool::Convert, "rail-convert"},
};

static const MiniMenuItem _menu_road_items[] = {
	{STR_LAI_ROAD_DESCRIPTION_ROAD, "ROAD", MiniTool::Road, "road"},
	{STR_LAI_STATION_DESCRIPTION_BUS_STATION, "BUS", MiniTool::BusStop, "bus-stop"},
	{STR_LAI_STATION_DESCRIPTION_TRUCK_LOADING_AREA, "TRUCK", MiniTool::TruckStop, "truck-stop"},
	{STR_LAI_STATION_DESCRIPTION_WAYPOINT, "WAYPOINT", MiniTool::RoadWaypoint, "road-waypoint"},
	{STR_LAI_ROAD_DESCRIPTION_ROAD_VEHICLE_DEPOT, "DEPOT", MiniTool::RoadDepot, "road-depot"},
	{STR_LAI_TUNNEL_DESCRIPTION_ROAD, "TUNNEL", MiniTool::RoadTunnel, "road-tunnel"},
	{INVALID_STRING_ID, "BRIDGE", MiniTool::RoadBridge, "road-bridge"},
	{INVALID_STRING_ID, "CONVERT", MiniTool::RoadConvert, "road-convert"},
};

static const MiniMenuItem _menu_water_items[] = {
	{STR_LAI_STATION_DESCRIPTION_SHIP_DOCK, "DOCK", MiniTool::Dock, "dock"},
	{STR_LAI_WATER_DESCRIPTION_SHIP_DEPOT, "DEPOT", MiniTool::ShipDepot, "ship-depot"},
	{STR_LAI_STATION_DESCRIPTION_BUOY, "BUOY", MiniTool::Buoy, "buoy"},
	{STR_LAI_WATER_DESCRIPTION_CANAL, "CANAL", MiniTool::Canal, "canal"},
	{STR_LAI_WATER_DESCRIPTION_LOCK, "LOCK", MiniTool::Lock, "lock"},
};

static const MiniMenuItem _menu_air_items[] = {
	{STR_LAI_STATION_DESCRIPTION_AIRPORT, "AIRPORT", MiniTool::Airport, "airport"},
};

static const MiniMenuItem _menu_land_items[] = {
	{STR_LAI_OBJECT_DESCRIPTION_COMPANY_HEADQUARTERS, "HQ", MiniTool::Headquarters, "headquarters"},
	{STR_LAI_TREE_NAME_TREES, "TREES", MiniTool::Trees, "trees"},
	{STR_LAI_OBJECT_DESCRIPTION_COMPANY_OWNED_LAND, "LAND", MiniTool::BuyLand, "buy-land"},
	{INVALID_STRING_ID, "INDUSTRY", MiniTool::Industry, "industry"},
	{INVALID_STRING_ID, "SIGN", MiniTool::Sign, "sign"},
};

/* Area-command tools live apart from construction: the bottom-right corner
 * is the command corner in the reference layout. */
static const MiniMenuItem _cmd_items[] = {
	{INVALID_STRING_ID, "LEVEL", MiniTool::Terraform, "terraform"},
	{INVALID_STRING_ID, "CLEAR", MiniTool::Demolish, "demolish"},
};

static const MiniMenuCategory _menu_cats[] = {
	{STR_RAIL_NAME_RAILROAD, "RAIL", "rail", _menu_rail_items},
	{STR_ROAD_NAME_ROAD, "ROAD", "road", _menu_road_items},
	{STR_LAI_WATER_DESCRIPTION_WATER, "WATER", "dock", _menu_water_items},
	{STR_REPLACE_VEHICLE_AIRCRAFT, "AIR", "airport", _menu_air_items},
	{INVALID_STRING_ID, "LAND", "headquarters", _menu_land_items},
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

MenuTile ToolTile(const MiniMenuItem &item)
{
	return MakeMenuTile(item.str, item.fallback, item.icon, _tool.Kind() == item.tool);
}

MenuTile CategoryTile(const MiniMenuCategory &category, bool open)
{
	return MakeMenuTile(category.str, category.fallback, category.icon, open);
}
