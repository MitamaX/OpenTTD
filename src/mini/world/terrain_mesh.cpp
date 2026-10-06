/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file terrain_mesh.cpp The ground of the map as triangles: each tile's facets, the walls where tiles step apart, and the sea beyond the edge. */

#include "../../stdafx.h"
#include "terrain_mesh.h"

#include <algorithm>
#include <optional>

#include "../../map_func.h"
#include "../map/tile_shapes.h"
#include "../map/world_tiles.h"

#include "../../safeguards.h"

static constexpr double NORMAL_SCALE = 127.0;
static constexpr double SKIRT_LEVELS_PER_STEP = 2.0;
static constexpr Vec3 UPRIGHT = {0.0, 0.0, 1.0};

/** A tile's corners as the surface texel names them. */
enum TileCorner : uint8_t {
	NORTH,
	WEST,
	EAST,
	SOUTH,
	CORNER_COUNT,
};

using Facet = std::array<TileCorner, 3>;

/* Each corner's offset from the tile's north corner. */
static constexpr std::array<std::array<int, 2>, CORNER_COUNT> CORNER_OFFSETS = {{{0, 0}, {1, 0}, {0, 1}, {1, 1}}};

static int8_t NormalByte(double component)
{
	return static_cast<int8_t>(std::lround(std::clamp(component, -1.0, 1.0) * NORMAL_SCALE));
}

uint32_t TerrainMesh::Add(const WorldPoint &at, const Vec3 &normal, bool wall)
{
	this->vertices.push_back({static_cast<float>(at.x), static_cast<float>(at.y), static_cast<float>(at.level), {NormalByte(normal.x), NormalByte(normal.y), NormalByte(normal.z), static_cast<int8_t>(wall ? NORMAL_SCALE : 0)}});
	return static_cast<uint32_t>(this->vertices.size() - 1);
}

void TerrainMesh::Triangle(uint32_t a, uint32_t b, uint32_t c)
{
	this->indices.insert(this->indices.end(), {a, b, c});
}

void TerrainMesh::Quad(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
	this->Triangle(a, b, c);
	this->Triangle(a, c, d);
}

static uint8_t CornerLevel(const SurfaceTexel &surface, TileCorner corner)
{
	switch (corner) {
		case NORTH: return surface.north;
		case WEST: return surface.west;
		case EAST: return surface.east;
		default: return surface.south;
	}
}

/* The two facets the game folds a tile into. */
static std::array<Facet, 2> FacetsOf(const SurfaceTexel &surface)
{
	if (FoldsWestToEast(surface)) return {{{NORTH, WEST, EAST}, {SOUTH, EAST, WEST}}};
	return {{{NORTH, WEST, SOUTH}, {NORTH, SOUTH, EAST}}};
}

/* A tile's own ground at one of its corners. */
class TileSurface {
public:
	TileSurface(int tx, int ty) : tx(tx), ty(ty), surface(_world_tiles.SurfaceAt(TileXY(tx, ty)))
	{
	}

	uint8_t Level(TileCorner corner) const { return CornerLevel(this->surface, corner); }

	WorldPoint Corner(TileCorner corner) const
	{
		return {static_cast<double>(this->tx + CORNER_OFFSETS[corner][0]), static_cast<double>(this->ty + CORNER_OFFSETS[corner][1]), static_cast<double>(this->Level(corner))};
	}

	std::array<Facet, 2> Facets() const { return FacetsOf(this->surface); }

	/* The facets that meet at a corner, each weighed alike since every facet covers half a tile. */
	Vec3 NormalAt(TileCorner corner) const
	{
		Vec3 sum = {0.0, 0.0, 0.0};
		for (const Facet &facet : this->Facets()) {
			if (std::ranges::find(facet, corner) != facet.end()) sum = sum + this->FacetNormal(facet);
		}
		return sum;
	}

private:
	Vec3 FacetNormal(const Facet &facet) const
	{
		Vec3 a = RenderPoint(this->Corner(facet[0]));
		Vec3 b = RenderPoint(this->Corner(facet[1]));
		Vec3 c = RenderPoint(this->Corner(facet[2]));
		Vec3 normal = Normalised(Cross(b - a, c - a));
		return normal.z < 0.0 ? normal * -1.0 : normal;
	}

	int tx;
	int ty;
	SurfaceTexel surface;
};

static bool OnMap(int tx, int ty)
{
	return tx >= 0 && ty >= 0 && tx <= static_cast<int>(Map::MaxX()) && ty <= static_cast<int>(Map::MaxY());
}

/* Every tile meeting at a corner point at the same height shares its facets there, so the ground shades smoothly wherever it runs on unbroken. */
static Vec3 SmoothNormal(int cx, int cy, uint8_t level)
{
	Vec3 sum = {0.0, 0.0, 0.0};
	for (TileCorner corner : {NORTH, WEST, EAST, SOUTH}) {
		int tx = cx - CORNER_OFFSETS[corner][0];
		int ty = cy - CORNER_OFFSETS[corner][1];
		if (!OnMap(tx, ty)) continue;
		TileSurface neighbour(tx, ty);
		if (neighbour.Level(corner) == level) sum = sum + neighbour.NormalAt(corner);
	}
	return Length(sum) > 0.0 ? Normalised(sum) : UPRIGHT;
}

static void AddTile(TerrainMesh &mesh, int tx, int ty)
{
	TileSurface tile(tx, ty);
	std::array<uint32_t, CORNER_COUNT> corners;
	for (TileCorner corner : {NORTH, WEST, EAST, SOUTH}) {
		WorldPoint at = tile.Corner(corner);
		corners[corner] = mesh.Add(at, SmoothNormal(static_cast<int>(at.x), static_cast<int>(at.y), tile.Level(corner)), false);
	}
	for (const Facet &facet : tile.Facets()) mesh.Triangle(corners[facet[0]], corners[facet[1]], corners[facet[2]]);
}

static void AddWalls(TerrainMesh &mesh, int tx, int ty)
{
	for (DiagDirection side = DIAGDIR_BEGIN; side < DIAGDIR_END; side++) {
		std::optional<StepFace> step = StepFaceOf(tx, ty, side);
		if (!step.has_value()) continue;
		Vec3 outward = {step->outward.x, step->outward.y, 0.0};
		std::array<uint32_t, 4> ring;
		std::ranges::transform(step->ring, ring.begin(), [&](const WorldPoint &corner) { return mesh.Add(corner, outward, true); });
		mesh.Quad(ring[0], ring[1], ring[2], ring[3]);
	}
}

static TerrainMesh BuildTiles(const TileSpan &tiles)
{
	TerrainMesh mesh;
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			AddTile(mesh, tx, ty);
			AddWalls(mesh, tx, ty);
		}
	}
	return mesh;
}

/* Every step-th corner from the block's first to past its last tile, the last one always included. */
static std::vector<int> LatticeLines(int first, int last, int step)
{
	std::vector<int> lines;
	for (int line = first; line < last + 1; line += step) lines.push_back(line);
	lines.push_back(last + 1);
	return lines;
}

static Vec3 LatticeNormal(double x, double y, double step)
{
	double rise = LevelRise();
	double slope_x = (GroundLevel(x + step, y) - GroundLevel(x - step, y)) * rise / (2.0 * step);
	double slope_y = (GroundLevel(x, y + step) - GroundLevel(x, y - step)) * rise / (2.0 * step);
	return Normalised({-slope_x, -slope_y, 1.0});
}

static TerrainMesh BuildLattice(const TileSpan &tiles, int step)
{
	std::vector<int> xs = LatticeLines(tiles.tx0, tiles.tx1, step);
	std::vector<int> ys = LatticeLines(tiles.ty0, tiles.ty1, step);
	size_t columns = xs.size();
	TerrainMesh mesh;
	auto at = [&](size_t i, size_t j) { return static_cast<uint32_t>(j * columns + i); };
	for (int y : ys) {
		for (int x : xs) mesh.Add({static_cast<double>(x), static_cast<double>(y), GroundLevel(x, y)}, LatticeNormal(x, y, step), false);
	}
	for (size_t j = 0; j + 1 < ys.size(); j++) {
		for (size_t i = 0; i + 1 < columns; i++) mesh.Quad(at(i, j), at(i + 1, j), at(i + 1, j + 1), at(i, j + 1));
	}

	double drop = SKIRT_LEVELS_PER_STEP * step;
	auto skirt = [&](uint32_t from, uint32_t to) {
		auto lowered = [&](uint32_t index) {
			const TerrainVertex &top = mesh.vertices[index];
			return mesh.Add({top.x, top.y, top.level - drop}, UPRIGHT, false);
		};
		mesh.Quad(from, to, lowered(to), lowered(from));
	};
	size_t last_row = ys.size() - 1;
	for (size_t i = 0; i + 1 < columns; i++) {
		skirt(at(i, 0), at(i + 1, 0));
		skirt(at(i, last_row), at(i + 1, last_row));
	}
	for (size_t j = 0; j + 1 < ys.size(); j++) {
		skirt(at(0, j), at(0, j + 1));
		skirt(at(columns - 1, j), at(columns - 1, j + 1));
	}
	return mesh;
}

TerrainMesh BuildTerrain(const TileSpan &tiles, int step)
{
	return step <= 1 ? BuildTiles(tiles) : BuildLattice(tiles, step);
}

/* Four strips around the map at sea level, meeting its edge where the border tiles lie. */
TerrainMesh BuildOuterSea(Dimension map, double reach)
{
	double width = map.width;
	double height = map.height;
	TerrainMesh mesh;
	auto strip = [&](double x0, double y0, double x1, double y1) {
		mesh.Quad(mesh.Add({x0, y0, 0.0}, UPRIGHT, false), mesh.Add({x1, y0, 0.0}, UPRIGHT, false), mesh.Add({x1, y1, 0.0}, UPRIGHT, false), mesh.Add({x0, y1, 0.0}, UPRIGHT, false));
	};
	strip(-reach, -reach, width + reach, 0.0);
	strip(-reach, height, width + reach, height + reach);
	strip(-reach, 0.0, 0.0, height);
	strip(width, 0.0, width + reach, height);
	return mesh;
}
