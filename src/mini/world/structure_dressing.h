/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file structure_dressing.h The small things a building wears up close: chimney pots, ridge caps, balconies, air conditioners, ladders and rims. */

#ifndef MINI_WORLD_STRUCTURE_DRESSING_H
#define MINI_WORLD_STRUCTURE_DRESSING_H

#include "../map/volume_geometry.h"
#include "structure_mesh.h"

/* Whether a fixture's own model stands in place of its plain solid. */
bool ReplacesSolid(const Solid &solid);
void DressSolid(const FormStyle &style, const Solid &solid, const PlanRect &footprint, StructureMesh &mesh);

#endif /* MINI_WORLD_STRUCTURE_DRESSING_H */
