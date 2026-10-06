/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file terrain_mesh.h The ground of the map as triangles: each tile's facets, the walls where tiles step apart, and the sea beyond the edge. */

#ifndef MINI_WORLD_TERRAIN_MESH_H
#define MINI_WORLD_TERRAIN_MESH_H

#include <array>
#include <cstddef>
#include <vector>

#include "../../core/geometry_type.hpp"
#include "../core/camera.h"
#include "../gpu/mesh_buffer.h"

/* Positions run in tiles and height levels; the normal is in render space and its last byte marks a wall. */
struct TerrainVertex {
	float x;
	float y;
	float level;
	std::array<int8_t, 4> normal;
};

inline constexpr std::array<VertexAttribute, 2> TERRAIN_LAYOUT = {{
	{0, 3, AttributeType::Float, offsetof(TerrainVertex, x)},
	{1, 4, AttributeType::NormalisedByte, offsetof(TerrainVertex, normal)},
}};

struct TerrainMesh {
	std::vector<TerrainVertex> vertices;
	std::vector<uint32_t> indices;

	uint32_t Add(const WorldPoint &at, const Vec3 &normal, bool wall);
	void Triangle(uint32_t a, uint32_t b, uint32_t c);
	void Quad(uint32_t a, uint32_t b, uint32_t c, uint32_t d);
};

/* At step one every tile keeps its own facets and walls; a longer step lays a lattice over every step-th corner, skirted down so nothing shows through beside a finer neighbour. */
TerrainMesh BuildTerrain(const TileSpan &tiles, int step);
TerrainMesh BuildOuterSea(Dimension map, double reach);

#endif /* MINI_WORLD_TERRAIN_MESH_H */
