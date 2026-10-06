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

#include "../../bridge_map.h"
#include "../../map_func.h"
#include "../../tile_map.h"
#include "../core/seed.h"
#include "seabed.h"

#include "../../safeguards.h"

static constexpr double FULL_DETAIL_PIXELS = 18.0;
static constexpr double HEADROOM_LEVELS = 2.0;
static constexpr uint64_t EVICT_FRAMES = 600;
static constexpr int BUILDS_PER_FRAME = 8;
static constexpr int MITRE_MARGIN = 1;

/* A change reaching a block only stales it when the ground or the ways it is built from changed, in it or on the tiles its pieces meet across its edge. */
void NetworkField::Sync(const WorldChanges &changes)
{
	this->frame++;
	this->Evict();

	Dimension size = _world_tiles.Size();
	if (changes.whole || size != this->grid.Map() || LevelRise() != this->rise) {
		this->Lay(size);
		return;
	}
	this->grid.ForEachTouched(changes.areas, MITRE_MARGIN, [&](size_t index) {
		NetworkChunk &chunk = this->chunks[index];
		uint32_t digest = this->Digest(index);
		if (digest == chunk.digest) return;
		chunk.digest = digest;
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
	for (int ty = std::max(tiles.ty0 - MITRE_MARGIN, 0); ty <= std::min<int>(tiles.ty1 + MITRE_MARGIN, this->grid.Map().height - 1); ty++) {
		for (int tx = std::max(tiles.tx0 - MITRE_MARGIN, 0); tx <= std::min<int>(tiles.tx1 + MITRE_MARGIN, this->grid.Map().width - 1); tx++) {
			TileIndex tile = TileXY(tx, ty);
			mix(_world_tiles.SurfaceAt(tile));
			mix(_world_tiles.NetworkAt(tile));
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

void NetworkField::Build(NetworkChunk &chunk, const TileSpan &tiles, WayDetail detail) const
{
	NetworkMeshes meshes = BuildNetwork(tiles, detail);
	chunk.waiting = std::move(meshes.layers);
	chunk.signals = std::move(meshes.signals);
	chunk.detail = detail;
	chunk.built = true;
	chunk.stale = false;
}

std::pair<Vec3, Vec3> NetworkField::Bounds(const NetworkChunk &chunk, size_t index) const
{
	TileSpan tiles = this->grid.TilesOf(index);
	return {
		{static_cast<double>(tiles.tx0), static_cast<double>(tiles.ty0), (chunk.low - DEEPEST_SINK) * this->rise},
		{tiles.tx1 + 1.0, tiles.ty1 + 1.0, (chunk.high + HEADROOM_LEVELS) * this->rise},
	};
}

/* A block is built once the camera comes near enough to show its ways, at the detail its nearest point asks for; the blocks in sight go first.
 * A few blocks are built a frame; one still waiting shows what it was last built as, or the ground's bands. */
void NetworkField::Prepare(const SceneView &camera, std::vector<const NetworkChunk *> &seen)
{
	seen.clear();
	int builds_left = BUILDS_PER_FRAME;
	for (bool in_sight : {true, false}) {
		for (size_t index = 0; index < this->chunks.size(); index++) {
			NetworkChunk &chunk = this->chunks[index];
			TileSpan tiles = this->grid.TilesOf(index);
			if (!chunk.surveyed) this->Survey(chunk, tiles);
			auto [low, high] = this->Bounds(chunk, index);
			if (BoxMeets(camera.frustum, low, high) != in_sight) continue;

			chunk.nearest_pixels = camera.NearestTilePixels(low, high);
			if (chunk.nearest_pixels < NETWORK_FADE_START) continue;
			chunk.wanted = this->frame;
			WayDetail detail = chunk.nearest_pixels >= FULL_DETAIL_PIXELS ? WayDetail::Full : WayDetail::Simple;
			if ((chunk.stale || !chunk.built || chunk.detail != detail) && builds_left > 0) {
				this->Build(chunk, tiles, detail);
				builds_left--;
			}
			if (in_sight && chunk.built) seen.push_back(&chunk);
		}
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
				const ModelMesh &mesh = (*chunk.waiting)[layer];
				if (mesh.indices.empty()) {
					chunk.layers[layer].Release();
				} else {
					chunk.layers[layer].Upload(mesh, MODEL_LAYOUT);
				}
			}
			chunk.waiting.reset();
		}
		if (!chunk.built) continue;
		auto [low, high] = this->Bounds(chunk, index);
		if (BoxMeets(frustum, low, high) && camera.NearestTilePixels(low, high) >= NETWORK_FADE_START) shown.push_back(&chunk);
	}
}

/* A block the camera has long been too far from gives its meshes back, so a big map only holds the ways near the view. */
void NetworkField::Evict()
{
	for (NetworkChunk &chunk : this->chunks) {
		if (!chunk.built || this->frame - chunk.wanted < EVICT_FRAMES) continue;
		for (MeshBuffer &layer : chunk.layers) layer.Release();
		chunk.waiting.reset();
		chunk.signals.clear();
		chunk.built = false;
	}
}

void NetworkField::Release()
{
	for (NetworkChunk &chunk : this->chunks) {
		for (MeshBuffer &layer : chunk.layers) layer.Release();
	}
	this->chunks.clear();
	this->grid.Clear();
}
