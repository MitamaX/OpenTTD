/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file block_scatter.cpp A scatter laid out a block of tiles at a time from the world's texels, and laid out afresh where they change. */

#include "../../stdafx.h"
#include "block_scatter.h"

#include "../../map_func.h"
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

void BlockScatter::Sync(const WorldChanges &changes)
{
	this->blocks.Sync(changes);
}

void BlockScatter::Prepare(const SceneView &camera, double shown_pixels, double cast_pixels)
{
	this->blocks.Refresh(camera, shown_pixels, cast_pixels, 0, [&](const TileSpan &tiles, ScatterCopies &copies) {
		copies.assign(this->models, {});
		this->Strew(tiles, copies);
	});
}

void BlockScatter::Gather(const SceneView &view, const Frustum &frustum, double fewest_pixels, VehicleBatch &batch)
{
	this->blocks.ForEachSeen(view, frustum, fewest_pixels, 0, [&](const ScatterCopies &copies) {
		for (size_t model = 0; model < copies.size(); model++) batch.Add(model, copies[model]);
		return true;
	});
}

void BlockScatter::Release()
{
	this->blocks.Release();
}
