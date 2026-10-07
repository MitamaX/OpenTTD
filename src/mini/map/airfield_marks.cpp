/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file airfield_marks.cpp What an airport tile is in the game's own airport layouts, and the markings its ground is painted with. */

#include "../../stdafx.h"
#include "airfield_marks.h"

#include "../../newgrf_airporttiles.h"
#include "../../station_map.h"
#include "../../table/airporttile_ids.h"

#include "../../safeguards.h"

std::optional<StationGfx> AirportStandIn(TileIndex tile)
{
	StationGfx gfx = GetAirportGfx(tile);
	StationGfx stand_in = gfx < NEW_AIRPORTTILE_OFFSET ? gfx : AirportTileSpec::Get(gfx)->grf_prop.subst_id;
	if (stand_in >= NEW_AIRPORTTILE_OFFSET) return std::nullopt;
	return stand_in;
}

AirfieldMark AirfieldMarkOf(TileIndex tile)
{
	std::optional<StationGfx> stand_in = AirportStandIn(tile);
	if (!stand_in.has_value()) return AirfieldMark::Apron;
	switch (*stand_in) {
		case APT_RUNWAY_1:
		case APT_RUNWAY_2:
		case APT_RUNWAY_3:
		case APT_RUNWAY_4:
		case APT_RUNWAY_5:
		case APT_RUNWAY_END:
		case APT_RUNWAY_END_FENCE_SE:
		case APT_RUNWAY_END_FENCE_NW:
		case APT_RUNWAY_FENCE_NW:
		case APT_RUNWAY_END_FENCE_NW_SW:
		case APT_RUNWAY_END_FENCE_SE_SW:
		case APT_RUNWAY_END_FENCE_NE_NW:
		case APT_RUNWAY_END_FENCE_NE_SE:
		case APT_RUNWAY_SMALL_NEAR_END:
		case APT_RUNWAY_SMALL_MIDDLE:
		case APT_RUNWAY_SMALL_FAR_END:
			return AirfieldMark::Runway;

		case APT_APRON_W:
		case APT_APRON_S:
		case APT_APRON_E:
		case APT_ARPON_N:
		case APT_APRON_HOR:
		case APT_APRON_N_FENCE_SW:
		case APT_APRON_VER_CROSSING_S:
		case APT_APRON_HOR_CROSSING_W:
		case APT_APRON_VER_CROSSING_N:
		case APT_APRON_HOR_CROSSING_E:
			return AirfieldMark::Taxiway;

		case APT_STAND:
		case APT_STAND_1:
		case APT_STAND_PIER_NE:
			return AirfieldMark::Stand;

		case APT_HELIPAD_1:
		case APT_HELIPAD_2:
		case APT_HELIPAD_2_FENCE_NW:
		case APT_HELIPAD_2_FENCE_NE_SE:
		case APT_HELIPAD_3_FENCE_SE_SW:
		case APT_HELIPAD_3_FENCE_NW_SW:
		case APT_HELIPAD_3_FENCE_NW:
			return AirfieldMark::Helipad;

		case APT_GRASS_FENCE_SW:
		case APT_GRASS_1:
		case APT_GRASS_2:
		case APT_GRASS_FENCE_NE_FLAG:
		case APT_GRASS_FENCE_NE_FLAG_2:
		case APT_EMPTY:
		case APT_EMPTY_FENCE_NE:
		case APT_RADAR_GRASS_FENCE_SW:
			return AirfieldMark::Grass;

		default:
			return AirfieldMark::Apron;
	}
}
