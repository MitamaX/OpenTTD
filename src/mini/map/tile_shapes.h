/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tile_shapes.h Track, road and band strokes across one tile's square. */

#ifndef MINI_MAP_TILE_SHAPES_H
#define MINI_MAP_TILE_SHAPES_H

#include "../../core/geometry_type.hpp"
#include "../../direction_type.h"
#include "../../road_type.h"
#include "../../track_type.h"

inline constexpr int _diag_dx[DIAGDIR_END] = {0, 1, 0, -1};
inline constexpr int _diag_dy[DIAGDIR_END] = {-1, 0, 1, 0};

void DrawTrackPiece(Track t, int x0, int y0, int x1, int y1, int width, uint32_t c);
void DrawTrackBitsPx(TrackBits bits, int x0, int y0, int x1, int y1, int width, uint32_t c);
void DrawRoadBitsPx(RoadBits bits, int x0, int y0, int x1, int y1, int width, uint32_t c);
void DrawAxisBand(Axis axis, int x0, int y0, int x1, int y1, int width, uint32_t c);

inline void DrawTrackPiece(Track t, const Rect &r, int width, uint32_t c)
{
	DrawTrackPiece(t, r.left, r.top, r.right, r.bottom, width, c);
}

inline void DrawAxisBand(Axis axis, const Rect &r, int width, uint32_t c)
{
	DrawAxisBand(axis, r.left, r.top, r.right, r.bottom, width, c);
}

#endif /* MINI_MAP_TILE_SHAPES_H */
