/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ground_cover.h Tufts of grass scattered over open meadow and rough land and scrub over the desert, laid out block by block from the map's ground and shown only near the eye. */

#ifndef MINI_WORLD_GROUND_COVER_H
#define MINI_WORLD_GROUND_COVER_H

#include <array>
#include <vector>

#include "chunk_grid.h"
#include "scatter.h"
#include "tuft_models.h"

class GroundCover final : public Scatter {
public:
	void Sync(const WorldChanges &changes) override;
	/* Each tuft is gathered in the bucket of its shape, a whole block at a time. */
	void Gather(const SceneView &view, const Frustum &frustum, double fewest_pixels, VehicleBatch &batch) override;
	void Release() override;

private:
	static constexpr int BLOCK_TILES = 16;

	struct Block {
		std::array<std::vector<VehicleInstance>, TUFT_SHAPES> tufts;
		bool stale = true;
	};

	void Build(size_t index);

	ChunkGrid grid{BLOCK_TILES};
	std::vector<Block> blocks;
};

#endif /* MINI_WORLD_GROUND_COVER_H */
