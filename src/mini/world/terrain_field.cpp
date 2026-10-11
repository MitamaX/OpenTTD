/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file terrain_field.cpp The map's ground and water surface as meshes, one pair per block of tiles, built as the camera's view asks for them. */

#include "../../stdafx.h"
#include "terrain_field.h"

#include <algorithm>
#include <tuple>

#include "../../map_func.h"
#include "../gpu/frame_profile.h"
#include "../map/way_course.h"
#include "hill_relief.h"
#include "network_field.h"
#include "seabed.h"
#include "shadow_map.h"
#include "water_mesh.h"

#include "../../safeguards.h"

static constexpr double OUTER_SEA_REACH = 8192.0;
/* The ground keeps every tile's own facets and banks wherever the network's meshes may stand on it. */
static constexpr double FINEST_CELL_PIXELS = NETWORK_FADE_START;
static constexpr int COARSEST_STEP = 8;
static constexpr GroundGrain COARSEST_GRAIN = {COARSEST_STEP, 1};
static constexpr double RELIEF_CELL_PIXELS = 32.0;
static constexpr int MOST_RELIEF_DIVISIONS = 2;

/* The longest lattice step whose cells still span no more than a few pixels, so distant ground keeps few triangles;
 * at full detail, curving ground is cut as finely as keeps its cells some pixels across. */
static GroundGrain GrainFor(double tile_pixels)
{
	int step = 1;
	while (step < COARSEST_STEP && tile_pixels * step < FINEST_CELL_PIXELS) step *= 2;
	int divisions = 1;
	while (step == 1 && divisions < MOST_RELIEF_DIVISIONS && tile_pixels / (2 * divisions) >= RELIEF_CELL_PIXELS) divisions *= 2;
	return {step, divisions};
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

	this->grid.ForEachTouched(changes.Of(ChangeKind::Relief), std::max(SHELF_TILES, WAY_EASE_REACH), [&](size_t index) {
		Chunk &chunk = this->chunks[index];
		if (chunk.rebuild.has_value()) chunk.rebuild->outdated = true;
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
	for (int ty = std::max(tiles.ty0 - ROLL_SPREAD, 0); ty <= std::min(tiles.ty1 + ROLL_SPREAD, static_cast<int>(Map::MaxY())); ty++) {
		for (int tx = std::max(tiles.tx0 - ROLL_SPREAD, 0); tx <= std::min(tiles.tx1 + ROLL_SPREAD, static_cast<int>(Map::MaxX())); tx++) {
			SurfaceTexel surface = _world_tiles.SurfaceAt(TileXY(tx, ty));
			chunk.low = std::min(chunk.low, surface.Lowest());
			chunk.high = std::max(chunk.high, surface.Highest());
		}
	}
	chunk.surveyed = true;
}

/* A block's box reaches out over the shelf of seabed beside it where it lies along the map's edge. */
std::pair<Vec3, Vec3> TerrainField::Bounds(size_t index)
{
	Chunk &chunk = this->chunks[index];
	TileSpan tiles = this->grid.TilesOf(index);
	if (!chunk.surveyed) this->Survey(chunk, tiles);

	Dimension map = this->grid.Map();
	auto shelf = [](bool on_edge) { return on_edge ? static_cast<double>(SHELF_TILES) : 0.0; };
	double rise = LevelRise();
	return {
		{tiles.tx0 - shelf(tiles.tx0 == 0), tiles.ty0 - shelf(tiles.ty0 == 0), (chunk.low - DEEPEST_SINK) * rise},
		{tiles.tx1 + 1.0 + shelf(tiles.tx1 + 1 == static_cast<int>(map.width)), tiles.ty1 + 1.0 + shelf(tiles.ty1 + 1 == static_cast<int>(map.height)), chunk.high * rise},
	};
}

double TerrainField::Distance(size_t index, const Vec3 &eye) const
{
	const Chunk &chunk = this->chunks[index];
	TileSpan tiles = this->grid.TilesOf(index);
	Vec3 middle = {(tiles.tx0 + tiles.tx1 + 1) * 0.5, (tiles.ty0 + tiles.ty1 + 1) * 0.5, (chunk.low + chunk.high) * 0.5 * LevelRise()};
	return Length(middle - eye);
}

/* The blocks in the camera's view, and those within the shadows' reach whose shadows may fall in it, are brought to the step the view of their nearest point asks for,
 * and those the world changed under are built afresh. A view with nothing yet to show has every block in it built whole at once; otherwise a block first coming into view
 * is laid at once, the nearest no finer than a draft where that would cost much and those past the slice of the frame at the coarsest step, and the rest are refined
 * within what is left of the slice, which hurries while any in view wait, each keeping its old meshes until its new ones are done. */
void TerrainField::Refresh(const SceneView &camera)
{
	this->nearby.clear();
	this->grid.ForEachAround(camera.eye, camera.Reach() + SHELF_TILES, [&](size_t index) {
		auto [low, high] = this->Bounds(index);
		this->nearby.push_back({index, low, high, this->Distance(index, camera.eye)});
		return true;
	});
	std::ranges::sort(this->nearby, {}, &Nearby::distance);

	double casting_pixels = camera.TilePixelsAt(camera.shadow_reach);
	this->due.clear();
	bool blank = true;
	for (const Nearby &near : this->nearby) {
		bool in_sight = BoxMeets(camera.frustum, near.low, near.high);
		double nearest_pixels = camera.NearestTilePixels(near.low, near.high);
		if (!in_sight && (nearest_pixels < casting_pixels || !this->ShadowShows(near, camera))) continue;
		Chunk &chunk = this->chunks[near.index];
		chunk.drawn = this->frame;
		if (in_sight) blank = blank && chunk.ground.Empty();
		GroundGrain grain = GrainFor(nearest_pixels);
		if (chunk.Outdated(grain)) {
			this->due.push_back({near.index, grain, in_sight});
		} else {
			chunk.rebuild.reset();
			chunk.due_since = 0;
		}
	}

	BuildSlice slice(std::ranges::any_of(this->due, &Due::in_sight));
	for (const Due &entry : this->due) {
		const Chunk &chunk = this->chunks[entry.index];
		if (!entry.in_sight || !chunk.ground.Empty()) continue;
		this->Build(entry.index, blank ? entry.grain : slice.Spent() ? COARSEST_GRAIN : chunk.NextGrain(entry.grain));
	}
	std::erase_if(this->due, [&](const Due &entry) { return !this->chunks[entry.index].Outdated(entry.grain); });
	for (const Due &entry : this->due) {
		uint64_t &since = this->chunks[entry.index].due_since;
		if (since == 0) since = this->frame;
	}
	std::ranges::stable_sort(this->due, {}, [&](const Due &entry) { return std::make_tuple(!entry.in_sight, this->chunks[entry.index].due_since); });
	this->Refine(slice);
}

void TerrainField::Build(size_t index, GroundGrain grain)
{
	Chunk &chunk = this->chunks[index];
	ProfileScope profile("build", "terrain", ProfileClock::Cpu);
	TerrainBuild &build = chunk.rebuild.emplace(TerrainBuild(this->grid.TilesOf(index), grain)).build;
	while (!build.Done()) build.Advance();
	this->Finish(index);
}

/* The blocks in view go first, then those that have waited longest and the nearest of them, so neither the blocks casting shadows into the view
 * nor a block the world keeps changing under can hold up the rest; a build cut short by the end of the slice goes on from where it stopped. */
void TerrainField::Refine(const BuildSlice &slice)
{
	for (const Due &entry : this->due) {
		Chunk &chunk = this->chunks[entry.index];
		ProfileScope profile("build", "terrain", ProfileClock::Cpu);
		GroundGrain grain = chunk.NextGrain(entry.grain);
		if (chunk.rebuild.has_value() && chunk.rebuild->build.Grain() != grain) chunk.rebuild.reset();
		if (!chunk.rebuild.has_value()) chunk.rebuild.emplace(TerrainBuild(this->grid.TilesOf(entry.index), grain));
		if (!slice.Carry(chunk.rebuild->build)) return;
		this->Finish(entry.index);
	}
}

void TerrainField::Finish(size_t index)
{
	Chunk &chunk = this->chunks[index];
	_frame_profile.Count("terrain_builds");
	chunk.ground.Upload(chunk.rebuild->build.Finish(), TERRAIN_LAYOUT);
	this->keep.Hold(index);
	if (chunk.stale) {
		TileSpan tiles = this->grid.TilesOf(index);
		chunk.water.Upload(BuildWaterSurface(tiles), WATER_LAYOUT);
		chunk.sprays = BuildFallSprays(tiles);
	}
	chunk.grain = chunk.rebuild->build.Grain();
	chunk.stale = chunk.rebuild->outdated;
	chunk.due_since = 0;
	chunk.rebuild.reset();
}

/* The nearest ground is drawn first and the seabed beyond the map last, so ground hidden behind hills is rejected before it is shaded. */
void TerrainField::DrawGround(const Frustum &frustum)
{
	if (this->chunks.empty()) return;
	for (const Nearby &near : this->nearby) {
		Chunk &chunk = this->chunks[near.index];
		if (chunk.ground.Empty() || !BoxMeets(frustum, near.low, near.high)) continue;
		chunk.drawn = this->frame;
		chunk.ground.Draw();
	}
	this->outer_bed.Draw();
}

bool TerrainField::Shows(const Nearby &near, const SceneView &camera) const
{
	return !this->chunks[near.index].ground.Empty() && BoxMeets(camera.frustum, near.low, near.high);
}

bool TerrainField::ShadowShows(const Nearby &near, const SceneView &camera) const
{
	auto [low, high] = SunSwept(camera.sun, near.low, near.high);
	return BoxMeets(camera.frustum, low, high);
}

void TerrainField::DrawWater(const SceneView &camera)
{
	if (this->chunks.empty()) return;
	this->outer_water.Draw();
	for (const Nearby &near : this->nearby) {
		if (this->Shows(near, camera)) this->chunks[near.index].water.Draw();
	}
}

/* Past what the field may keep, the blocks longest out of every view give their meshes back, so a big map only holds the ground about the view. */
void TerrainField::Evict()
{
	auto bytes = [](const Chunk &chunk) { return chunk.ground.Bytes() + chunk.water.Bytes(); };
	this->keep.Trim(this->chunks, this->frame, bytes, &Chunk::drawn, [](Chunk &chunk) {
		chunk.ground.Release();
		chunk.water.Release();
		chunk.sprays.clear();
		chunk.rebuild.reset();
		chunk.stale = true;
	});
}

void TerrainField::Release()
{
	for (Chunk &chunk : this->chunks) {
		chunk.ground.Release();
		chunk.water.Release();
	}
	this->chunks.clear();
	this->due.clear();
	this->nearby.clear();
	this->keep.Clear();
	this->outer_bed.Release();
	this->outer_water.Release();
	this->grid.Clear();
}
