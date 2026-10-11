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
#include <span>
#include <utility>
#include <vector>

#include "../gpu/mesh_buffer.h"
#include "../map/world_tiles.h"
#include "build_slice.h"
#include "chunk_grid.h"
#include "chunk_keep.h"
#include "scene_view.h"
#include "smoke_vent.h"
#include "terrain_mesh.h"

class TerrainField {
public:
	static constexpr int CHUNK_TILES = 32;

	void Sync(const WorldChanges &changes);
	void Refresh(const SceneView &camera);
	void DrawGround(const Sight &sight);
	void DrawWater(const SceneView &camera);
	void Release();

	template <typename Visit>
	void ForEachSprays(const SceneView &camera, Visit visit) const
	{
		for (const Nearby &near : this->nearby) {
			if (this->Shows(near, camera)) visit(std::span<const SmokeVent>(this->chunks[near.index].sprays));
		}
	}

private:
	static constexpr int DRAFT_STEP = 2;
	static constexpr GroundGrain DRAFT_GRAIN = {DRAFT_STEP, 1};

	/* A block of tiles: the meshes it shows and the spray off its falls, the grain its ground was built at and whether the world changed under them since, the build that will replace them,
	 * the frame since which they have waited for it, and the height range its ground spans. */
	struct Chunk {
		MeshBuffer ground;
		MeshBuffer water;
		std::vector<SmokeVent> sprays;
		std::optional<Rebuild<TerrainBuild>> rebuild;
		GroundGrain grain{};
		bool stale = true;
		bool surveyed = false;
		uint8_t low = 0;
		uint8_t high = 0;
		uint64_t drawn = 0;
		uint64_t due_since = 0;

		bool Outdated(const GroundGrain &wanted) const { return this->stale || this->grain != wanted; }
		/* Ground wanted at full detail is first drafted as a lattice, which costs little, wherever none so fine shows yet. */
		GroundGrain NextGrain(const GroundGrain &wanted) const { return wanted.step == 1 && (this->ground.Empty() || this->grain.step > DRAFT_STEP) ? DRAFT_GRAIN : wanted; }
	};

	/* A block whose meshes are out of date, the grain the camera's view asks of it and whether it lies in that view or only casts its shadow into it. */
	struct Due {
		size_t index;
		GroundGrain grain;
		bool in_sight;
	};

	/* A block near enough to the eye for this frame's views to show, its box and how far its middle lies from the eye. */
	struct Nearby {
		size_t index;
		Vec3 low;
		Vec3 high;
		double distance;
	};

	bool Shows(const Nearby &near, const SceneView &camera) const;
	bool ShadowShows(const Nearby &near, const SceneView &camera) const;
	void Lay(Dimension map);
	void Survey(Chunk &chunk, const TileSpan &tiles) const;
	std::pair<Vec3, Vec3> Bounds(size_t index);
	double Distance(size_t index, const Vec3 &eye) const;
	void Build(size_t index, GroundGrain grain);
	void Refine(const BuildSlice &slice);
	void Finish(size_t index);
	void Evict();

	ChunkGrid grid{CHUNK_TILES};
	ChunkKeep keep;
	std::vector<Chunk> chunks;
	MeshBuffer outer_bed;
	MeshBuffer outer_water;
	std::vector<Due> due;
	std::vector<Nearby> nearby;
	uint64_t frame = 0;
};

#endif /* MINI_WORLD_TERRAIN_FIELD_H */
