/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file block_scatter.h A scatter laid out a block of tiles at a time from the world's texels, and laid out afresh where they change. */

#ifndef MINI_WORLD_BLOCK_SCATTER_H
#define MINI_WORLD_BLOCK_SCATTER_H

#include <vector>

#include "chunk_grid.h"
#include "scatter.h"

/* A tile with nothing on it: no way, no building and no water. */
bool IsOpenLand(int tx, int ty);
/* Whether every tile within reach of one is open land. */
bool ClearAround(int tx, int ty, int reach);

/* The copies of each model a block holds. */
using ScatterCopies = std::vector<std::vector<VehicleInstance>>;

/* A block goes stale when a tile within reach of it changes, since what stands on a tile may hang on the tiles about it; only a few stale blocks are strewn again each frame. */
class BlockScatter : public Scatter {
public:
	BlockScatter(size_t models, int reach) : models(models), reach(reach) {}

	void Sync(const WorldChanges &changes) final;
	void Gather(const SceneView &view, const Frustum &frustum, double fewest_pixels, VehicleBatch &batch) final;
	void Release() final;

protected:
	/* Adds the copies standing on the tiles to the buckets of their models. */
	virtual void Strew(const TileSpan &tiles, ScatterCopies &copies) const = 0;

private:
	static constexpr int BLOCK_TILES = 16;
	static constexpr int MOST_STREWN_PER_FRAME = 4;

	struct Block {
		ScatterCopies copies;
		bool stale = true;
	};

	void Lay();

	ChunkGrid grid{BLOCK_TILES};
	std::vector<Block> blocks;
	size_t models;
	int reach;
};

#endif /* MINI_WORLD_BLOCK_SCATTER_H */
