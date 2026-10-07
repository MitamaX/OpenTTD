/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file terrain_mesh.h The ground of the map as triangles: each tile's facets, the walls where tiles step apart, and the seabed beyond the edge. */

#ifndef MINI_WORLD_TERRAIN_MESH_H
#define MINI_WORLD_TERRAIN_MESH_H

#include <array>
#include <cstddef>

#include "../../core/geometry_type.hpp"
#include "../core/camera.h"
#include "../gpu/mesh_buffer.h"
#include "seabed.h"

/* Positions run in tiles and height levels; the normal is in render space and its last byte marks a wall, or how far below the water the ground has sunk. */
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

inline constexpr double WALL_MARK = 1.0;
/* The face of earth the ground is cut down in at the map's edge. */
inline constexpr double EDGE_MARK = 0.5;
inline constexpr double DRY_MARK = 0.0;
/* Earth banked up under a way, which grass covers however steep it stands. */
inline constexpr double FILL_MARK = 0.25;

struct TerrainMesh : TriangleList<TerrainVertex> {
	using TriangleList<TerrainVertex>::Add;
	uint32_t Add(const WorldPoint &at, const Vec3 &normal, double mark);
};

double SunkMark(double sink);

/* A block's ground, built a row of tiles at a time so the work may be spread over frames. At step one every tile keeps its own facets and walls;
 * a longer step lays a lattice over every step-th corner in one go, skirted down so nothing shows through beside a finer neighbour.
 * Under water the ground sinks toward the seabed; along the map's edge it is cut down in a face of earth over the shelf of seabed falling away beyond. */
class TerrainBuild {
public:
	TerrainBuild(const TileSpan &tiles, int step);

	int Step() const { return this->step; }
	bool Done() const { return this->next_row > this->tiles.ty1; }
	void Advance();
	TerrainMesh Finish();

private:
	TileSpan tiles;
	int step;
	int next_row;
	Seabed bed;
	TerrainMesh mesh;
};

/* The open sea's floor, flat from the foot of the shelves around the map out to the horizon. */
TerrainMesh BuildOuterBed(Dimension map, double reach);

#endif /* MINI_WORLD_TERRAIN_MESH_H */
