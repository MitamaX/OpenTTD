/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_looks.h Which model a vehicle's unit wears, in whose colours, carrying what, and how high it rides. */

#ifndef MINI_WORLD_VEHICLE_LOOKS_H
#define MINI_WORLD_VEHICLE_LOOKS_H

#include "vehicle_models.h"

struct Vehicle;

/* The owner's livery colours and the cargo's colour as rgb, and how full the unit is. */
struct VehiclePaint {
	uint32_t primary;
	uint32_t secondary;
	uint32_t cargo;
	double load;
};

/* All of these read the game's state, so they run only while it holds still. */
VehicleLook LookOf(const Vehicle *unit);
VehiclePaint PaintOf(const Vehicle *unit);
/* How far above the ground or water under it a unit's model stands: on the rail heads, the road's surface or the waterline. */
double RideHeightOf(const Vehicle *unit);

#endif /* MINI_WORLD_VEHICLE_LOOKS_H */
