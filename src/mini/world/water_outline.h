/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file water_outline.h The curved outline the water shaders draw, worked out on the CPU as well so the ground under the water can follow it. */

#ifndef MINI_WORLD_WATER_OUTLINE_H
#define MINI_WORLD_WATER_OUTLINE_H

#include <array>

/* Where the smoothed water field crosses this, the water's outline runs. */
inline constexpr double WATERLINE = 0.58;
/* How far natural shores fray off their tiles, in tiles. */
inline constexpr double SHORE_WARP = 0.22;
inline constexpr double SHORE_WARP_FREQUENCY = 1.7;
inline constexpr std::array<double, 2> SHORE_WARP_SEEDS = {7.1, 93.4};
/* How far in levels a sheet may stand from a tile's water and still join it wholly, and from how far it no longer joins it at all. */
inline constexpr double SHEET_HOLD = 0.5;
inline constexpr double SHEET_REACH = 1.0;
inline constexpr double BRIM_PULL = 0.35;
inline constexpr double BRIM_FADE = 0.45;

/* How far a point lies inside the water of a sheet, as the water shaders draw it: the water of every tile as wholly as it joins the sheet smoothed by a cubic B-spline,
 * drawn back from dry ground falling away below a level river's sheet and read about the point frayed the way natural shores fray. */
double SheetWater(double x, double y, double level);
double Joining(int tx, int ty, double level);

#endif /* MINI_WORLD_WATER_OUTLINE_H */
