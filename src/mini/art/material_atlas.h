/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file material_atlas.h Where the map atlas keeps its sprites, building materials and wall strips, and the texture rectangles of each. */

#ifndef MINI_ART_MATERIAL_ATLAS_H
#define MINI_ART_MATERIAL_ATLAS_H

#include <span>

#include "../../core/geometry_type.hpp"
#include "../gpu/draw_list.h"
#include "../map/building_form.h"

inline constexpr int ATLAS_WIDTH = 2048;
inline constexpr int ATLAS_HEIGHT = 1024;
inline constexpr int ATLAS_GUTTER = 8;
inline constexpr int ATLAS_GRAIN = 16;
inline constexpr int MAX_MIP_LEVEL = 4;
inline constexpr int STRIP_TEXELS = 256;
inline constexpr float SURFACE_MEAN = 0.85f;

struct AtlasGrid {
	int origin_y;
	int pitch_x;
	int pitch_y;
	int content_w;
	int content_h;
	int columns;
	int rows;
};

inline constexpr AtlasGrid SPRITE_GRID = {0, 64, 64, 48, 48, 32, 1};
inline constexpr AtlasGrid SURFACE_GRID = {64, 80, 80, 64, 64, 25, 2};
inline constexpr AtlasGrid STRIP_GRID = {224, 80, 272, 64, 256, 25, 2};

Point ContentOrigin(const AtlasGrid &grid, int index);
UvRect ContentUv(const AtlasGrid &grid, int index);
void PaintMaterialCells(std::span<uint32_t> atlas);
UvRect MaterialUv(Material material);
UvRect FacadeUv(Material wall, WindowGrid grid);
UvRect GlazingUv(WindowGrid grid);
float FacadeMean(Material wall, WindowGrid grid);

#endif /* MINI_ART_MATERIAL_ATLAS_H */
