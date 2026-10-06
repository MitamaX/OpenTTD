/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file facade_painters.h Wall strips: a wall material stacked storey over storey with its openings, and the glazing laid over them. */

#ifndef MINI_ART_FACADE_PAINTERS_H
#define MINI_ART_FACADE_PAINTERS_H

#include "../map/building_form.h"
#include "luma_cell.h"

LumaCell PaintFacadeStrip(const LumaCell &surface, Material wall, WindowGrid grid);
LumaCell PaintGlazingStrip(WindowGrid grid);
float StoreyMean(const LumaCell &strip, WindowGrid grid);

#endif /* MINI_ART_FACADE_PAINTERS_H */
