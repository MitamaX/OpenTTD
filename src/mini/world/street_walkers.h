/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file street_walkers.h People strolling along town pavements, laid out from the map's own roads and moved every frame; they only ever look, never live. */

#ifndef MINI_WORLD_STREET_WALKERS_H
#define MINI_WORLD_STREET_WALKERS_H

#include <array>
#include <span>
#include <vector>

#include "chunk_grid.h"
#include "scatter.h"

/* A walker paces up and down a stretch of pavement, from its start along a heading for a length, or stands still where its pace is nothing. */
struct Walker {
	float x;
	float y;
	float along_x;
	float along_y;
	float length;
	float phase;
	float pace;
	std::array<uint8_t, 4> shirt;
	std::array<uint8_t, 4> trousers;
};

/* Walkers stand on the pavements of town streets that run straight on through a tile, more of them where houses crowd close.
 * A block is laid out again once the roads or houses about it change, a few blocks a frame at most. */
class StreetWalkers final : public Scatter {
public:
	void Sync(const WorldChanges &changes) override;
	/* Each walker is gathered in the bucket of its pose. */
	void Gather(const SceneView &view, const Frustum &frustum, double fewest_pixels, VehicleBatch &batch) override;
	void Release() override;

private:
	static constexpr int BLOCK_TILES = 16;

	struct Block {
		std::vector<Walker> walkers;
		bool stale = true;
	};

	void Build(size_t index);
	static size_t GatherBlock(const SceneView &view, std::span<const Walker> walkers, double fewest_pixels, VehicleBatch &batch);

	ChunkGrid grid{BLOCK_TILES};
	std::vector<Block> blocks;
};

#endif /* MINI_WORLD_STREET_WALKERS_H */
