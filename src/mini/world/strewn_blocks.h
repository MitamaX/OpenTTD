/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file strewn_blocks.h What a scatter laid out on each block of tiles, laid out afresh within a slice of each frame where the world about it changed. */

#ifndef MINI_WORLD_STREWN_BLOCKS_H
#define MINI_WORLD_STREWN_BLOCKS_H

#include <algorithm>
#include <tuple>
#include <vector>

#include "../gpu/frame_profile.h"
#include "build_slice.h"
#include "chunk_grid.h"

/* A block goes stale when a tile within reach of it changes in a way the scatter reads, since what stands on a tile may hang on the tiles about it.
 * The stale blocks the camera sees where a tile spans the pixels they show at are strewn again first, those strewn longest ago first,
 * then those out of its sight where a tile spans the pixels they cast shadows at; each shows what it held until its turn comes. */
template <class Content>
class StrewnBlocks {
public:
	StrewnBlocks(ChangeKinds reads, int reach) : reads(reads), reach(reach) {}

	void Sync(const WorldChanges &changes)
	{
		if (changes.whole || this->grid.Map() != _world_tiles.Size()) {
			this->Lay();
			return;
		}
		this->grid.ForEachTouched(changes, this->reads, this->reach, [&](size_t index) { this->blocks[index].stale = true; });
	}

	/* Strew lays out a block's tiles into its content afresh; the blocks are grown by a margin of tiles. */
	template <class Strew>
	void Refresh(const SceneView &camera, double shown_pixels, double cast_pixels, int margin, Strew strew)
	{
		if (this->grid.Map() != _world_tiles.Size()) this->Lay();
		double top = Top();
		this->waiting.clear();
		this->grid.ForEachWithin(camera, std::min(shown_pixels, cast_pixels), margin, [&](size_t index) {
			const Block &block = this->blocks[index];
			if (!block.stale) return true;
			auto [low, high] = this->grid.BoxOf(index, margin, top);
			bool unseen = !BoxMeets(camera.frustum, low, high);
			if (camera.NearestTilePixels(low, high) >= (unseen ? cast_pixels : shown_pixels)) this->waiting.emplace_back(unseen, block.strewn_turn, index);
			return true;
		});
		std::ranges::sort(this->waiting);

		BuildSlice slice(STREWING_SLICE);
		for (const auto &[unseen, turn, index] : this->waiting) {
			if (slice.Spent()) return;
			Block &block = this->blocks[index];
			_frame_profile.Count("scatter_strews");
			ProfileScope profile("build", "scatter", ProfileClock::Cpu);
			strew(this->grid.TilesOf(index), block.content);
			block.stale = false;
			block.strewn_turn = ++this->strews;
		}
	}

	/* Every block's content the view sees where a tile spans at least the fewest pixels, the blocks grown by a margin of tiles; the visit says whether to go on. */
	template <class Visit>
	void ForEachSeen(const SceneView &view, const Frustum &frustum, double fewest_pixels, int margin, Visit visit)
	{
		this->grid.ForEachSeen(view, frustum, fewest_pixels, margin, Top(), [&](size_t index) { return visit(this->blocks[index].content); });
	}

	void Release()
	{
		this->grid.Clear();
		this->blocks.clear();
	}

private:
	static constexpr int BLOCK_TILES = 16;
	static constexpr std::chrono::microseconds STREWING_SLICE{300};

	struct Block {
		Content content;
		bool stale = true;
		uint64_t strewn_turn = 0;
	};

	static double Top() { return (_world_tiles.Peak() + 1.0) * LevelRise(); }

	void Lay()
	{
		this->grid.Lay(_world_tiles.Size());
		this->blocks = std::vector<Block>(this->grid.Count());
	}

	ChunkGrid grid{BLOCK_TILES};
	std::vector<Block> blocks;
	std::vector<std::tuple<bool, uint64_t, size_t>> waiting;
	uint64_t strews = 0;
	ChangeKinds reads;
	int reach;
};

#endif /* MINI_WORLD_STREWN_BLOCKS_H */
