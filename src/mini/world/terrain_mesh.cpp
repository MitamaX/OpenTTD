/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file terrain_mesh.cpp The ground of the map as triangles: each tile's facets, the walls where tiles step apart, and the seabed beyond the edge. */

#include "../../stdafx.h"
#include "terrain_mesh.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "../../map_func.h"
#include "../map/tile_shapes.h"
#include "../map/world_tiles.h"
#include "seabed.h"
#include "shore_relief.h"

#include "../../safeguards.h"

static constexpr double NORMAL_SCALE = 127.0;
static constexpr double SKIRT_LEVELS_PER_STEP = 2.0;
static constexpr double STEEP_BANK_RUN = 0.35;
static constexpr double LONGEST_BANK_RUN = 1.0;
static constexpr double LEAST_BANK_FALL = 0.25;
static constexpr double BANK_PROBE = 0.25;
static constexpr int COAST_DIVISIONS = 4;
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

uint32_t TerrainMesh::Add(const WorldPoint &at, const Vec3 &normal, double mark)
{
	return this->Add({static_cast<float>(at.x), static_cast<float>(at.y), static_cast<float>(at.level), {NormalByte(normal.x), NormalByte(normal.y), NormalByte(normal.z), NormalByte(mark)}});
}

double SunkMark(double sink)
{
	return -sink / DEEPEST_SINK;
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

/* One side of a tile going round from its first corner: where its middle lies on the half tile lattice and which tile lies across it. */
struct BasinSide {
	TileCorner from;
	TileCorner to;
	int half_x;
	int half_y;
	int across_x;
	int across_y;
};

static constexpr std::array<BasinSide, CORNER_COUNT> BASIN_SIDES = {{
	{NORTH, WEST, 1, 0, 0, -1},
	{WEST, SOUTH, 2, 1, 1, 0},
	{SOUTH, EAST, 1, 2, 0, 1},
	{EAST, NORTH, 0, 1, -1, 0},
}};

/* A tile of open water dips toward its middle, and toward the middles of its sides where open water runs on, so even a narrow stream has a channel under it. */
static void AddBasin(TerrainMesh &mesh, const Seabed &bed, const std::array<uint32_t, CORNER_COUNT> &corners, int tx, int ty)
{
	auto at = [&](int half_x, int half_y, double level) {
		return mesh.Add({half_x * HALF_TILE, half_y * HALF_TILE, level}, bed.Normal(half_x, half_y, 1), SunkMark(SurfaceLevelOf(tx, ty) - level));
	};
	uint32_t centre = at(2 * tx + 1, 2 * ty + 1, bed.Level(2 * tx + 1, 2 * ty + 1));
	for (const BasinSide &side : BASIN_SIDES) {
		int half_x = 2 * tx + side.half_x;
		int half_y = 2 * ty + side.half_y;
		double ends = (mesh.vertices[corners[side.from]].level + mesh.vertices[corners[side.to]].level) * 0.5;
		double level = SharesBasin(tx, ty, tx + side.across_x, ty + side.across_y) ? bed.Level(half_x, half_y) : ends;
		uint32_t middle = at(half_x, half_y, level);
		mesh.Triangle(corners[side.from], middle, centre);
		mesh.Triangle(middle, corners[side.to], centre);
	}
}

static TileCorner CornerAt(int dx, int dy)
{
	for (TileCorner corner : {NORTH, WEST, EAST, SOUTH}) {
		if (CORNER_OFFSETS[corner][0] == dx && CORNER_OFFSETS[corner][1] == dy) return corner;
	}
	NOT_REACHED();
}

/* A bare coast tile is laid as a fine grid over its curved ground, sharing the corners the tile would have had. */
static void AddCoast(TerrainMesh &mesh, const Seabed &bed, const std::array<uint32_t, CORNER_COUNT> &corners, int tx, int ty)
{
	std::array<double, CORNER_COUNT> levels;
	std::ranges::transform(corners, levels.begin(), [&](uint32_t index) { return static_cast<double>(mesh.vertices[index].level); });
	ShoreRelief relief(bed, tx, ty, levels);
	double water = SurfaceLevelOf(tx, ty);

	constexpr int SIDE = COAST_DIVISIONS + 1;
	std::array<uint32_t, SIDE * SIDE> grid;
	for (int j = 0; j < SIDE; j++) {
		for (int i = 0; i < SIDE; i++) {
			bool on_corner = (i % COAST_DIVISIONS == 0) && (j % COAST_DIVISIONS == 0);
			if (on_corner) {
				grid[j * SIDE + i] = corners[CornerAt(i / COAST_DIVISIONS, j / COAST_DIVISIONS)];
				continue;
			}
			double x = tx + static_cast<double>(i) / COAST_DIVISIONS;
			double y = ty + static_cast<double>(j) / COAST_DIVISIONS;
			double level = relief.Level(x, y);
			grid[j * SIDE + i] = mesh.Add({x, y, level}, relief.Normal(x, y), SunkMark(std::max(water - level, 0.0)));
		}
	}
	for (int j = 0; j < COAST_DIVISIONS; j++) {
		for (int i = 0; i < COAST_DIVISIONS; i++) mesh.Quad(grid[j * SIDE + i], grid[j * SIDE + i + 1], grid[(j + 1) * SIDE + i + 1], grid[(j + 1) * SIDE + i]);
	}
}

static void AddTile(TerrainMesh &mesh, const Seabed &bed, int tx, int ty)
{
	TileSurface tile(tx, ty);
	std::array<uint32_t, CORNER_COUNT> corners;
	for (TileCorner corner : {NORTH, WEST, EAST, SOUTH}) {
		WorldPoint at = tile.Corner(corner);
		int cx = static_cast<int>(at.x);
		int cy = static_cast<int>(at.y);
		double sink = bed.Sink(cx, cy);
		Vec3 normal = sink > 0.0 ? bed.Normal(2 * cx, 2 * cy, 1) : SmoothNormal(cx, cy, tile.Level(corner));
		corners[corner] = mesh.Add({at.x, at.y, at.level - sink}, normal, SunkMark(sink));
	}
	if (WaterFormOf(tx, ty) == WaterForm::Open) {
		AddBasin(mesh, bed, corners, tx, ty);
		return;
	}
	if (IsBareCoast(tx, ty)) {
		AddCoast(mesh, bed, corners, tx, ty);
		return;
	}
	for (const Facet &facet : tile.Facets()) mesh.Triangle(corners[facet[0]], corners[facet[1]], corners[facet[2]]);
}

static bool StandsInSea(int tx, int ty)
{
	return OnMap(tx, ty) && _world_tiles.WaterAt(TileXY(tx, ty)).sea > 0;
}

/* How many tiles out a bank runs for each level it falls: on along the ground above where that falls toward the water, so the two meet without a fold, and steeply where it does not. */
static double BankRun(int tx, int ty, const StepFace &step)
{
	TileGround ground(tx, ty);
	WorldPoint middle = Between(step.ring[0], step.ring[3], HALF_TILE);
	double inner = ground.Level(middle.x - step.outward.x * BANK_PROBE, middle.y - step.outward.y * BANK_PROBE);
	double fall = (inner - ground.Level(middle.x, middle.y)) / BANK_PROBE;
	return fall >= LEAST_BANK_FALL ? 1.0 / fall : STEEP_BANK_RUN;
}

static Vec3 FaceNormal(const std::array<WorldPoint, 4> &ring)
{
	Vec3 normal = Normalised(Cross(RenderPoint(ring[2]) - RenderPoint(ring[0]), RenderPoint(ring[3]) - RenderPoint(ring[1])));
	return normal.z < 0.0 ? normal * -1.0 : normal;
}

static bool AtCorner(const WorldPoint &at)
{
	return at.x == std::floor(at.x) && at.y == std::floor(at.y);
}

/* A bank's corners sink with the seabed as the ground's own corners do, so the two meet without a gap. */
static WorldPoint Settled(const Seabed &bed, const WorldPoint &at)
{
	if (!AtCorner(at)) return at;
	return {at.x, at.y, at.level - bed.Sink(static_cast<int>(at.x), static_cast<int>(at.y))};
}

/* A step whose foot stands in the sea leans out into it as a bank of the ground above, instead of standing upright; along its top it shades on as the ground it hangs from. */
static void AddBank(TerrainMesh &mesh, const Seabed &bed, int tx, int ty, const StepFace &step)
{
	double run = BankRun(tx, ty, step);
	auto lean = [&](const WorldPoint &top, const WorldPoint &foot) {
		double reach = std::min((top.level - foot.level) * run, LONGEST_BANK_RUN);
		return WorldPoint{foot.x + step.outward.x * reach, foot.y + step.outward.y * reach, foot.level};
	};
	std::array<WorldPoint, 4> ring = {step.ring[0], lean(step.ring[0], step.ring[1]), lean(step.ring[3], step.ring[2]), step.ring[3]};
	Vec3 face = FaceNormal(ring);
	auto normal = [&](const WorldPoint &at, bool top) {
		return top && AtCorner(at) ? SmoothNormal(static_cast<int>(at.x), static_cast<int>(at.y), static_cast<uint8_t>(at.level)) : face;
	};
	std::array<uint32_t, 4> corners;
	for (size_t i = 0; i < ring.size(); i++) corners[i] = mesh.Add(Settled(bed, ring[i]), normal(ring[i], i == 0 || i == 3), DRY_MARK);
	mesh.Quad(corners[0], corners[1], corners[2], corners[3]);
}

static void AddWall(TerrainMesh &mesh, const StepFace &step)
{
	Vec3 outward = {step.outward.x, step.outward.y, 0.0};
	std::array<uint32_t, 4> ring;
	std::ranges::transform(step.ring, ring.begin(), [&](const WorldPoint &corner) { return mesh.Add(corner, outward, WALL_MARK); });
	mesh.Quad(ring[0], ring[1], ring[2], ring[3]);
}

static void AddWalls(TerrainMesh &mesh, const Seabed &bed, int tx, int ty)
{
	for (DiagDirection side = DIAGDIR_BEGIN; side < DIAGDIR_END; side++) {
		std::optional<StepFace> step = StepFaceOf(tx, ty, side);
		if (!step.has_value()) continue;
		TileIndexDiffC across = TileIndexDiffCByDiagDir(side);
		if (StandsInSea(tx + across.x, ty + across.y)) {
			AddBank(mesh, bed, tx, ty, *step);
		} else {
			AddWall(mesh, *step);
		}
	}
}

static TerrainMesh BuildTiles(const TileSpan &tiles)
{
	Seabed bed(tiles);
	TerrainMesh mesh;
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			AddTile(mesh, bed, tx, ty);
			AddWalls(mesh, bed, tx, ty);
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

static TerrainMesh BuildLattice(const TileSpan &tiles, int step)
{
	Seabed bed(tiles);
	std::vector<int> xs = LatticeLines(tiles.tx0, tiles.tx1, step);
	std::vector<int> ys = LatticeLines(tiles.ty0, tiles.ty1, step);
	size_t columns = xs.size();
	TerrainMesh mesh;
	auto at = [&](size_t i, size_t j) { return static_cast<uint32_t>(j * columns + i); };
	for (int y : ys) {
		for (int x : xs) mesh.Add({static_cast<double>(x), static_cast<double>(y), bed.Level(2 * x, 2 * y)}, bed.Normal(2 * x, 2 * y, 2 * step), SunkMark(bed.Sink(x, y)));
	}
	for (size_t j = 0; j + 1 < ys.size(); j++) {
		for (size_t i = 0; i + 1 < columns; i++) mesh.Quad(at(i, j), at(i + 1, j), at(i + 1, j + 1), at(i, j + 1));
	}

	double drop = SKIRT_LEVELS_PER_STEP * step;
	auto skirt = [&](uint32_t from, uint32_t to) {
		auto lowered = [&](uint32_t index) {
			const TerrainVertex &top = mesh.vertices[index];
			return mesh.Add({top.x, top.y, top.level - drop}, UPRIGHT, DRY_MARK);
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

/* The seabed falls from the map's edge to the open sea's floor over a shelf, then lies flat out to the horizon. */
TerrainMesh BuildOuterBed(Dimension map, double reach)
{
	double width = map.width;
	double height = map.height;
	double shelf = SHELF_TILES;
	double floor = -DEEPEST_SINK;
	TerrainMesh mesh;
	auto add = [&](double x, double y, double level) { return mesh.Add({x, y, level}, UPRIGHT, SunkMark(-level)); };
	auto edge = [&](int corners, int cx, int cy, int step_x, int step_y, double out_x, double out_y) {
		for (int corner = 0; corner < corners; corner++) {
			int ax = cx + corner * step_x;
			int ay = cy + corner * step_y;
			int bx = ax + step_x;
			int by = ay + step_y;
			mesh.Quad(add(ax, ay, -CornerSink(ax, ay)), add(bx, by, -CornerSink(bx, by)), add(bx + out_x, by + out_y, floor), add(ax + out_x, ay + out_y, floor));
		}
	};
	int columns = static_cast<int>(map.width);
	int rows = static_cast<int>(map.height);
	edge(columns, 0, 0, 1, 0, 0.0, -shelf);
	edge(columns, 0, rows, 1, 0, 0.0, shelf);
	edge(rows, 0, 0, 0, 1, -shelf, 0.0);
	edge(rows, columns, 0, 0, 1, shelf, 0.0);

	auto nook = [&](int cx, int cy, double dx, double dy) {
		mesh.Quad(add(cx, cy, -CornerSink(cx, cy)), add(cx + dx, cy, floor), add(cx + dx, cy + dy, floor), add(cx, cy + dy, floor));
	};
	nook(0, 0, -shelf, -shelf);
	nook(columns, 0, shelf, -shelf);
	nook(0, rows, -shelf, shelf);
	nook(columns, rows, shelf, shelf);

	auto strip = [&](double x0, double y0, double x1, double y1) {
		mesh.Quad(add(x0, y0, floor), add(x1, y0, floor), add(x1, y1, floor), add(x0, y1, floor));
	};
	strip(-reach, -reach, width + reach, -shelf);
	strip(-reach, height + shelf, width + reach, height + reach);
	strip(-reach, -shelf, -shelf, height + shelf);
	strip(width + shelf, -shelf, width + reach, height + shelf);
	return mesh;
}
