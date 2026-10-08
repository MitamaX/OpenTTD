/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file forest_field.h Every tree standing on the map, planted block by block from the world's flora, and the ones a view draws at each detail. */

#ifndef MINI_WORLD_FOREST_FIELD_H
#define MINI_WORLD_FOREST_FIELD_H

#include <array>
#include <cstddef>
#include <utility>
#include <vector>

#include "../gpu/instanced_meshes.h"
#include "chunk_grid.h"
#include "chunk_keep.h"
#include "scene_view.h"
#include "tree_models.h"

/* Where a tree stands in tiles and height levels, then its turn, size, colour shift and withering, each a share of a byte. */
struct TreeInstance {
	float x;
	float y;
	float level;
	std::array<uint8_t, 4> look;
};

inline constexpr std::array<VertexAttribute, 2> TREE_INSTANCE_LAYOUT = {{
	{3, 3, AttributeType::Float, offsetof(TreeInstance, x)},
	{4, 4, AttributeType::NormalisedUnsignedByte, offsetof(TreeInstance, look)},
}};

/* The largest size a tree's size byte stands for, as a share of a grown tree. */
inline constexpr double TREE_LARGEST_SCALE = 2.0;

using TreeBatch = InstanceBatch<TreeInstance>;

class ForestField {
public:
	static constexpr int CELL_TILES = 16;

	void Sync(const WorldChanges &changes);
	void Refresh(const SceneView &camera, TreeDetail coarsest_shown, TreeDetail coarsest_cast);
	void Gather(const SceneView &camera, const Frustum &frustum, TreeDetail coarsest, TreeBatch &batch);
	void Release();

private:
	/* A shape's trees within a block, lying together. */
	struct Run {
		size_t shape;
		size_t first;
		size_t count;
	};

	/* A block's trees, sorted by shape, the height levels its ground spans under them, and whether the flora or the ground changed under them since they were planted. */
	struct Cell {
		std::vector<TreeInstance> trees;
		std::vector<Run> runs;
		double low = 0.0;
		double high = 0.0;
		bool planted = false;
		bool stale = true;
		uint64_t drawn = 0;
	};

	/* A stale block near enough for the camera to show its trees, whether it lies out of sight, and the tile pixels where it comes nearest the eye. */
	struct Due {
		bool unseen;
		double pixels;
		size_t index;
	};

	std::pair<Vec3, Vec3> Bounds(const Cell &cell, size_t index) const;
	void Plant(Cell &cell, const TileSpan &tiles) const;
	void GatherCell(const Cell &cell, const SceneView &camera, double near_pixels, double far_pixels, TreeDetail coarsest, TreeBatch &batch) const;
	void Evict();

	ChunkGrid grid{CELL_TILES};
	ChunkKeep keep;
	std::vector<Cell> cells;
	std::vector<Due> due;
	uint64_t frame = 0;
};

#endif /* MINI_WORLD_FOREST_FIELD_H */
