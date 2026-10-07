/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file network_field.cpp The map's rails, roads, bridges and stations as meshes, one set per block of tiles, built at the detail views ask for. */

#include "../../stdafx.h"
#include "network_field.h"

#include <algorithm>
#include <bit>
#include <tuple>

#include "../../bridge_map.h"
#include "../../map_func.h"
#include "../../tile_map.h"
#include "../core/seed.h"
#include "../gpu/frame_profile.h"
#include "../map/way_course.h"
#include "seabed.h"

#include "../../safeguards.h"

static constexpr double FULL_DETAIL_PIXELS = 18.0;
static constexpr double HEADROOM_LEVELS = 2.0;
static constexpr uint64_t EVICT_FRAMES = 600;
static constexpr int EASE_MARGIN = WAY_EASE_REACH;

/* A change reaching a block only stales it when the ground or the ways it is built from changed, in it or near enough for the runs through it to ease differently. */
void NetworkField::Sync(const WorldChanges &changes)
{
	this->frame++;
	this->Evict();

	Dimension size = _world_tiles.Size();
	if (changes.whole || size != this->grid.Map() || LevelRise() != this->rise) {
		this->Lay(size);
		return;
	}
	this->grid.ForEachTouched(changes.areas, EASE_MARGIN, [&](size_t index) {
		NetworkChunk &chunk = this->chunks[index];
		uint32_t digest = this->Digest(index);
		if (digest == chunk.digest) return;
		chunk.digest = digest;
		if (chunk.rebuild.has_value()) chunk.rebuild->outdated = true;
		chunk.stale = true;
		chunk.surveyed = false;
	});
}

void NetworkField::Lay(Dimension map)
{
	this->Release();
	this->grid.Lay(map);
	this->chunks = std::vector<NetworkChunk>(this->grid.Count());
	for (size_t index = 0; index < this->chunks.size(); index++) this->chunks[index].digest = this->Digest(index);
	this->rise = LevelRise();
}

uint32_t NetworkField::Digest(size_t index) const
{
	TileSpan tiles = this->grid.TilesOf(index);
	uint32_t digest = 0;
	auto mix = [&digest](const auto &texel) { digest = Hash32(digest ^ std::bit_cast<uint32_t>(texel)); };
	for (int ty = std::max(tiles.ty0 - EASE_MARGIN, 0); ty <= std::min<int>(tiles.ty1 + EASE_MARGIN, this->grid.Map().height - 1); ty++) {
		for (int tx = std::max(tiles.tx0 - EASE_MARGIN, 0); tx <= std::min<int>(tiles.tx1 + EASE_MARGIN, this->grid.Map().width - 1); tx++) {
			TileIndex tile = TileXY(tx, ty);
			mix(_world_tiles.SurfaceAt(tile));
			mix(_world_tiles.NetworkAt(tile));
			digest = Hash32(digest ^ static_cast<uint32_t>(_world_tiles.WaysEase(tile)));
		}
	}
	return digest;
}

/* A bridge's deck may stand well above the ground of the block it crosses. */
void NetworkField::Survey(NetworkChunk &chunk, const TileSpan &tiles) const
{
	chunk.low = _world_tiles.Peak();
	chunk.high = 0;
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			TileIndex tile = TileXY(tx, ty);
			SurfaceTexel surface = _world_tiles.SurfaceAt(tile);
			chunk.low = std::min<uint>({chunk.low, surface.north, surface.west, surface.east, surface.south});
			chunk.high = std::max<uint>({chunk.high, surface.north, surface.west, surface.east, surface.south});
			if (IsBridgeAbove(tile)) chunk.high = std::max<uint>(chunk.high, GetBridgeHeight(GetSouthernBridgeEnd(tile)));
		}
	}
	chunk.surveyed = true;
}

void NetworkField::Build(size_t index, WayDetail detail)
{
	NetworkChunk &chunk = this->chunks[index];
	ProfileScope profile("build", "network", ProfileClock::Cpu);
	NetworkBuild &build = chunk.rebuild.emplace(NetworkBuild(this->grid.TilesOf(index), detail)).build;
	while (!build.Done()) build.Advance();
	this->Finish(chunk);
}

/* The blocks in sight go first, then those still showing none of their ways, then those that have waited longest and the nearest of them,
 * so a block the world keeps changing under cannot hold up the rest; a build cut short by the end of the slice goes on from where it stopped. */
void NetworkField::Refine()
{
	std::ranges::sort(this->due, {}, [&](const Due &entry) {
		const NetworkChunk &chunk = this->chunks[entry.index];
		return std::make_tuple(!entry.in_sight, chunk.built, chunk.due_since, -chunk.nearest_pixels);
	});
	BuildSlice slice;
	for (const Due &entry : this->due) {
		NetworkChunk &chunk = this->chunks[entry.index];
		ProfileScope profile("build", "network", ProfileClock::Cpu);
		WayDetail detail = chunk.NextDetail(entry.detail);
		if (chunk.rebuild.has_value() && chunk.rebuild->build.Detail() != detail) chunk.rebuild.reset();
		if (!chunk.rebuild.has_value()) chunk.rebuild.emplace(NetworkBuild(this->grid.TilesOf(entry.index), detail));
		if (!slice.Carry(chunk.rebuild->build)) return;
		this->Finish(chunk);
	}
}

void NetworkField::Finish(NetworkChunk &chunk)
{
	_frame_profile.Count("network_builds");
	chunk.waiting = chunk.rebuild->build.Finish();
	chunk.signals = std::move(chunk.waiting->signals);
	chunk.detail = chunk.rebuild->build.Detail();
	chunk.built = true;
	chunk.stale = chunk.rebuild->outdated;
	chunk.due_since = 0;
	chunk.rebuild.reset();
}

std::pair<Vec3, Vec3> NetworkField::Bounds(const NetworkChunk &chunk, size_t index) const
{
	TileSpan tiles = this->grid.TilesOf(index);
	return {
		{static_cast<double>(tiles.tx0), static_cast<double>(tiles.ty0), (chunk.low - DEEPEST_SINK) * this->rise},
		{tiles.tx1 + 1.0, tiles.ty1 + 1.0, (chunk.high + HEADROOM_LEVELS) * this->rise},
	};
}

/* A block is built once the camera comes near enough to show its ways, at the detail its nearest point asks for, and built afresh when the world changes under it.
 * A view with no ways yet to show has the blocks in sight built whole at once; otherwise the blocks are built within a slice of each frame,
 * each showing what it was last built as, or the ground's bands, until its new meshes are done. */
void NetworkField::Prepare(const SceneView &camera, std::vector<const NetworkChunk *> &seen)
{
	seen.clear();
	this->due.clear();
	bool blank = true;
	for (size_t index = 0; index < this->chunks.size(); index++) {
		NetworkChunk &chunk = this->chunks[index];
		TileSpan tiles = this->grid.TilesOf(index);
		if (!chunk.surveyed) this->Survey(chunk, tiles);
		auto [low, high] = this->Bounds(chunk, index);
		chunk.nearest_pixels = camera.NearestTilePixels(low, high);
		if (chunk.nearest_pixels < NETWORK_FADE_START) continue;
		chunk.wanted = this->frame;
		bool in_sight = BoxMeets(camera.frustum, low, high);
		if (in_sight) {
			blank = blank && !chunk.built;
			seen.push_back(&chunk);
		}
		WayDetail detail = chunk.nearest_pixels >= FULL_DETAIL_PIXELS ? WayDetail::Full : WayDetail::Simple;
		if (chunk.Outdated(detail)) {
			this->due.push_back({index, detail, in_sight});
			if (chunk.due_since == 0) chunk.due_since = this->frame;
		} else {
			chunk.rebuild.reset();
			chunk.due_since = 0;
		}
	}

	if (blank) {
		for (const Due &entry : this->due) {
			if (entry.in_sight) this->Build(entry.index, entry.detail);
		}
		std::erase_if(this->due, [](const Due &entry) { return entry.in_sight; });
	}
	this->Refine();
	std::erase_if(seen, [](const NetworkChunk *chunk) { return !chunk->built; });
}

template <class Vertex>
static void Hand(MeshBuffer &buffer, const TriangleList<Vertex> &mesh, std::span<const VertexAttribute> layout)
{
	if (mesh.indices.empty()) {
		buffer.Release();
	} else {
		buffer.Upload(mesh, layout);
	}
}

/* Blocks built since the last frame are handed to the GPU before any view draws them. */
void NetworkField::Gather(const SceneView &camera, const Frustum &frustum, std::vector<const NetworkChunk *> &shown)
{
	shown.clear();
	for (size_t index = 0; index < this->chunks.size(); index++) {
		NetworkChunk &chunk = this->chunks[index];
		if (chunk.waiting.has_value()) {
			for (size_t layer = 0; layer < NETWORK_LAYERS; layer++) {
				Hand(chunk.layers[layer], chunk.waiting->layers[layer], MODEL_LAYOUT);
				Hand(chunk.spans[layer], chunk.waiting->spans[layer], SPAN_LAYOUT);
			}
			chunk.waiting.reset();
		}
		if (!chunk.built) continue;
		auto [low, high] = this->Bounds(chunk, index);
		if (BoxMeets(frustum, low, high) && camera.NearestTilePixels(low, high) >= NETWORK_FADE_START) shown.push_back(&chunk);
	}
}

/* A block the camera has long been too far from gives its meshes back and drops any build of them left unfinished, so a big map only holds the ways near the view. */
void NetworkField::Evict()
{
	for (NetworkChunk &chunk : this->chunks) {
		if (this->frame - chunk.wanted < EVICT_FRAMES) continue;
		chunk.rebuild.reset();
		chunk.due_since = 0;
		if (!chunk.built) continue;
		chunk.Release();
		chunk.waiting.reset();
		chunk.signals.clear();
		chunk.built = false;
	}
}

void NetworkChunk::Release()
{
	for (MeshBuffer &layer : this->layers) layer.Release();
	for (MeshBuffer &span : this->spans) span.Release();
}

void NetworkField::Release()
{
	for (NetworkChunk &chunk : this->chunks) chunk.Release();
	this->chunks.clear();
	this->due.clear();
	this->grid.Clear();
}
