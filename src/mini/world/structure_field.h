/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file structure_field.h The map's buildings as one mesh per block of tiles, built at the detail views ask for and rebuilt where a form changed. */

#ifndef MINI_WORLD_STRUCTURE_FIELD_H
#define MINI_WORLD_STRUCTURE_FIELD_H

#include <optional>
#include <span>
#include <vector>

#include "../gpu/mesh_buffer.h"
#include "build_slice.h"
#include "chunk_grid.h"
#include "chunk_keep.h"
#include "scene_view.h"
#include "structure_mesh.h"

/* Below the tile pixels the fade ends at, a building no longer shows; it fades out over the band given here. */
inline constexpr double STRUCTURE_FADE_START = 1.2;
inline constexpr double STRUCTURE_FADE_END = 2.2;

/* A block's buildings, built a tile at a time so the work may be spread over frames, each tile's form noted in the digests as it is read. */
class StructureBuild {
public:
	StructureBuild(const TileSpan &tiles, StructureDetail detail, std::span<uint32_t> digests);

	StructureDetail Detail() const { return this->detail; }
	bool Done() const { return this->next_ty > this->tiles.ty1; }
	void Advance();
	StructureParts Finish();

private:
	TileSpan tiles;
	StructureDetail detail;
	std::span<uint32_t> digests;
	int next_tx;
	int next_ty;
	StructureParts parts;
};

/* A block of tiles: its mesh, built on the game's side and waiting to be handed to the GPU or already there, the boxes clicks meet, the stacks that smoke,
 * the detail it was built at and whether a form changed under it since, the build that will replace it and the frame since which it has waited for it,
 * and the box it fills, guessed from the ground until it is built. */
struct StructureChunk {
	MeshBuffer mesh;
	std::optional<StructureMesh> waiting;
	std::optional<Rebuild<StructureBuild>> rebuild;
	std::vector<StructurePick> picks;
	std::vector<SmokeVent> vents;
	StructureDetail detail = StructureDetail::Simple;
	Vec3 low{};
	Vec3 high{};
	bool built = false;
	bool stale = true;
	bool surveyed = false;
	uint64_t wanted = 0;
	uint64_t due_since = 0;

	bool Outdated(StructureDetail wanted_detail) const { return this->stale || !this->built || this->detail != wanted_detail; }
	/* Buildings wanted in full where none show yet are first drafted simply, which costs little. */
	StructureDetail NextDetail(StructureDetail wanted_detail) const { return this->built ? wanted_detail : StructureDetail::Simple; }
};

/* The building a sight line meets first, the tile under where it meets it, and how far along the line that lies. */
struct StructureHit {
	TileIndex tile;
	double distance;
};

/* Blocks are built from the game's map in Prepare, while the game's state holds still; drawing only hands them to the GPU and culls them.
 * Every tile the game marked is read again, with the tiles around it whose forms it may change, and a block whose forms differ goes stale. */
class StructureField {
public:
	static constexpr int CHUNK_TILES = 16;

	void Sync(const WorldChanges &changes);
	void Prepare(const SceneView &camera);
	void Gather(const SceneView &camera, const Frustum &frustum, std::vector<const StructureChunk *> &shown);
	std::optional<StructureHit> Pick(const Vec3 &origin, const Vec3 &direction) const;
	void Release();

private:
	/* A block near enough to show its buildings whose mesh is out of date, whether it is in sight, the tile pixels where it comes nearest the eye, and the detail it is wanted at. */
	struct Due {
		size_t index;
		bool in_sight;
		double pixels;
		StructureDetail detail;
	};

	void Lay(Dimension map);
	void Notice(TileIndex tile);
	void Survey(StructureChunk &chunk, size_t index) const;
	void Refine();
	void Finish(size_t index);
	void Evict();

	ChunkGrid grid{CHUNK_TILES};
	ChunkKeep keep;
	BuildsUnderWay building;
	std::vector<StructureChunk> chunks;
	std::vector<uint32_t> digests;
	std::vector<Due> due;
	std::vector<size_t> arrived;
	double rise = 0.0;
	uint64_t frame = 0;
};

#endif /* MINI_WORLD_STRUCTURE_FIELD_H */
