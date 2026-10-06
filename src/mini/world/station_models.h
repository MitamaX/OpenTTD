/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file station_models.h Stations in 3D: rail platforms with their buildings and roofs as each tile's layout stands them, waypoint gantries, and bus shelters. */

#ifndef MINI_WORLD_STATION_MODELS_H
#define MINI_WORLD_STATION_MODELS_H

#include "../../tile_type.h"
#include "way_shapes.h"

void LayRailStop(ModelMesh &mesh, TileIndex tile, WayDetail detail);
void LayBusShelter(ModelMesh &mesh, TileIndex tile, WayDetail detail);

#endif /* MINI_WORLD_STATION_MODELS_H */
