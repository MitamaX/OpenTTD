/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file surface_painters.h One repeating grey pattern per building material, shared by the walls and roofs made of it. */

#ifndef MINI_ART_SURFACE_PAINTERS_H
#define MINI_ART_SURFACE_PAINTERS_H

#include <cstdint>

#include "../map/building_form.h"
#include "luma_cell.h"

using SurfacePainter = void (*)(LumaCell &cell, uint32_t seed);

LumaCell PaintSurface(Material material);

#endif /* MINI_ART_SURFACE_PAINTERS_H */
