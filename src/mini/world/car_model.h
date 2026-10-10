/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file car_model.h The car parked about town, at a kerb or in a yard, built of the parts road vehicles are. */

#ifndef MINI_WORLD_CAR_MODEL_H
#define MINI_WORLD_CAR_MODEL_H

#include "../model/model_mesh.h"

/* Nose along x in the paint given, its right wheels on the ground at the origin and its left up a kerb as high as given. */
ModelMesh CarModel(uint32_t paint, double kerb = 0.0);

#endif /* MINI_WORLD_CAR_MODEL_H */
