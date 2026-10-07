/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file terrain_field.cpp The map's ground and water surface as meshes, one pair per block of tiles, built as views ask for them. */

#include "../../stdafx.h"
#include "terrain_field.h"

#include <algorithm>

#include "../../map_func.h"
#include "../map/way_profile.h"
#include "network_field.h"
#include "seabed.h"
#include "terrain_mesh.h"
#include "water_mesh.h"

#include "../../safeguards.h"

static constexpr double OUTER_SEA_REACH = 8192.0;
/* The ground keeps every tile's own facets and banks wherever the network's meshes may stand on it. */
static constexpr double FINEST_CELL_PIXELS = NETWORK_FADE_START;
static constexpr int COARSEST_STEP = 8;
static constexpr uint64_t EVICT_FRAMES = 600;

/* The longest lattice step whose cells still span no more than a few pixels, so distant ground keeps few triangles. */
static int StepFor(double tile_pixels)
{
	int step = 1;
	while (step < COARSEST_STEP && tile_pixels * step < FINEST_CELL_PIXELS) step *= 2;
	return step;
}

/* A tile whose shape or water changed reshapes the walls, the shading and the seabed of the tiles beside it, and one whose ways changed eases the runs through it afresh,
 * so the blocks around it go stale too. */
void TerrainField::Sync(const WorldChanges &changes)
{
	this->frame++;
	this->Evict();

	Dimension size = _world_tiles.Size();
	if (changes.whole || size != this->grid.Map()) {
		this->Lay(size);
		return;
	}

	this->grid.ForEachTouched(changes.reliefs, std::max(SHELF_TILES, WAY_EASE_REACH), [&](size_t index) {
		Chunk &chunk = this->chunks[index];
		chunk.stale = true;
		chunk.surveyed = false;
	});
}

void TerrainField::Lay(Dimension map)
{
	this->Release();
	this->grid.Lay(map);
	this->chunks = std::vector<Chunk>(this->grid.Count());
	if (this->chunks.empty()) return;

	this->outer_bed.Upload(BuildOuterBed(map, OUTER_SEA_REACH), TERRAIN_LAYOUT);
	this->outer_water.Upload(BuildOuterWater(map, OUTER_SEA_REACH), WATER_LAYOUT);
}

void TerrainField::Survey(Chunk &chunk, const TileSpan &tiles) const
{
	chunk.low = UINT8_MAX;
	chunk.high = 0;
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			SurfaceTexel surface = _world_tiles.SurfaceAt(TileXY(tx, ty));
			chunk.low = std::min({chunk.low, surface.north, surface.west, surface.east, surface.south});
			chunk.high = std::max({chunk.high, surface.north, surface.west, surface.east, surface.south});
		}
	}
	chunk.surveyed = true;
}

/* A block out of the frustum is left alone; one inside is built at the step the camera's view of its nearest point asks for, whichever view draws it. */
TerrainField::Chunk *TerrainField::Prepare(size_t index, const SceneView &camera, const Frustum &frustum)
{
	Chunk &chunk = this->chunks[index];
	TileSpan tiles = this->grid.TilesOf(index);
	if (!chunk.surveyed) this->Survey(chunk, tiles);

	double rise = LevelRise();
	Vec3 low = {static_cast<double>(tiles.tx0), static_cast<double>(tiles.ty0), (chunk.low - DEEPEST_SINK) * rise};
	Vec3 high = {tiles.tx1 + 1.0, tiles.ty1 + 1.0, chunk.high * rise};
	if (!BoxMeets(frustum, low, high)) return nullptr;

	int step = StepFor(camera.NearestTilePixels(low, high));
	if (chunk.stale || chunk.step != step) {
		chunk.ground.Upload(BuildTerrain(tiles, step), TERRAIN_LAYOUT);
		if (chunk.stale) chunk.water.Upload(BuildWaterSurface(tiles), WATER_LAYOUT);
		chunk.step = step;
		chunk.stale = false;
	}
	chunk.drawn = this->frame;
	return &chunk;
}

void TerrainField::DrawGround(const SceneView &camera, const Frustum &frustum)
{
	if (this->chunks.empty()) return;
	this->outer_bed.Draw();
	for (size_t index = 0; index < this->chunks.size(); index++) {
		if (Chunk *chunk = this->Prepare(index, camera, frustum); chunk != nullptr) chunk->ground.Draw();
	}
}

void TerrainField::DrawWater(const SceneView &camera)
{
	if (this->chunks.empty()) return;
	this->outer_water.Draw();
	for (size_t index = 0; index < this->chunks.size(); index++) {
		if (Chunk *chunk = this->Prepare(index, camera, camera.frustum); chunk != nullptr) chunk->water.Draw();
	}
}

/* A block long out of every view gives its meshes back, so a big map only holds the ground near the view. */
void TerrainField::Evict()
{
	for (Chunk &chunk : this->chunks) {
		if (chunk.ground.Empty() || this->frame - chunk.drawn < EVICT_FRAMES) continue;
		chunk.ground.Release();
		chunk.water.Release();
		chunk.stale = true;
	}
}

void TerrainField::Release()
{
	for (Chunk &chunk : this->chunks) {
		chunk.ground.Release();
		chunk.water.Release();
	}
	this->chunks.clear();
	this->outer_bed.Release();
	this->outer_water.Release();
	this->grid.Clear();
}
