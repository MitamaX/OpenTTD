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

#include "../../bridge_map.h"
#include "../../map_func.h"
#include "../../tile_map.h"
#include "seabed.h"

#include "../../safeguards.h"

static constexpr double FULL_DETAIL_PIXELS = 18.0;
static constexpr double HEADROOM_LEVELS = 2.0;
static constexpr uint64_t EVICT_FRAMES = 600;

/* A changed tile reshapes the pieces beside it, which meet it in mitres, so the blocks around a change go stale too. */
void NetworkField::Sync(const WorldChanges &changes)
{
	this->frame++;
	this->Evict();

	Dimension size = _world_tiles.Size();
	if (changes.whole || size != this->grid.Map() || LevelRise() != this->rise) {
		this->Lay(size);
		return;
	}
	this->grid.ForEachTouched(changes, 1, [&](size_t index) {
		NetworkChunk &chunk = this->chunks[index];
		chunk.stale = true;
		chunk.surveyed = false;
	});
}

void NetworkField::Lay(Dimension map)
{
	this->Release();
	this->grid.Lay(map);
	this->chunks = std::vector<NetworkChunk>(this->grid.Count());
	this->rise = LevelRise();
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
	for (size_t layer = 0; layer < NETWORK_LAYERS; layer++) {
		if (meshes.layers[layer].indices.empty()) {
			chunk.layers[layer].Release();
		} else {
			chunk.layers[layer].Upload(meshes.layers[layer], MODEL_LAYOUT);
		}
	}
	chunk.signals = std::move(meshes.signals);
	chunk.detail = detail;
	chunk.built = true;
	chunk.stale = false;
}

/* A block is built only once a view comes near enough to show its ways, at the detail its nearest point asks for. */
void NetworkField::Gather(const SceneView &camera, const Frustum &frustum, std::vector<const NetworkChunk *> &shown)
{
	shown.clear();
	double rise = LevelRise();
	for (size_t index = 0; index < this->chunks.size(); index++) {
		NetworkChunk &chunk = this->chunks[index];
		TileSpan tiles = this->grid.TilesOf(index);
		if (!chunk.surveyed) this->Survey(chunk, tiles);

		Vec3 low = {static_cast<double>(tiles.tx0), static_cast<double>(tiles.ty0), (chunk.low - DEEPEST_SINK) * rise};
		Vec3 high = {tiles.tx1 + 1.0, tiles.ty1 + 1.0, (chunk.high + HEADROOM_LEVELS) * rise};
		if (!BoxMeets(frustum, low, high)) continue;

		double pixels = camera.NearestTilePixels(low, high);
		if (pixels < NETWORK_FADE_START) continue;
		WayDetail detail = pixels >= FULL_DETAIL_PIXELS ? WayDetail::Full : WayDetail::Simple;
		if (chunk.stale || !chunk.built || chunk.detail != detail) this->Build(chunk, tiles, detail);
		chunk.nearest_pixels = pixels;
		chunk.drawn = this->frame;
		shown.push_back(&chunk);
	}
}

/* A block long out of every view gives its meshes back, so a big map only holds the ways near the view. */
void NetworkField::Evict()
{
	for (NetworkChunk &chunk : this->chunks) {
		if (!chunk.built || this->frame - chunk.drawn < EVICT_FRAMES) continue;
		for (MeshBuffer &layer : chunk.layers) layer.Release();
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
