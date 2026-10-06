/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file terrain_pass.cpp The map's ground as meshes, one per block of tiles, shaded from the world's texels. */

#include "../../stdafx.h"
#include "terrain_pass.h"

#include <algorithm>
#include <array>

#include "../../core/math_func.hpp"
#include "../../settings_type.h"
#include "../core/tones.h"
#include "../core/tuning.h"
#include "../gpu/gl_api.h"
#include "../map/map_overlay.h"
#include "terrain_mesh.h"

#include "../../safeguards.h"

static constexpr std::array<const char *, 2> VERTEX_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/terrain.vert",
};
static constexpr std::array<const char *, 4> FRAGMENT_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/common.glsl",
	"mini_ui/shaders/network.glsl",
	"mini_ui/shaders/terrain.frag",
};
static constexpr double OUTER_SEA_REACH = 8192.0;
static constexpr double FINEST_CELL_PIXELS = 3.0;
static constexpr int COARSEST_STEP = 8;
static constexpr uint64_t EVICT_FRAMES = 600;

/* The longest lattice step whose cells still span no more than a few pixels, so distant ground keeps few triangles. */
static int StepFor(double tile_pixels)
{
	int step = 1;
	while (step < COARSEST_STEP && tile_pixels * step < FINEST_CELL_PIXELS) step *= 2;
	return step;
}

TerrainPass::TerrainPass(const WorldTextures &textures) : textures(textures), program(VERTEX_SOURCES, FRAGMENT_SOURCES)
{
}

void TerrainPass::Reload()
{
	this->program.Reload();
}

/* A changed tile reshapes the walls and the shading of the tiles beside it, so the blocks around a change go stale too. */
void TerrainPass::Sync(const WorldChanges &changes)
{
	Dimension size = _world_tiles.Size();
	if (changes.whole || size != this->map) {
		this->Lay(size);
		return;
	}

	uint rows = static_cast<uint>(this->chunks.size()) / std::max(this->columns, 1u);
	for (const Rect &area : changes.areas) {
		uint x0 = static_cast<uint>(std::max(area.left - 1, 0)) / CHUNK_TILES;
		uint y0 = static_cast<uint>(std::max(area.top - 1, 0)) / CHUNK_TILES;
		uint x1 = std::min(static_cast<uint>(area.right + 1) / CHUNK_TILES, this->columns - 1);
		uint y1 = std::min(static_cast<uint>(area.bottom + 1) / CHUNK_TILES, rows - 1);
		for (uint y = y0; y <= y1; y++) {
			for (uint x = x0; x <= x1; x++) {
				Chunk &chunk = this->chunks[y * this->columns + x];
				chunk.stale = true;
				chunk.surveyed = false;
			}
		}
	}
}

void TerrainPass::Lay(Dimension map)
{
	for (Chunk &chunk : this->chunks) chunk.mesh.Release();
	this->map = map;
	this->columns = CeilDiv(map.width, CHUNK_TILES);
	this->chunks = std::vector<Chunk>(static_cast<size_t>(this->columns) * CeilDiv(map.height, CHUNK_TILES));

	TerrainMesh sea = BuildOuterSea(map, OUTER_SEA_REACH);
	this->outer_sea.Upload<TerrainVertex>(sea.vertices, TERRAIN_LAYOUT, sea.indices);
}

TileSpan TerrainPass::TilesOf(size_t index) const
{
	int tx = static_cast<int>(index % this->columns) * CHUNK_TILES;
	int ty = static_cast<int>(index / this->columns) * CHUNK_TILES;
	return {tx, ty, std::min(tx + CHUNK_TILES, static_cast<int>(this->map.width)) - 1, std::min(ty + CHUNK_TILES, static_cast<int>(this->map.height)) - 1};
}

void TerrainPass::Survey(Chunk &chunk, const TileSpan &tiles) const
{
	chunk.low = UINT8_MAX;
	chunk.high = 0;
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			SurfaceTexel surface = _world_tiles.SurfaceAt(TileXY(tx, ty));
			chunk.low = std::min({chunk.low, surface.north, surface.west, surface.east, surface.south});
			chunk.high = std::max({chunk.high, surface.north, surface.west, surface.east, surface.south});
		}
	}
	chunk.surveyed = true;
}

void TerrainPass::Draw(const SceneView &view)
{
	if (this->chunks.empty() || !this->program.Ready()) return;

	this->frame++;
	this->Configure();
	this->textures.Bind();
	this->outer_sea.Draw();
	for (size_t index = 0; index < this->chunks.size(); index++) this->DrawChunk(index, view);
	this->Evict();
}

/* A block out of view is left alone; one in view is built at the step its nearest point asks for. */
void TerrainPass::DrawChunk(size_t index, const SceneView &view)
{
	Chunk &chunk = this->chunks[index];
	TileSpan tiles = this->TilesOf(index);
	if (!chunk.surveyed) this->Survey(chunk, tiles);

	double rise = LevelRise();
	Vec3 low = {static_cast<double>(tiles.tx0), static_cast<double>(tiles.ty0), chunk.low * rise};
	Vec3 high = {tiles.tx1 + 1.0, tiles.ty1 + 1.0, chunk.high * rise};
	if (!view.Sees(low, high)) return;

	Vec3 nearest = {Clamp(view.eye.x, low.x, high.x), Clamp(view.eye.y, low.y, high.y), Clamp(view.eye.z, low.z, high.z)};
	int step = StepFor(view.TilePixelsAt(Length(view.eye - nearest)));
	if (chunk.stale || chunk.step != step) {
		TerrainMesh mesh = BuildTerrain(tiles, step);
		chunk.mesh.Upload<TerrainVertex>(mesh.vertices, TERRAIN_LAYOUT, mesh.indices);
		chunk.step = step;
		chunk.stale = false;
	}
	chunk.drawn = this->frame;
	chunk.mesh.Draw();
}

/* A block long out of view gives its mesh back, so a big map only holds the ground near the view. */
void TerrainPass::Evict()
{
	for (Chunk &chunk : this->chunks) {
		if (chunk.mesh.Empty() || this->frame - chunk.drawn < EVICT_FRAMES) continue;
		chunk.mesh.Release();
		chunk.stale = true;
	}
}

void TerrainPass::Configure() const
{
	this->program.Use();
	WorldTextures::BindSamplers(this->program);
	glUniform1i(this->program.Uniform("u_landscape"), to_underlying(_settings_game.game_creation.landscape));
	glUniform1f(this->program.Uniform("u_contour"), ChannelShare(_tuning.contour_alpha));
	glUniform1f(this->program.Uniform("u_grid"), ChannelShare(_tuning.grid_alpha));
	glUniform1i(this->program.Uniform("u_layer"), to_underlying(_overlay.Filter()));
	glUniform1f(this->program.Uniform("u_sink"), ChannelShare(_tuning.filter_alpha));
}

void TerrainPass::Release()
{
	for (Chunk &chunk : this->chunks) chunk.mesh.Release();
	this->chunks.clear();
	this->outer_sea.Release();
	this->program.Release();
	this->map = {};
	this->columns = 0;
}
