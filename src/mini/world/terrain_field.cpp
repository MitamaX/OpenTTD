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
#include <chrono>

#include "../../map_func.h"
#include "../gpu/frame_profile.h"
#include "../map/way_course.h"
#include "network_field.h"
#include "seabed.h"
#include "water_mesh.h"

#include "../../safeguards.h"

static constexpr double OUTER_SEA_REACH = 8192.0;
/* The ground keeps every tile's own facets and banks wherever the network's meshes may stand on it. */
static constexpr double FINEST_CELL_PIXELS = NETWORK_FADE_START;
static constexpr int COARSEST_STEP = 8;
static constexpr uint64_t EVICT_FRAMES = 600;
static constexpr std::chrono::microseconds REFINE_BUDGET{1500};

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
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			SurfaceTexel surface = _world_tiles.SurfaceAt(TileXY(tx, ty));
			chunk.low = std::min({chunk.low, surface.north, surface.west, surface.east, surface.south});
			chunk.high = std::max({chunk.high, surface.north, surface.west, surface.east, surface.south});
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

/* The blocks in the camera's view are brought to the step its view of their nearest point asks for, and those the world changed under are built afresh.
 * A view with nothing yet to show has every block built whole at once; otherwise a block first coming into view is laid at once, no finer than a draft where that would cost much,
 * and the rest are refined within a slice of each frame, each keeping its old meshes until its new ones are done. Shadows only draw the blocks there are. */
void TerrainField::Refresh(const SceneView &camera)
{
	this->due.clear();
	bool blank = true;
	for (size_t index = 0; index < this->chunks.size(); index++) {
		auto [low, high] = this->Bounds(index);
		if (!BoxMeets(camera.frustum, low, high)) continue;
		Chunk &chunk = this->chunks[index];
		chunk.drawn = this->frame;
		blank = blank && chunk.ground.Empty();
		int step = StepFor(camera.NearestTilePixels(low, high));
		if (chunk.Outdated(step)) {
			this->due.push_back({index, step, this->Distance(index, camera.eye)});
		} else {
			chunk.rebuild.reset();
			chunk.due_since = 0;
		}
	}

	for (const Due &entry : this->due) {
		const Chunk &chunk = this->chunks[entry.index];
		if (chunk.ground.Empty()) this->Build(entry.index, blank ? entry.step : chunk.NextStep(entry.step));
	}
	std::erase_if(this->due, [&](const Due &entry) { return !this->chunks[entry.index].Outdated(entry.step); });
	for (const Due &entry : this->due) {
		uint64_t &since = this->chunks[entry.index].due_since;
		if (since == 0) since = this->frame;
	}
	std::ranges::sort(this->due, {}, [&](const Due &entry) { return std::make_pair(this->chunks[entry.index].due_since, entry.distance); });
	this->Refine();
}

void TerrainField::Build(size_t index, int step)
{
	Chunk &chunk = this->chunks[index];
	ProfileScope profile("build", "terrain", ProfileClock::Cpu);
	TerrainBuild &build = chunk.rebuild.emplace(TerrainBuild(this->grid.TilesOf(index), step)).build;
	while (!build.Done()) build.Advance();
	this->Finish(index);
}

/* The blocks that have waited longest go first, the nearest of them first, so a block the world keeps changing under cannot hold up the rest;
 * a build cut short by the end of the slice goes on from where it stopped. */
void TerrainField::Refine()
{
	using Clock = std::chrono::steady_clock;
	Clock::time_point deadline = Clock::now() + REFINE_BUDGET;
	for (const Due &entry : this->due) {
		Chunk &chunk = this->chunks[entry.index];
		ProfileScope profile("build", "terrain", ProfileClock::Cpu);
		int step = chunk.NextStep(entry.step);
		if (chunk.rebuild.has_value() && chunk.rebuild->build.Step() != step) chunk.rebuild.reset();
		if (!chunk.rebuild.has_value()) chunk.rebuild.emplace(TerrainBuild(this->grid.TilesOf(entry.index), step));
		TerrainBuild &build = chunk.rebuild->build;
		while (!build.Done()) {
			if (Clock::now() >= deadline) return;
			build.Advance();
		}
		this->Finish(entry.index);
	}
}

void TerrainField::Finish(size_t index)
{
	Chunk &chunk = this->chunks[index];
	_frame_profile.Count("terrain_builds");
	chunk.ground.Upload(chunk.rebuild->build.Finish(), TERRAIN_LAYOUT);
	if (chunk.stale) chunk.water.Upload(BuildWaterSurface(this->grid.TilesOf(index)), WATER_LAYOUT);
	chunk.step = chunk.rebuild->build.Step();
	chunk.stale = chunk.rebuild->outdated;
	chunk.due_since = 0;
	chunk.rebuild.reset();
}

/* The nearest ground is drawn first and the seabed beyond the map last, so ground hidden behind hills is rejected before it is shaded. */
void TerrainField::DrawGround(const SceneView &camera, const Frustum &frustum)
{
	if (this->chunks.empty()) return;
	this->shown.clear();
	for (size_t index = 0; index < this->chunks.size(); index++) {
		Chunk &chunk = this->chunks[index];
		if (chunk.ground.Empty()) continue;
		auto [low, high] = this->Bounds(index);
		if (!BoxMeets(frustum, low, high)) continue;
		chunk.drawn = this->frame;
		this->shown.push_back({&chunk, this->Distance(index, camera.eye)});
	}
	std::ranges::sort(this->shown, {}, &Shown::distance);
	for (const Shown &entry : this->shown) entry.chunk->ground.Draw();
	this->outer_bed.Draw();
}

void TerrainField::DrawWater(const SceneView &camera)
{
	if (this->chunks.empty()) return;
	this->outer_water.Draw();
	for (size_t index = 0; index < this->chunks.size(); index++) {
		Chunk &chunk = this->chunks[index];
		if (chunk.ground.Empty()) continue;
		auto [low, high] = this->Bounds(index);
		if (BoxMeets(camera.frustum, low, high)) chunk.water.Draw();
	}
}

/* A block long out of every view gives its meshes back, so a big map only holds the ground near the view. */
void TerrainField::Evict()
{
	for (Chunk &chunk : this->chunks) {
		if (chunk.ground.Empty() || this->frame - chunk.drawn < EVICT_FRAMES) continue;
		chunk.ground.Release();
		chunk.water.Release();
		chunk.rebuild.reset();
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
	this->due.clear();
	this->outer_bed.Release();
	this->outer_water.Release();
	this->grid.Clear();
}
