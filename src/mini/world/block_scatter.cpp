/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file block_scatter.cpp A scatter laid out a block of tiles at a time from the world's texels, and laid out afresh where they change. */

#include "../../stdafx.h"
#include "block_scatter.h"

#include <algorithm>
#include <span>

#include "../../map_func.h"
#include "../gpu/frame_profile.h"
#include "../map/tile_shapes.h"

#include "../../safeguards.h"

bool IsOpenLand(int tx, int ty)
{
	if (!OnMap(tx, ty)) return false;
	TileIndex tile = TileXY(tx, ty);
	return GroundworkOf(_world_tiles.GroundAt(tile), _world_tiles.NetworkAt(tile)) == Groundwork::Open && _world_tiles.WaterAt(tile).level == 0;
}

bool ClearAround(int tx, int ty, int reach)
{
	for (int dy = -reach; dy <= reach; dy++) {
		for (int dx = -reach; dx <= reach; dx++) {
			if (!IsOpenLand(tx + dx, ty + dy)) return false;
		}
	}
	return true;
}

void BlockScatter::Lay()
{
	this->grid.Lay(_world_tiles.Size());
	this->blocks = std::vector<Block>(this->grid.Count());
}

void BlockScatter::Sync(const WorldChanges &changes)
{
	if (changes.whole || this->grid.Map() != _world_tiles.Size()) {
		this->Lay();
		return;
	}
	this->grid.ForEachTouched(changes.areas, this->reach, [&](size_t index) { this->blocks[index].stale = true; });
}

void BlockScatter::Gather(const SceneView &view, const Frustum &frustum, double fewest_pixels, VehicleBatch &batch)
{
	if (this->grid.Map() != _world_tiles.Size()) this->Lay();
	double top = (_world_tiles.Peak() + 1.0) * LevelRise();
	this->waiting.clear();
	this->grid.ForEachSeen(view, frustum, fewest_pixels, 0, top, [&](size_t index) {
		if (this->blocks[index].stale) this->waiting.push_back(index);
		return true;
	});
	size_t due = std::min<size_t>(this->waiting.size(), MOST_STREWN_PER_FRAME);
	std::partial_sort(this->waiting.begin(), this->waiting.begin() + due, this->waiting.end(), [&](size_t a, size_t b) { return this->blocks[a].strewn_turn < this->blocks[b].strewn_turn; });
	for (size_t index : std::span(this->waiting).first(due)) {
		Block &block = this->blocks[index];
		_frame_profile.Count("scatter_strews");
		ProfileScope profile("build", "scatter", ProfileClock::Cpu);
		block.copies.assign(this->models, {});
		this->Strew(this->grid.TilesOf(index), block.copies);
		block.stale = false;
		block.strewn_turn = ++this->strews;
	}

	this->grid.ForEachSeen(view, frustum, fewest_pixels, 0, top, [&](size_t index) {
		const Block &block = this->blocks[index];
		for (size_t model = 0; model < block.copies.size(); model++) batch.Add(model, block.copies[model]);
		return true;
	});
}

void BlockScatter::Release()
{
	this->grid.Clear();
	this->blocks.clear();
}
