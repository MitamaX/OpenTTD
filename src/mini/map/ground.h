/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ground.h The colour and art of bare ground, by height, slope and cover. */

#ifndef MINI_MAP_GROUND_H
#define MINI_MAP_GROUND_H

#include "../../mini_atlas.h"
#include "../../slope_type.h"
#include "../../tile_type.h"

uint32_t GroundColour(TileIndex tile, int h);
MiniSprite GroundSlot(TileIndex tile);
uint32_t GroundOverviewColour(TileIndex tile, Slope s, int hbase);
void DrawGround(TileIndex tile, int x0, int y0, int x1, int y1, int ppt);

#endif /* MINI_MAP_GROUND_H */
