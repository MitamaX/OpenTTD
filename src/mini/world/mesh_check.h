/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mesh_check.h A survey of the ground and water meshes of the loaded map for cracks, overlaps, folds, flipped faces, floating slabs and kinked ways, run once when the environment asks for it. */

#ifndef MINI_WORLD_MESH_CHECK_H
#define MINI_WORLD_MESH_CHECK_H

/* With OTTD_MINI_MESHCHECK naming a CSV file, surveys the map once it is loaded, optionally within OTTD_MINI_MESHCHECK_AREA "x0,y0,x1,y1", writes the findings and quits. */
void RunAskedMeshCheck();

#endif /* MINI_WORLD_MESH_CHECK_H */
