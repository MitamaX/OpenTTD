/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file terrain_pass.h The map's ground as meshes, one per block of tiles, shaded from the world's texels. */

#ifndef MINI_WORLD_TERRAIN_PASS_H
#define MINI_WORLD_TERRAIN_PASS_H

#include <vector>

#include "../gpu/mesh_buffer.h"
#include "shader_program.h"
#include "world_pass.h"
#include "world_textures.h"

class TerrainPass final : public WorldPass {
public:
	static constexpr int CHUNK_TILES = 32;

	explicit TerrainPass(const WorldTextures &textures);

	void Reload() override;
	void Sync(const WorldChanges &changes) override;
	void Draw(const SceneView &view) override;
	void Release() override;

private:
	/* A block of tiles: the mesh it was last built at and the height range its ground spans. */
	struct Chunk {
		MeshBuffer mesh;
		int step = 0;
		bool stale = true;
		bool surveyed = false;
		uint8_t low = 0;
		uint8_t high = 0;
		uint64_t drawn = 0;
	};

	void Lay(Dimension map);
	void Survey(Chunk &chunk, const TileSpan &tiles) const;
	TileSpan TilesOf(size_t index) const;
	void DrawChunk(size_t index, const SceneView &view);
	void Evict();
	void Configure() const;

	const WorldTextures &textures;
	ShaderProgram program;
	std::vector<Chunk> chunks;
	MeshBuffer outer_sea;
	Dimension map{};
	uint columns = 0;
	uint64_t frame = 0;
};

#endif /* MINI_WORLD_TERRAIN_PASS_H */
