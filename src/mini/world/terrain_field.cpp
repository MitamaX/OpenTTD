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
#include "sea_banks.h"
#include "seabed.h"
#include "shadow_map.h"
#include "water_mesh.h"

#include "../../safeguards.h"

static constexpr double OUTER_SEA_REACH = 8192.0;
/* The ground keeps every tile's own facets and banks wherever the network's meshes may stand on it. */
static constexpr double FINEST_CELL_PIXELS = NETWORK_FADE_START;
static constexpr double RESTING_EYE_STEP = 0.01;
static constexpr int COARSEST_STEP = 8;
static constexpr GroundGrain COARSEST_GRAIN = {COARSEST_STEP, {}};

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
	int spread = std::max(ROLL_SPREAD, SEA_BANK_REACH);
	for (int ty = std::max(tiles.ty0 - spread, 0); ty <= std::min(tiles.ty1 + spread, static_cast<int>(Map::MaxY())); ty++) {
		for (int tx = std::max(tiles.tx0 - spread, 0); tx <= std::min(tiles.tx1 + spread, static_cast<int>(Map::MaxX())); tx++) {
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
		{tiles.tx1 + 1.0 + shelf(tiles.tx1 + 1 == static_cast<int>(map.width)), tiles.ty1 + 1.0 + shelf(tiles.ty1 + 1 == static_cast<int>(map.height)), (chunk.high + CREST_WAVE) * rise},
	};
}

/* At full detail, curving ground is cut finely on each tile near enough for its cells to stay some pixels across. */
GroundGrain TerrainField::GrainOf(const Nearby &near, const SceneView &camera, double nearest_pixels) const
{
	int step = StepFor(nearest_pixels);
	if (step > 1 || nearest_pixels < this->KeptPixels()) return {step, {}};
	return {step, this->FineTiles(near, camera, nearest_pixels)};
}

double TerrainField::KeptPixels() const
{
	return this->moving ? FINE_RELIEF_MOVING_KEPT_PIXELS : FINE_RELIEF_KEPT_PIXELS;
}

BlockTiles TerrainField::FineTiles(const Nearby &near, const SceneView &camera, double nearest_pixels) const
{
	if (camera.FarthestTilePixels(near.low, near.high) >= FINE_RELIEF_PIXELS) return BlockTiles().set();
	GroundGrain laid = this->chunks[near.index].LatestGrain();
	bool none_chosen = nearest_pixels < FINE_RELIEF_CHOSEN_PIXELS;
	if (none_chosen && laid.fine.none()) return {};

	double kept_pixels = this->KeptPixels();
	TileSpan tiles = this->grid.TilesOf(near.index);
	BlockTiles needed;
	BlockTiles chosen;
	BlockTiles kept;
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			double pixels = camera.NearestTilePixels({static_cast<double>(tx), static_cast<double>(ty), near.low.z}, {tx + 1.0, ty + 1.0, near.high.z});
			size_t bit = BlockBit(tiles, tx, ty);
			needed[bit] = pixels >= FINE_RELIEF_PIXELS;
			chosen[bit] = pixels >= FINE_RELIEF_CHOSEN_PIXELS;
			kept[bit] = pixels >= kept_pixels;
		}
	}
	bool serves = laid.step == 1 && (needed & ~laid.fine).none() && (laid.fine & ~kept).none();
	return serves ? laid.fine : chosen;
}

double TerrainField::Distance(size_t index, const Vec3 &eye) const
{
	const Chunk &chunk = this->chunks[index];
	TileSpan tiles = this->grid.TilesOf(index);
	Vec3 middle = {(tiles.tx0 + tiles.tx1 + 1) * 0.5, (tiles.ty0 + tiles.ty1 + 1) * 0.5, (chunk.low + chunk.high) * 0.5 * LevelRise()};
	return Length(middle - eye);
}

void TerrainField::GatherNearby(const SceneView &camera)
{
	this->moving = Length(camera.eye - this->last_eye) > RESTING_EYE_STEP;
	this->last_eye = camera.eye;
	this->nearby.clear();
	this->grid.ForEachAround(camera.eye, camera.Reach() + SHELF_TILES, [&](size_t index) {
		auto [low, high] = this->Bounds(index);
		this->nearby.push_back({index, low, high, this->Distance(index, camera.eye)});
		return true;
	});
	std::ranges::sort(this->nearby, {}, &Nearby::distance);
}

void TerrainField::QueueDue(const SceneView &camera)
{
	double casting_pixels = camera.TilePixelsAt(camera.shadow_reach);
	this->due.clear();
	for (const Nearby &near : this->nearby) {
		bool in_sight = BoxMeets(camera.frustum, near.low, near.high);
		double nearest_pixels = camera.NearestTilePixels(near.low, near.high);
		if (!in_sight && (nearest_pixels < casting_pixels || !this->ShadowShows(near, camera))) continue;
		Chunk &chunk = this->chunks[near.index];
		chunk.drawn = this->frame;
		GroundGrain grain = this->GrainOf(near, camera, nearest_pixels);
		if (chunk.Outdated(grain)) {
			this->due.push_back({near.index, grain, in_sight});
		} else {
			chunk.rebuild.reset();
			chunk.due_since = 0;
		}
	}
}

bool TerrainField::ViewBlank(const SceneView &camera) const
{
	return std::ranges::none_of(this->nearby, [&](const Nearby &near) { return this->Shows(near, camera); });
}

GroundGrain TerrainField::FirstSightGrain(const Due &entry, bool blank, const BuildSlice &slice) const
{
	if (blank) return entry.grain;
	return slice.Spent() ? COARSEST_GRAIN : this->chunks[entry.index].NextGrain(entry.grain);
}

/* Brings the blocks the views show, and those casting shadows into them, to the step they are due. */
void TerrainField::Refresh(const SceneView &camera)
{
	this->GatherNearby(camera);
	this->QueueDue(camera);

	BuildSlice slice(std::ranges::any_of(this->due, &Due::in_sight));
	bool blank = this->ViewBlank(camera);
	for (const Due &entry : this->due) {
		if (!entry.in_sight || !this->chunks[entry.index].ground.Empty()) continue;
		this->Build(entry.index, this->FirstSightGrain(entry, blank, slice));
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

/* Builds the due blocks in order of need within what is left of the slice. */
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
void TerrainField::DrawGround(const Sight &sight)
{
	if (this->chunks.empty()) return;
	for (const Nearby &near : this->nearby) {
		Chunk &chunk = this->chunks[near.index];
		if (chunk.ground.Empty() || !sight.Sees(near.low, near.high)) continue;
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
	return ShadowMeets(camera.frustum, camera.sun, near.low, near.high);
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
