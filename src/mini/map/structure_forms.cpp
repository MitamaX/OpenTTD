/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file structure_forms.cpp The building form of whatever stands on a tile. */

#include "../../stdafx.h"
#include "structure_forms.h"

#include "../../rail_map.h"
#include "../../road_map.h"
#include "../../station_map.h"
#include "../../tile_map.h"
#include "../../water_map.h"
#include "house_forms.h"
#include "industry_forms.h"
#include "object_forms.h"
#include "transport_forms.h"

#include "../../safeguards.h"

std::optional<BuildingForm> StructureForm(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_HOUSE: return HouseForm(tile);
		case MP_INDUSTRY: return IndustryForm(tile);
		case MP_OBJECT: return ObjectForm(tile);
		case MP_RAILWAY: return IsRailDepot(tile) ? DepotForm(tile) : std::nullopt;
		case MP_ROAD: return IsRoadDepot(tile) ? DepotForm(tile) : std::nullopt;
		case MP_WATER: return IsShipDepot(tile) ? DepotForm(tile) : std::nullopt;
		case MP_STATION: return HasStationRail(tile) ? std::nullopt : StationForm(tile);
		default: return std::nullopt;
	}
}
