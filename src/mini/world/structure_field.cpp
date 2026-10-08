/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file structure_field.cpp The map's buildings as one mesh per block of tiles, built at the detail views ask for and rebuilt where a form changed. */

#include "../../stdafx.h"
#include "structure_field.h"

#include <algorithm>
#include <bit>
#include <ranges>
#include <tuple>
#include <utility>

#include "../../map_func.h"
#include "../../station_map.h"
#include "../../tile_map.h"
#include "../core/seed.h"
#include "../gpu/frame_profile.h"
#include "../map/structure_forms.h"

#include "../../safeguards.h"

static constexpr double FULL_DETAIL_PIXELS = 20.0;
static constexpr double FOOTING_LEVELS = 1.0;
static constexpr uint64_t EVICT_FRAMES = 600;
static constexpr int NEIGHBOUR_REACH = 1;
static constexpr int FOOTPRINT_REACH = MAX_FOOTPRINT_TILES - 1;

static MiniLayer LayerOf(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_RAILWAY: return MiniLayer::Rail;
		case MP_ROAD: return MiniLayer::Road;
		case MP_STATION: return IsAnyRoadStop(tile) ? MiniLayer::Road : MiniLayer::None;
		default: return MiniLayer::None;
	}
}

static uint32_t Mixed(uint32_t digest, uint32_t value)
{
	return Hash32(digest ^ value);
}

static uint32_t Mixed(uint32_t digest, float value)
{
	return Mixed(digest, std::bit_cast<uint32_t>(value));
}

static uint32_t SolidDigest(uint32_t digest, const Solid &solid)
{
	for (float value : {solid.x0, solid.y0, solid.x1, solid.y1, solid.base, solid.wall, solid.rise, solid.taper}) digest = Mixed(digest, value);
	uint32_t kinds = to_underlying(solid.kind) | to_underlying(solid.role) << 4 | to_underlying(solid.fixture) << 8 | to_underlying(solid.roof) << 12 | solid.ridge << 16 | solid.high_side << 20 | solid.fronts.base() << 24;
	uint32_t claddings = to_underlying(solid.wall_material) | to_underlying(solid.windows) << 8 | to_underlying(solid.roof_material) << 16;
	for (uint32_t value : {kinds, claddings, solid.wall_tint, solid.roof_tint, solid.glass_tint}) digest = Mixed(digest, value);
	return digest;
}

/* What a tile stands as, when the tile anchors a form; a tile anchoring none, or only a part of a form anchored elsewhere, has none. */
static std::optional<BuildingForm> AnchoredForm(TileIndex tile)
{
	std::optional<BuildingForm> form = StructureForm(tile);
	if (!form.has_value() || TileXY(form->tx, form->ty) != tile) return std::nullopt;
	return form;
}

static uint32_t DigestOf(const std::optional<BuildingForm> &form)
{
	if (!form.has_value()) return 0;
	uint32_t digest = Mixed(Mixed(Hash32(form->size_x | form->size_y << 8), form->floor), static_cast<uint32_t>(form->count));
	for (const Solid &solid : form->Solids()) digest = SolidDigest(digest, solid);
	return digest;
}

void StructureField::Sync(const WorldChanges &changes)
{
	this->frame++;
	this->Evict();

	Dimension size = _world_tiles.Size();
	if (changes.whole || size != this->grid.Map() || LevelRise() != this->rise) this->Lay(size);
}

void StructureField::Lay(Dimension map)
{
	this->Release();
	this->grid.Lay(map);
	this->chunks = std::vector<StructureChunk>(this->grid.Count());
	this->digests.assign(static_cast<size_t>(map.width) * map.height, 0);
	this->rise = LevelRise();
}

/* A marked tile may change the forms of the tiles around it too: which way a house faces, or which sides an industry's roofs join. */
void StructureField::Notice(TileIndex tile)
{
	_frame_profile.Count("structure_notices");
	Dimension map = this->grid.Map();
	int x = TileX(tile);
	int y = TileY(tile);
	for (int ty = std::max(y - NEIGHBOUR_REACH, 0); ty <= std::min<int>(y + NEIGHBOUR_REACH, map.height - 1); ty++) {
		for (int tx = std::max(x - NEIGHBOUR_REACH, 0); tx <= std::min<int>(x + NEIGHBOUR_REACH, map.width - 1); tx++) {
			TileIndex near = TileXY(tx, ty);
			uint32_t digest = DigestOf(AnchoredForm(near));
			if (digest == this->digests[near.base()]) continue;
			this->digests[near.base()] = digest;
			StructureChunk &chunk = this->chunks[this->grid.IndexOf(tx, ty)];
			if (chunk.rebuild.has_value()) chunk.rebuild->outdated = true;
			chunk.stale = true;
		}
	}
}

/* Until it is built, a block is guessed to reach from below its lowest ground to the tallest building above its highest. */
void StructureField::Survey(StructureChunk &chunk, size_t index) const
{
	TileSpan tiles = this->grid.TilesOf(index);
	uint low = _world_tiles.Peak();
	uint high = 0;
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			SurfaceTexel surface = _world_tiles.SurfaceAt(TileXY(tx, ty));
			low = std::min<uint>({low, surface.north, surface.west, surface.east, surface.south});
			high = std::max<uint>({high, surface.north, surface.west, surface.east, surface.south});
		}
	}
	chunk.low = {static_cast<double>(tiles.tx0), static_cast<double>(tiles.ty0), (low - FOOTING_LEVELS) * this->rise};
	chunk.high = {static_cast<double>(tiles.tx1 + 1 + FOOTPRINT_REACH), static_cast<double>(tiles.ty1 + 1 + FOOTPRINT_REACH), high * this->rise + MAX_STRUCTURE_TILES * FORM_HEIGHT_SCALE};
	chunk.surveyed = true;
}

StructureBuild::StructureBuild(const TileSpan &tiles, StructureDetail detail, std::span<uint32_t> digests) :
	tiles(tiles), detail(detail), digests(digests), next_tx(tiles.tx0), next_ty(tiles.ty0)
{
}

void StructureBuild::Advance()
{
	TileIndex tile = TileXY(this->next_tx, this->next_ty);
	std::optional<BuildingForm> form = AnchoredForm(tile);
	this->digests[tile.base()] = DigestOf(form);
	if (form.has_value()) BuildStructure(*form, this->detail, LayerOf(tile), this->parts);
	if (++this->next_tx <= this->tiles.tx1) return;
	this->next_tx = this->tiles.tx0;
	this->next_ty++;
}

StructureParts StructureBuild::Finish()
{
	return std::move(this->parts);
}

/* A block is built once the camera comes near enough to show its buildings, at the detail its nearest point asks for, and built afresh where a form changed in it.
 * The blocks are built within a slice of each frame, each showing what it was last built as until its new mesh is done. */
void StructureField::Prepare(const SceneView &camera)
{
	if (this->chunks.empty()) return;
	for (TileIndex tile : _world_tiles.Touched()) this->Notice(tile);

	double casting_pixels = camera.TilePixelsAt(camera.shadow_reach);
	this->due.clear();
	for (size_t index = 0; index < this->chunks.size(); index++) {
		StructureChunk &chunk = this->chunks[index];
		if (!chunk.surveyed) this->Survey(chunk, index);
		bool in_sight = BoxMeets(camera.frustum, chunk.low, chunk.high);
		double nearest_pixels = camera.NearestTilePixels(chunk.low, chunk.high);
		if (nearest_pixels < (in_sight ? STRUCTURE_FADE_START : casting_pixels)) continue;
		chunk.wanted = this->frame;
		StructureDetail detail = nearest_pixels >= FULL_DETAIL_PIXELS ? StructureDetail::Full : StructureDetail::Simple;
		if (chunk.Outdated(detail)) {
			this->due.push_back({index, in_sight, nearest_pixels, detail});
			if (chunk.due_since == 0) chunk.due_since = this->frame;
		} else {
			chunk.rebuild.reset();
			chunk.due_since = 0;
		}
	}
	this->Refine();
}

/* The blocks in sight go first, then those still showing none of their buildings, then those that have waited longest and the nearest of them,
 * so neither the blocks casting shadows into the view nor a block the world keeps changing under can hold up the rest; a build cut short by the end of the slice goes on from where it stopped. */
void StructureField::Refine()
{
	std::ranges::sort(this->due, {}, [&](const Due &entry) {
		const StructureChunk &chunk = this->chunks[entry.index];
		return std::make_tuple(!entry.in_sight, chunk.built, chunk.due_since, -entry.pixels);
	});
	BuildSlice slice;
	for (const Due &entry : this->due) {
		StructureChunk &chunk = this->chunks[entry.index];
		ProfileScope profile("build", "structures", ProfileClock::Cpu);
		StructureDetail detail = chunk.NextDetail(entry.detail);
		if (chunk.rebuild.has_value() && chunk.rebuild->build.Detail() != detail) chunk.rebuild.reset();
		if (!chunk.rebuild.has_value()) chunk.rebuild.emplace(StructureBuild(this->grid.TilesOf(entry.index), detail, this->digests));
		if (!slice.Carry(chunk.rebuild->build)) return;
		this->Finish(chunk);
	}
}

/* The block's box shrinks to the mesh once it is built, so culling and detail go by what really stands there. */
void StructureField::Finish(StructureChunk &chunk)
{
	_frame_profile.Count("structure_builds");
	StructureParts parts = chunk.rebuild->build.Finish();
	StructureMesh &mesh = parts.mesh;
	if (!mesh.vertices.empty()) {
		auto [x0, x1] = std::ranges::minmax(mesh.vertices | std::views::transform([](const StructureVertex &vertex) { return vertex.model.x; }));
		auto [y0, y1] = std::ranges::minmax(mesh.vertices | std::views::transform([](const StructureVertex &vertex) { return vertex.model.y; }));
		auto [z0, z1] = std::ranges::minmax(mesh.vertices | std::views::transform([](const StructureVertex &vertex) { return vertex.model.z; }));
		chunk.low = {x0, y0, z0};
		chunk.high = {x1, y1, z1};
	}
	chunk.waiting = std::move(mesh);
	chunk.picks = std::move(parts.picks);
	chunk.vents = std::move(parts.vents);
	chunk.detail = chunk.rebuild->build.Detail();
	chunk.built = true;
	chunk.stale = chunk.rebuild->outdated;
	chunk.due_since = 0;
	chunk.rebuild.reset();
}

/* Blocks built since the last frame are handed to the GPU before any view draws them. */
void StructureField::Gather(const SceneView &camera, const Frustum &frustum, std::vector<const StructureChunk *> &shown)
{
	shown.clear();
	for (StructureChunk &chunk : this->chunks) {
		if (chunk.waiting.has_value()) {
			if (chunk.waiting->indices.empty()) {
				chunk.mesh.Release();
			} else {
				chunk.mesh.Upload(*chunk.waiting, STRUCTURE_LAYOUT);
			}
			chunk.waiting.reset();
		}
		if (!chunk.built || chunk.mesh.Empty()) continue;
		if (BoxMeets(frustum, chunk.low, chunk.high) && camera.NearestTilePixels(chunk.low, chunk.high) >= STRUCTURE_FADE_START) shown.push_back(&chunk);
	}
}

std::optional<StructureHit> StructureField::Pick(const Vec3 &origin, const Vec3 &direction) const
{
	std::optional<StructureHit> nearest;
	auto nearer = [&nearest](double distance) { return !nearest.has_value() || distance < nearest->distance; };
	for (const StructureChunk &chunk : this->chunks) {
		if (!chunk.built) continue;
		std::optional<double> chunk_entry = BoxEntry(origin, direction, chunk.low, chunk.high);
		if (!chunk_entry.has_value() || !nearer(*chunk_entry)) continue;
		for (const StructurePick &pick : chunk.picks) {
			std::optional<double> entry = BoxEntry(origin, direction, pick.low, pick.high);
			if (!entry.has_value() || !nearer(*entry)) continue;
			Vec3 at = origin + direction * *entry;
			int tx = std::clamp(static_cast<int>(std::floor(at.x)), pick.tiles.tx0, pick.tiles.tx1);
			int ty = std::clamp(static_cast<int>(std::floor(at.y)), pick.tiles.ty0, pick.tiles.ty1);
			nearest = StructureHit{TileXY(tx, ty), *entry};
		}
	}
	return nearest;
}

/* A block the camera has long been too far from gives its mesh back and drops any build of it left unfinished, so a big map only holds the buildings near the view. */
void StructureField::Evict()
{
	for (StructureChunk &chunk : this->chunks) {
		if (this->frame - chunk.wanted < EVICT_FRAMES) continue;
		chunk.rebuild.reset();
		chunk.due_since = 0;
		if (!chunk.built) continue;
		chunk.mesh.Release();
		chunk.waiting.reset();
		chunk.picks.clear();
		chunk.vents.clear();
		chunk.built = false;
		chunk.surveyed = false;
	}
}

void StructureField::Release()
{
	for (StructureChunk &chunk : this->chunks) chunk.mesh.Release();
	this->chunks.clear();
	this->digests.clear();
	this->due.clear();
	this->grid.Clear();
}
