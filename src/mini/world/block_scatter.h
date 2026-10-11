/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file block_scatter.h A scatter laid out a block of tiles at a time from the world's texels, and laid out afresh where they change. */

#ifndef MINI_WORLD_BLOCK_SCATTER_H
#define MINI_WORLD_BLOCK_SCATTER_H

#include <array>
#include <vector>

#include "chunk_grid.h"
#include "scatter.h"
#include "strewn_blocks.h"

/* A tile with nothing on it: no way, no building and no water. */
bool IsOpenLand(int tx, int ty);
inline constexpr ChangeKinds OPEN_LAND_READS = {ChangeKind::Cover, ChangeKind::Ways, ChangeKind::Water};
/* Whether every tile within reach of one is open land. */
bool ClearAround(int tx, int ty, int reach);

/* The copies of each model a block holds. */
using ScatterCopies = std::vector<std::vector<VehicleInstance>>;

VehicleInstance StrewnCopy(const Vec3 &at, const Vec3 &attitude, double scale, const std::array<uint8_t, 4> &primary, const std::array<uint8_t, 4> &secondary);

class BlockScatter : public Scatter {
public:
	BlockScatter(size_t models, int reach, ChangeKinds reads, int margin = 0) : models(models), margin(margin), blocks(reads, reach) {}

	void Sync(const WorldChanges &changes) final;
	void Refresh(const SceneView &camera, double shown_pixels, double cast_pixels) final;
	void Gather(const Sight &sight, double fewest_pixels, VehicleBatch &batch) final;
	void Release() final;

protected:
	/* Adds the copies standing on the tiles to the buckets of their models. */
	virtual void Strew(const TileSpan &tiles, ScatterCopies &copies) const = 0;

private:
	size_t models;
	int margin;
	StrewnBlocks<ScatterCopies> blocks;
};

#endif /* MINI_WORLD_BLOCK_SCATTER_H */
