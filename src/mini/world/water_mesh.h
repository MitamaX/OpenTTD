/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file water_mesh.h The water's surface as triangles: level sheets over open water and shores, sloping ones down streams, and the sea beyond the edge. */

#ifndef MINI_WORLD_WATER_MESH_H
#define MINI_WORLD_WATER_MESH_H

#include <array>
#include <cstddef>

#include "../../core/geometry_type.hpp"
#include "../core/camera.h"
#include "../gpu/mesh_buffer.h"

/* Positions run in tiles and height levels. */
struct WaterVertex {
	float x;
	float y;
	float level;
};

inline constexpr std::array<VertexAttribute, 1> WATER_LAYOUT = {{
	{0, 3, AttributeType::Float, offsetof(WaterVertex, x)},
}};

using WaterMesh = TriangleList<WaterVertex>;

WaterMesh BuildWaterSurface(const TileSpan &tiles);
WaterMesh BuildOuterWater(Dimension map, double reach);

#endif /* MINI_WORLD_WATER_MESH_H */
