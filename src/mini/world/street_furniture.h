/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file street_furniture.h The small things standing beside ways: lamp posts, benches and level crossing barriers. */

#ifndef MINI_WORLD_STREET_FURNITURE_H
#define MINI_WORLD_STREET_FURNITURE_H

#include "way_shapes.h"

/* Each stands on a point of the map at a height above its footing, its front turned toward a direction. */
ModelMesh LampPost(const MapVector &at, const MapVector &facing, double base);
ModelMesh Bench(const MapVector &at, const MapVector &facing, double base);
ModelMesh CrossingBarrier(const MapVector &at, const MapVector &facing, double base);

#endif /* MINI_WORLD_STREET_FURNITURE_H */
