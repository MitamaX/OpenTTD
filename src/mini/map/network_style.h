/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file network_style.h How wide rails and roads are drawn, in tiles from a piece's centre line. */

#ifndef MINI_MAP_NETWORK_STYLE_H
#define MINI_MAP_NETWORK_STYLE_H

/* Far off, each way shows as a band of these half widths on the ground; up close its mesh covers the band. */
inline constexpr double DISTANT_RAIL_HALF = 0.13;
inline constexpr double ROAD_HALF = 0.30;
inline constexpr double TRAM_BED_HALF = 0.18;

#endif /* MINI_MAP_NETWORK_STYLE_H */
