/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file network_field.h The map's rails, roads, bridges and stations as meshes, one set per block of tiles, built at the detail views ask for. */

#ifndef MINI_WORLD_NETWORK_FIELD_H
#define MINI_WORLD_NETWORK_FIELD_H

#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "../gpu/mesh_buffer.h"
#include "chunk_grid.h"
#include "network_mesh.h"
#include "scene_view.h"

/* Past the tile pixels the simple detail ends at, the ground's bands stand in for the ways; meshes fade into them over the band given here. */
inline constexpr double NETWORK_FADE_START = 9.0;
inline constexpr double NETWORK_FADE_END = 12.0;

/* A block of tiles: its meshes per layer, built on the game's side and waiting to be handed to the GPU or already there,
 * the detail they were built at, its signals, the levels its ways span, and a digest of the texels its ways were built from. */
struct NetworkChunk {
	std::array<MeshBuffer, NETWORK_LAYERS> layers;
	std::optional<std::array<ModelMesh, NETWORK_LAYERS>> waiting;
	std::vector<SignalSpot> signals;
	WayDetail detail = WayDetail::Simple;
	double nearest_pixels = 0.0;
	bool built = false;
	bool stale = true;
	bool surveyed = false;
	uint low = 0;
	uint high = 0;
	uint32_t digest = 0;
	uint64_t wanted = 0;
};

/* Blocks are surveyed and built from the map in Prepare, while the game's state holds still; drawing only hands them to the GPU and culls them. */
class NetworkField {
public:
	static constexpr int CHUNK_TILES = 16;

	void Sync(const WorldChanges &changes);
	void Prepare(const SceneView &camera, std::vector<const NetworkChunk *> &seen);
	void Gather(const SceneView &camera, const Frustum &frustum, std::vector<const NetworkChunk *> &shown);
	void Release();

private:
	void Lay(Dimension map);
	uint32_t Digest(size_t index) const;
	void Survey(NetworkChunk &chunk, const TileSpan &tiles) const;
	void Build(NetworkChunk &chunk, const TileSpan &tiles, WayDetail detail) const;
	std::pair<Vec3, Vec3> Bounds(const NetworkChunk &chunk, size_t index) const;
	void Evict();

	ChunkGrid grid{CHUNK_TILES};
	std::vector<NetworkChunk> chunks;
	double rise = 0.0;
	uint64_t frame = 0;
};

#endif /* MINI_WORLD_NETWORK_FIELD_H */
