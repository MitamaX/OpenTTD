/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file network_style.h How wide and how high rails and roads are drawn, in tiles from a piece's centre line. */

#ifndef MINI_MAP_NETWORK_STYLE_H
#define MINI_MAP_NETWORK_STYLE_H

#include <array>

#include "world_tiles.h"

inline constexpr double RAIL_BED_HALF = 0.26;
inline constexpr double RAIL_GAUGE_HALF = 0.11;
inline constexpr double RAIL_HALF = 0.022;
inline constexpr double NO_RAILS = 0.0;
inline constexpr double MONORAIL_HALF = 0.10;
inline constexpr double MAGLEV_HALF = 0.21;
inline constexpr double MAGLEV_STRIP_HALF = 0.032;
inline constexpr double DISTANT_RAIL_HALF = RAIL_GAUGE_HALF + RAIL_HALF;
inline constexpr double ROAD_HALF = 0.30;
inline constexpr double TRAM_BED_HALF = 0.18;
inline constexpr double DECK_HALF = 0.36;
inline constexpr double CATENARY_RISE = 0.45;
inline constexpr double WIRE_HALF = 0.006;

inline constexpr double SLEEPERS_PER_TILE = 7.0;
inline constexpr double RAIL_FREQUENCY = 1.0 / (2.0 * RAIL_GAUGE_HALF);
inline constexpr double MARKING_FREQUENCY = 4.0;

struct RailLookWidths {
	double body;
	double strip;
};

inline constexpr std::array<RailLookWidths, static_cast<size_t>(RailLook::End)> RAIL_LOOK_WIDTHS = {{
	{RAIL_BED_HALF, RAIL_HALF},
	{MONORAIL_HALF, NO_RAILS},
	{MAGLEV_HALF, MAGLEV_STRIP_HALF},
}};

#endif /* MINI_MAP_NETWORK_STYLE_H */
