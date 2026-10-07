/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file terrain_field.h The map's ground and water surface as meshes, one pair per block of tiles, built as the camera's view asks for them. */

#ifndef MINI_WORLD_TERRAIN_FIELD_H
#define MINI_WORLD_TERRAIN_FIELD_H

#include <optional>
#include <utility>
#include <vector>

#include "../gpu/mesh_buffer.h"
#include "../map/world_tiles.h"
#include "chunk_grid.h"
#include "scene_view.h"
#include "terrain_mesh.h"

class TerrainField {
public:
	static constexpr int CHUNK_TILES = 32;

	void Sync(const WorldChanges &changes);
	void Refresh(const SceneView &camera);
	void DrawGround(const SceneView &camera, const Frustum &frustum);
	void DrawWater(const SceneView &camera);
	void Release();

private:
	static constexpr int DRAFT_STEP = 2;

	/* A build under way to replace a block's meshes, and whether the world changed under it since it began. */
	struct Rebuild {
		TerrainBuild build;
		bool outdated = false;
	};

	/* A block of tiles: the meshes it shows, the step its ground was built at and whether the world changed under them since, the build that will replace them,
	 * the frame since which they have waited for it, and the height range its ground spans. */
	struct Chunk {
		MeshBuffer ground;
		MeshBuffer water;
		std::optional<Rebuild> rebuild;
		int step = 0;
		bool stale = true;
		bool surveyed = false;
		uint8_t low = 0;
		uint8_t high = 0;
		uint64_t drawn = 0;
		uint64_t due_since = 0;

		bool Outdated(int wanted_step) const { return this->stale || this->step != wanted_step; }
		/* Ground wanted at full detail is first drafted as a lattice, which costs little, wherever none so fine shows yet. */
		int NextStep(int wanted_step) const { return wanted_step == 1 && (this->ground.Empty() || this->step > DRAFT_STEP) ? DRAFT_STEP : wanted_step; }
	};

	/* A block in the camera's view whose meshes are out of date, the step the view asks of it and how far its middle lies from the eye. */
	struct Due {
		size_t index;
		int step;
		double distance;
	};

	/* A block to draw this frame and how far its middle lies from the eye. */
	struct Shown {
		const Chunk *chunk;
		double distance;
	};

	void Lay(Dimension map);
	void Survey(Chunk &chunk, const TileSpan &tiles) const;
	std::pair<Vec3, Vec3> Bounds(size_t index);
	double Distance(size_t index, const Vec3 &eye) const;
	void Build(size_t index, int step);
	void Refine();
	void Finish(size_t index);
	void Evict();

	ChunkGrid grid{CHUNK_TILES};
	std::vector<Chunk> chunks;
	MeshBuffer outer_bed;
	MeshBuffer outer_water;
	std::vector<Due> due;
	std::vector<Shown> shown;
	uint64_t frame = 0;
};

#endif /* MINI_WORLD_TERRAIN_FIELD_H */
