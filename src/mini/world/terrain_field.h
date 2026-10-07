/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file terrain_field.h The map's ground and water surface as meshes, one pair per block of tiles, built as views ask for them. */

#ifndef MINI_WORLD_TERRAIN_FIELD_H
#define MINI_WORLD_TERRAIN_FIELD_H

#include <vector>

#include "../gpu/mesh_buffer.h"
#include "../map/world_tiles.h"
#include "chunk_grid.h"
#include "scene_view.h"

class TerrainField {
public:
	static constexpr int CHUNK_TILES = 32;

	void Sync(const WorldChanges &changes);
	void DrawGround(const SceneView &camera, const Frustum &frustum);
	void DrawWater(const SceneView &camera);
	void Release();

private:
	/* A block of tiles: the meshes it was last built at and the height range its ground spans. */
	struct Chunk {
		MeshBuffer ground;
		MeshBuffer water;
		int step = 0;
		bool stale = true;
		bool surveyed = false;
		uint8_t low = 0;
		uint8_t high = 0;
		uint64_t drawn = 0;
	};

	void Lay(Dimension map);
	void Survey(Chunk &chunk, const TileSpan &tiles) const;
	Chunk *Prepare(size_t index, const SceneView &camera, const Frustum &frustum);
	void Evict();

	/* A block to draw this frame and how far its middle lies from the eye. */
	struct Shown {
		const Chunk *chunk;
		double distance;
	};

	ChunkGrid grid{CHUNK_TILES};
	std::vector<Chunk> chunks;
	MeshBuffer outer_bed;
	MeshBuffer outer_water;
	std::vector<Shown> shown;
	uint64_t frame = 0;
};

#endif /* MINI_WORLD_TERRAIN_FIELD_H */
