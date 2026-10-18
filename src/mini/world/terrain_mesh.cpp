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
#include <map>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

#include "../../map_func.h"
#include "../../track_func.h"
#include "../../tunnel_map.h"
#include "../map/network_style.h"
#include "../map/tile_shapes.h"
#include "../map/way_course.h"
#include "../map/world_tiles.h"
#include "bridge_models.h"
#include "sea_frame.h"
#include "seabed.h"
#include "shore_relief.h"
#include "tile_grid.h"
#include "tile_relief.h"
#include "track_models.h"
#include "water_sheet.h"
#include "way_shapes.h"

#include "../../safeguards.h"

static constexpr double SKIRT_LEVELS_PER_STEP = 2.0;
static constexpr double RELIEF_SKIRT_LEVELS = 0.25;
static constexpr double EARTH_BANK_RUN_PER_RISE = 2.5;
static constexpr double OPEN_EARTH_BANK_RUN = 1.0;
static constexpr double FAR_EARTH_BANK_RUN = 2.0;
static constexpr double WAYSIDE_EARTH_BANK_RUN = 0.2;
static constexpr int EARTH_BANK_ROWS = 8;
static constexpr int BANK_COLUMNS_PER_TILE = FINE_RELIEF_DIVISIONS;
static constexpr double BANK_TOP_INSET = 1.5 * CLIMB_PROBE;
/* Earth stands no steeper than this, in tile widths of fall for each of run; a way banked up any steeper over a way below stands on a retaining wall. */
static constexpr double STEEPEST_EARTH = 1.6;
static constexpr double LEAST_WALL_FALL = 0.3;
static constexpr size_t LEAST_WALL_ROWS = 3;
static constexpr double INNER_OFFSET_LIMIT = 0.85;
static constexpr int RAMP_BANK_ROWS = 12;
static constexpr double RAMP_BANK_RUN = 1.2;
static constexpr double RAMP_BANK_RUN_PER_RISE = 1.5;
static constexpr double COPING_RISE = 0.025;
static constexpr double COPING_OVERHANG = 0.015;
static constexpr double WING_THICKNESS = 0.12;
static constexpr double DECK_WING_THICKNESS = 0.04;
static constexpr double CopingDepthFor(double thickness) { return thickness + COPING_OVERHANG; }
static constexpr double COPING_DEPTH = CopingDepthFor(WING_THICKNESS);
static_assert(DECK_HALF - CopingDepthFor(DECK_WING_THICKNESS) >= std::max(ROAD_HALF, BALLAST_HALF));
/* A bank's foot dips this far into the ground, in levels, so the two part on a clean line instead of flickering where the bank runs out over it. */
static constexpr double BANK_FOOT_DIP = 0.03;
static constexpr double EARTH_BANK_FOOT_DIP = 0.06;
static constexpr double SHALLOW_BANK_FOOT_DIP = 0.16;
static constexpr double SHALLOW_BANK_RISE = 1.0;
static constexpr double SHALLOWEST_FACE = 2.0 * BANK_FOOT_DIP;
static constexpr double SHALLOWEST_CAP = 5.0 * BANK_FOOT_DIP;
static constexpr double LEAST_STEP = 1e-3;
static constexpr double SAME_POINT = 1e-6;
static constexpr double PARALLEL_LEAST = 1.0 - SAME_POINT;
static constexpr double WALL_OVERLAP_LEAST = 0.1;
static constexpr int LEAST_SHORE_DIVISIONS = 2;
static constexpr int BANK_DIVISIONS_FACTOR = 2;
static constexpr int BASIN_SIDE_CUTS = 2;
static constexpr int NO_SHORE_CUTS = 0;
static constexpr ShoreCut NO_SHORE_CUT = {NO_SHORE_CUTS, false};
static constexpr int SHORE_CUTS_MARGIN = 1;
static constexpr double FORMATION_RUN = 2.0;
static constexpr double FORMATION_ROWS_PER_TILE = 4.0;
static constexpr int FORMATION_TOE_STEPS = 8;
static constexpr int FORMATION_TOE_REFINES = 4;
static constexpr int NEIGHBOUR_WAY_REACH = 1;
static constexpr double WAY_MIDLINE_MARGIN = 0.02;
static constexpr double SEAT_STEP_LEAST = 0.2;
static constexpr double ALONGSIDE_LEAST_DOT = 0.98;
static constexpr double ALONGSIDE_SKEW = 0.2;
static constexpr double ALONGSIDE_SLIP = 0.01;
static constexpr double WIDEST_FILLED_GAP = 0.5;
static constexpr double SAME_CREST_LEVELS = 0.05;
static constexpr double ROW_SPACING = 1.0 / FORMATION_ROWS_PER_TILE;
static constexpr int FILL_EASING_ROWS = 4;
static constexpr double WAY_CESS = DECK_HALF - BALLAST_HALF;
static constexpr double SEA_FLOOR = -DEEPEST_SINK;
static constexpr double CREST_ROUNDING = 0.5;
static constexpr double END_PROBE = 0.05;
static constexpr double MIDWAY = 0.5;
static constexpr double OWN_TILE_INSET = 1e-3;
static constexpr double EDGE_SNAP = 1e-3;
static constexpr double JOINT_REACH = 0.1;
static constexpr int BANK_SOURCE_REACH = 2;
static_assert(BANK_SOURCE_REACH >= FAR_EARTH_BANK_RUN);

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

uint32_t TerrainMesh::Add(const WorldPoint &at, const Vec3 &normal, double mark)
{
	return this->Add({static_cast<float>(at.x), static_cast<float>(at.y), static_cast<float>(at.level), {SignedByte(normal.x), SignedByte(normal.y), SignedByte(normal.z), SignedByte(mark)}});
}

double SunkMark(double sink)
{
	return -sink / DEEPEST_SINK;
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

	uint8_t Level(TileCorner corner) const { return this->surface.CornerAt(CORNER_OFFSETS[corner][0], CORNER_OFFSETS[corner][1]); }

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
	return Length(sum) > 0.0 ? Normalised(sum) : UP;
}

CornerNormals::CornerNormals(const TileSpan &tiles) : tiles(tiles), columns(tiles.tx1 - tiles.tx0 + 2)
{
	this->corners.resize(static_cast<size_t>(this->columns) * (tiles.ty1 - tiles.ty0 + 2));
}

static Vec3 CornerNormalOf(const Landform &land, int cx, int cy, uint8_t level)
{
	return land.RollsRound(cx, cy) ? land.RolledNormal(cx, cy) : SmoothNormal(cx, cy, level);
}

Vec3 CornerNormals::At(const Landform &land, int cx, int cy, uint8_t level)
{
	Known &corner = this->corners[static_cast<size_t>(cy - this->tiles.ty0) * this->columns + (cx - this->tiles.tx0)];
	if (!corner.known) corner = {CornerNormalOf(land, cx, cy, level), level, true};
	return corner.level == level ? corner.normal : SmoothNormal(cx, cy, level);
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

/* A tile of open water dips toward its middle, and toward the middles of its sides where open water runs on, so even a narrow stream has a channel under it; a river laying its bed along its own outline meets it on a straight side. */
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
		int nx = tx + side.across_x;
		int ny = ty + side.across_y;
		bool sunk = SharesBasin(tx, ty, nx, ny) && !IsRiverside(nx, ny) && !LiesAlongShore(bed, nx, ny);
		double level = sunk ? bed.Level(half_x, half_y) : ends;
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

static Vec3 VertexNormal(const TerrainMesh &mesh, uint32_t index)
{
	return NormalOfBytes(mesh.vertices[index].normal);
}

static Vec3 FacetedNormal(const std::array<Vec3, CORNER_COUNT> &corners, double fx, double fy)
{
	Vec3 north = Lerp(corners[NORTH], corners[WEST], fx);
	Vec3 south = Lerp(corners[EAST], corners[SOUTH], fx);
	return Lerp(north, south, fy);
}

static Vec3 FacetedNormal(const TerrainMesh &mesh, const std::array<uint32_t, CORNER_COUNT> &corners, double fx, double fy)
{
	return FacetedNormal(std::array<Vec3, CORNER_COUNT>{VertexNormal(mesh, corners[NORTH]), VertexNormal(mesh, corners[WEST]), VertexNormal(mesh, corners[EAST]), VertexNormal(mesh, corners[SOUTH])}, fx, fy);
}

static Vec3 GroundNormalAt(const Landform &land, double x, double y)
{
	int tx = static_cast<int>(std::floor(x));
	int ty = static_cast<int>(std::floor(y));
	if (!OnMap(tx, ty)) return ClimbNormal(land.Climb(x, y));
	LaidGround laid(land, tx, ty);
	const TileRelief *relief = laid.Relief();
	if (relief == nullptr) return ClimbNormal(land.Climb(x, y));
	TileSurface tile(tx, ty);
	std::array<Vec3, CORNER_COUNT> corners;
	for (TileCorner corner : {NORTH, WEST, EAST, SOUTH}) {
		corners[corner] = Normalised(CornerNormalOf(land, tx + CORNER_OFFSETS[corner][0], ty + CORNER_OFFSETS[corner][1], tile.Level(corner)));
	}
	return relief->Normal(x, y, FacetedNormal(corners, x - tx, y - ty));
}

/* A tile whose ground curves is laid as a fine grid over it, sharing the tile's own corners wherever a neighbour keeps them and shading as its facets would between them;
 * it is marked sunk as far as it lies under the water, where the water stands no higher than the tile's own ground. */
static std::vector<uint32_t> AddRelief(TerrainMesh &mesh, const TileRelief &relief, const std::array<uint32_t, CORNER_COUNT> &corners, int tx, int ty, int divisions)
{
	WaterSheet sheet(tx, ty);
	TileGround ground(tx, ty);
	auto mark = [&](double x, double y, double level) {
		if (!relief.HoldsWater()) return DRY_MARK;
		double water = std::min(sheet.Level(x, y), ground.Level(x, y));
		if (level < water) return SunkMark(water - level);
		return relief.OnBankFace(x, y) ? ARMOUR_MARK : DRY_MARK;
	};
	return LayTileGrid(mesh, tx, ty, divisions, [&](int i, int j, double x, double y) {
		bool on_corner = (i % divisions == 0) && (j % divisions == 0);
		if (on_corner && relief.Pinned(x, y)) return corners[CornerAt(i / divisions, j / divisions)];
		double level = relief.Level(x, y);
		return mesh.Add({x, y, level}, relief.Normal(x, y, FacetedNormal(mesh, corners, x - tx, y - ty)), mark(x, y, level));
	});
}

static void AddSkirt(TerrainMesh &mesh, uint32_t from, uint32_t to, double drop)
{
	auto lowered = [&](uint32_t index) {
		const TerrainVertex &top = mesh.vertices[index];
		return mesh.Add({top.x, top.y, top.level - drop}, UP, DRY_MARK);
	};
	mesh.Quad(from, to, lowered(to), lowered(from));
}

template <class Value>
struct Sides {
	Value low_x;
	Value high_x;
	Value low_y;
	Value high_y;

	template <class Map>
	auto Mapped(const Map &map) const
	{
		return Sides<decltype(map(this->low_x))>{map(this->low_x), map(this->high_x), map(this->low_y), map(this->high_y)};
	}
};

using OpenSides = Sides<bool>;
using SideCuts = Sides<int>;

static OpenSides OpenSidesOf(const TileSpan &block, const TileSpan &area)
{
	return {
		area.tx0 == block.tx0 && block.tx0 > 0,
		area.tx1 == block.tx1 && block.tx1 < static_cast<int>(Map::MaxX()),
		area.ty0 == block.ty0 && block.ty0 > 0,
		area.ty1 == block.ty1 && block.ty1 < static_cast<int>(Map::MaxY()),
	};
}

static void SkirtOpenSides(TerrainMesh &mesh, std::span<const uint32_t> grid, size_t columns, const OpenSides &open, double drop)
{
	size_t rows = grid.size() / columns;
	auto at = [&](size_t i, size_t j) { return grid[j * columns + i]; };
	for (size_t i = 0; i + 1 < columns; i++) {
		if (open.low_y) AddSkirt(mesh, at(i, 0), at(i + 1, 0), drop);
		if (open.high_y) AddSkirt(mesh, at(i, rows - 1), at(i + 1, rows - 1), drop);
	}
	for (size_t j = 0; j + 1 < rows; j++) {
		if (open.low_x) AddSkirt(mesh, at(0, j), at(0, j + 1), drop);
		if (open.high_x) AddSkirt(mesh, at(columns - 1, j), at(columns - 1, j + 1), drop);
	}
}

static ShoreCut ShoreCutOf(const Landform &land, int grained, int tx, int ty)
{
	if (!OnMap(tx, ty) || IsRiverside(tx, ty)) return NO_SHORE_CUT;
	if (LaidGround::RollsAt(land, tx, ty)) return {grained, false};
	LaidGround laid(land, tx, ty);
	if (laid.Relief() == nullptr) return WaterFormOf(tx, ty) == WaterForm::Open ? ShoreCut{BASIN_SIDE_CUTS, false} : NO_SHORE_CUT;
	return {std::max(grained * (laid.Banked() ? BANK_DIVISIONS_FACTOR : 1), LEAST_SHORE_DIVISIONS), true};
}

ShoreCut TerrainBuild::ShoreCutAt(int tx, int ty) const
{
	return this->shore_cuts.At(tx, ty, [this](int x, int y) { return ShoreCutOf(this->land, this->DivisionsOf(x, y), x, y); });
}

static bool Nests(const ShoreCut &cut, const ShoreCut &other)
{
	return cut.nests && other.nests && (cut.cuts % other.cuts == 0 || other.cuts % cut.cuts == 0);
}

static int CoarserCuts(const ShoreCut &cut, const ShoreCut &other)
{
	return Nests(cut, other) && other.cuts < cut.cuts ? other.cuts : NO_SHORE_CUTS;
}

static bool CutApart(const ShoreCut &cut, const ShoreCut &other)
{
	return cut.cuts != NO_SHORE_CUTS && other.cuts != NO_SHORE_CUTS && cut.cuts != other.cuts && !Nests(cut, other);
}

static OpenSides Either(const OpenSides &first, const OpenSides &second)
{
	return {first.low_x || second.low_x, first.high_x || second.high_x, first.low_y || second.low_y, first.high_y || second.high_y};
}

struct ShoreSeams {
	int divisions;
	SideCuts coarser;
	OpenSides skirted;
};

template <class CutAt>
static ShoreSeams SeamsOf(const TileSpan &block, int tx, int ty, const CutAt &cut_at)
{
	ShoreCut cut = cut_at(tx, ty);
	Sides<ShoreCut> beside = {cut_at(tx - 1, ty), cut_at(tx + 1, ty), cut_at(tx, ty - 1), cut_at(tx, ty + 1)};
	OpenSides apart = beside.Mapped([&](const ShoreCut &other) { return CutApart(cut, other); });
	return {cut.cuts, beside.Mapped([&](const ShoreCut &other) { return CoarserCuts(cut, other); }), Either(OpenSidesOf(block, {tx, ty, tx, ty}), apart)};
}

static void LayBetween(TerrainVertex &vertex, const TerrainVertex &from, const TerrainVertex &to, double share)
{
	vertex.level = static_cast<float>(std::lerp(static_cast<double>(from.level), static_cast<double>(to.level), share));
	for (size_t byte = 0; byte < vertex.normal.size(); byte++) {
		vertex.normal[byte] = static_cast<int8_t>(std::lround(std::lerp(static_cast<double>(from.normal[byte]), static_cast<double>(to.normal[byte]), share)));
	}
}

static void FollowCoarserSides(TerrainMesh &mesh, std::span<const uint32_t> grid, size_t columns, const SideCuts &coarser)
{
	size_t divisions = columns - 1;
	auto follow = [&](int cuts, auto at) {
		if (cuts == NO_SHORE_CUTS) return;
		size_t span = divisions / cuts;
		for (size_t along = 0; along <= divisions; along++) {
			size_t from = along / span * span;
			if (from == along) continue;
			LayBetween(mesh.vertices[at(along)], mesh.vertices[at(from)], mesh.vertices[at(from + span)], static_cast<double>(along - from) / span);
		}
	};
	follow(coarser.low_y, [&](size_t along) { return grid[along]; });
	follow(coarser.high_y, [&](size_t along) { return grid[divisions * columns + along]; });
	follow(coarser.low_x, [&](size_t along) { return grid[along * columns]; });
	follow(coarser.high_x, [&](size_t along) { return grid[along * columns + divisions]; });
}

void TerrainBuild::AddTile(int tx, int ty)
{
	TerrainMesh &mesh = this->mesh;
	const Seabed &bed = this->land.Bed();
	TileSurface tile(tx, ty);
	std::array<double, CORNER_COUNT> settled = bed.SettledCorners(tx, ty);
	std::array<uint32_t, CORNER_COUNT> corners;
	for (TileCorner corner : {NORTH, WEST, EAST, SOUTH}) {
		WorldPoint at = tile.Corner(corner);
		int cx = static_cast<int>(at.x);
		int cy = static_cast<int>(at.y);
		double sink = at.level - settled[corner];
		Vec3 normal = sink > 0.0 ? bed.Normal(2 * cx, 2 * cy, 1) : this->normals.At(this->land, cx, cy, tile.Level(corner));
		corners[corner] = mesh.Add({at.x, at.y, settled[corner]}, normal, SunkMark(sink));
	}
	LaidGround laid(this->land, tx, ty);
	if (IsRiverside(tx, ty)) {
		AddRelief(mesh, *laid.Relief(), corners, tx, ty, CHANNEL_DIVISIONS);
		return;
	}
	if (const TileRelief *relief = laid.Relief(); relief != nullptr) {
		ShoreSeams seams = SeamsOf(this->tiles, tx, ty, [this](int x, int y) { return this->ShoreCutAt(x, y); });
		std::vector<uint32_t> grid = AddRelief(mesh, *relief, corners, tx, ty, seams.divisions);
		FollowCoarserSides(mesh, grid, seams.divisions + 1, seams.coarser);
		SkirtOpenSides(mesh, grid, seams.divisions + 1, seams.skirted, RELIEF_SKIRT_LEVELS);
		return;
	}
	if (WaterFormOf(tx, ty) == WaterForm::Open) {
		AddBasin(mesh, bed, corners, tx, ty);
		return;
	}
	for (const Facet &facet : tile.Facets()) mesh.Triangle(corners[facet[0]], corners[facet[1]], corners[facet[2]]);
}

int TerrainBuild::DivisionsOf(int tx, int ty) const
{
	int inside_x = std::clamp(tx, this->tiles.tx0, this->tiles.tx1);
	int inside_y = std::clamp(ty, this->tiles.ty0, this->tiles.ty1);
	return this->grain.fine[BlockBit(this->tiles, inside_x, inside_y)] ? FINE_RELIEF_DIVISIONS : 1;
}

static bool StandsInSea(int tx, int ty)
{
	return OnMap(tx, ty) && _world_tiles.WaterAt(TileXY(tx, ty)).sea > 0;
}

static bool AtCorner(const WorldPoint &at)
{
	return at.x == std::floor(at.x) && at.y == std::floor(at.y);
}

static bool SameSpot(const WorldPoint &first, const WorldPoint &second)
{
	return std::abs(first.x - second.x) <= SAME_POINT && std::abs(first.y - second.y) <= SAME_POINT;
}

static bool SamePlace(const WorldPoint &first, const WorldPoint &second)
{
	return SameSpot(first, second) && std::abs(first.level - second.level) <= SAME_POINT;
}

static Groundwork GroundworkAt(int tx, int ty)
{
	TileIndex tile = TileXY(tx, ty);
	return GroundworkOf(_world_tiles.GroundAt(tile), _world_tiles.NetworkAt(tile));
}

static bool StandsBlocked(int tx, int ty)
{
	return IsTunnelTile(TileXY(tx, ty)) || GroundworkAt(tx, ty) == Groundwork::Built;
}

static constexpr std::array<std::array<size_t, 2>, 2> STEP_ENDS = {{{0, 1}, {3, 2}}};

static double FallBeyondEarth(double fall, double reach)
{
	return std::max(0.0, fall - reach * STEEPEST_EARTH / LevelRise());
}

/* How a bank between a tile and a lower one beside it is laid: how far it may reach out over the lower ground, and whether it is earth banked up under a way. */
struct EarthBank {
	double run;
	bool fill;

	/* Whether earth could hold a fall this deep over a reach; a cutting is left rock, however steep. */
	bool HoldsFall(double fall, double reach) const
	{
		return !this->fill || FallBeyondEarth(fall, reach) == 0.0;
	}

	bool Holds(const StepFace &step, const std::array<double, 2> &runs) const
	{
		for (size_t end = 0; end < runs.size(); end++) {
			if (!this->HoldsFall(step.ring[STEP_ENDS[end][0]].level - step.ring[STEP_ENDS[end][1]].level, runs[end])) return false;
		}
		return true;
	}
};

/* Earth banks lean out over the lower tile under ways, short of a way along it and farther over open ground; buildings and tunnel entrances keep stone walls. */
static std::optional<EarthBank> EarthBankOf(int tx, int ty, int nx, int ny)
{
	if (!OnMap(nx, ny) || StandsBlocked(nx, ny)) return std::nullopt;
	Groundwork high = GroundworkAt(tx, ty);
	Groundwork low = GroundworkAt(nx, ny);
	if (high == Groundwork::Built || (high == Groundwork::Open && low == Groundwork::Open)) return std::nullopt;
	bool fill = high == Groundwork::Way;
	if (low == Groundwork::Way) return EarthBank{WAYSIDE_EARTH_BANK_RUN, fill};
	int fx = 2 * nx - tx;
	int fy = 2 * ny - ty;
	bool open_beyond = OnMap(fx, fy) && GroundworkAt(fx, fy) == Groundwork::Open;
	return EarthBank{open_beyond ? FAR_EARTH_BANK_RUN : OPEN_EARTH_BANK_RUN, fill};
}

/* The points down one end of a bank, from its top at the step's top to its foot on the ground, their normals, and the ground under each. */
struct BankEdge {
	std::array<WorldPoint, EARTH_BANK_ROWS + 1> points;
	std::array<Vec3, EARTH_BANK_ROWS + 1> normals;
	std::array<double, EARTH_BANK_ROWS + 1> ground;
};

/* How far down from its top toward the ground a bank has come a share of the way out, levelling off at both ends so it rounds into the way's shoulder and the ground. */
static double BankFall(double share)
{
	return share * share * (3.0 - 2.0 * share);
}

static double BankFallSlope(double share)
{
	return 6.0 * share * (1.0 - share);
}

static double MeshLevelIn(const Landform &land, const LaidGround &laid, int tx, int ty, double x, double y)
{
	if (!LaidGround::RollsAt(land, tx, ty)) return laid.Level(x, y);
	double u = std::clamp(x - tx, 0.0, 1.0) * FINE_RELIEF_DIVISIONS;
	double v = std::clamp(y - ty, 0.0, 1.0) * FINE_RELIEF_DIVISIONS;
	int i = std::min(static_cast<int>(u), FINE_RELIEF_DIVISIONS - 1);
	int j = std::min(static_cast<int>(v), FINE_RELIEF_DIVISIONS - 1);
	u -= i;
	v -= j;
	auto corner = [&](int di, int dj) { return laid.Level(tx + static_cast<double>(i + di) / FINE_RELIEF_DIVISIONS, ty + static_cast<double>(j + dj) / FINE_RELIEF_DIVISIONS); };
	double first = corner(0, 0);
	if (u >= v) return first + u * (corner(1, 0) - first) + v * (corner(1, 1) - corner(1, 0));
	return first + u * (corner(1, 1) - corner(0, 1)) + v * (corner(0, 1) - first);
}

static double MeshLevelAt(const Landform &land, double x, double y)
{
	int tx = static_cast<int>(std::floor(x));
	int ty = static_cast<int>(std::floor(y));
	return OnMap(tx, ty) ? MeshLevelIn(land, LaidGround(land, tx, ty), tx, ty, x, y) : land.LevelAt(x, y);
}

struct BankProfile {
	double (*fall)(double share);
	double (*slope)(double share);
};

static double StraightFall(double share)
{
	return share;
}

static double StraightFallSlope(double)
{
	return 1.0;
}

static constexpr BankProfile EASED_PROFILE = {BankFall, BankFallSlope};
static constexpr BankProfile STRAIGHT_PROFILE = {StraightFall, StraightFallSlope};

static double EarthReach(const WorldPoint &top, const WorldPoint &foot, double run_per_rise, double run)
{
	return std::clamp((top.level - foot.level) * LevelRise() * run_per_rise, 0.0, run);
}

/* One end of a bank: from its top it grades out over the lower ground, a reach out, onto the ground there;
 * the ground is read a point and how far out it lies. */
template <class GroundUnder>
static BankEdge BankEnd(const Landform &land, const GroundUnder &ground_under, const WorldPoint &top, const WorldPoint &foot, const MapVector &outward, double reach, const BankProfile &profile, const MapVector &top_climb = {}, double foot_dip = EARTH_BANK_FOOT_DIP)
{
	BankEdge edge;
	for (int row = 0; row <= EARTH_BANK_ROWS; row++) {
		double share = static_cast<double>(row) / EARTH_BANK_ROWS;
		double out = reach * share;
		double x = foot.x + outward.x * out;
		double y = foot.y + outward.y * out;
		double under = ground_under(x, y, out);
		double sunk = std::min(under - foot_dip * share, top.level);
		double fall = profile.fall(share);
		MapVector climb = land.Climb(x, y) * fall + top_climb * (1.0 - fall) - outward * ((top.level - sunk) * profile.slope(share) / std::max(reach, CLIMB_PROBE));
		edge.points[row] = {x, y, top.level + (sunk - top.level) * fall};
		edge.normals[row] = ClimbNormal(climb);
		edge.ground[row] = std::min(under - BANK_FOOT_DIP * share, top.level);
	}
	edge.normals[EARTH_BANK_ROWS] = GroundNormalAt(land, edge.points[EARTH_BANK_ROWS].x, edge.points[EARTH_BANK_ROWS].y);
	if (AtCorner(top)) edge.normals[0] = SmoothNormal(static_cast<int>(top.x), static_cast<int>(top.y), static_cast<uint8_t>(top.level));
	return edge;
}

static double BankMark(const WorldPoint &at, double dry_mark)
{
	int tx = static_cast<int>(std::floor(at.x));
	int ty = static_cast<int>(std::floor(at.y));
	if (!OnMap(tx, ty) || WaterFormOf(tx, ty) == WaterForm::Dry) return dry_mark;
	double water = WaterSheet(tx, ty).Level(at.x, at.y);
	return at.level < water ? SunkMark(water - at.level) : dry_mark;
}

struct StripVertex {
	WorldPoint at;
	Vec3 normal;
	double mark;
};

using StripRow = std::array<StripVertex, 2>;

template <class Footing>
static StripVertex CrestVertexOn(const Footing &footing, const WorldPoint &at)
{
	return {at, ClimbNormal(ClimbOf(footing, at.x, at.y)), FILL_MARK};
}

static void AddStrip(TerrainMesh &mesh, std::span<const StripRow> rows)
{
	uint32_t previous = 0;
	for (size_t row = 0; row < rows.size(); row++) {
		uint32_t near = mesh.Add(rows[row][0].at, rows[row][0].normal, rows[row][0].mark);
		mesh.Add(rows[row][1].at, rows[row][1].normal, rows[row][1].mark);
		if (row > 0) mesh.Quad(previous, previous + 1, near + 1, near);
		previous = near;
	}
}

static void AddBankStrip(TerrainMesh &mesh, const BankEdge &first, const BankEdge &second, double mark)
{
	std::vector<StripRow> rows;
	for (size_t row = 0; row < first.points.size(); row++) {
		rows.push_back({StripVertex{first.points[row], first.normals[row], BankMark(first.points[row], mark)}, StripVertex{second.points[row], second.normals[row], BankMark(second.points[row], mark)}});
	}
	AddStrip(mesh, rows);
}

static BankEdge Spanned(const StripRow &row)
{
	BankEdge edge{};
	for (size_t point = 0; point < edge.points.size(); point++) {
		double share = static_cast<double>(point) / EARTH_BANK_ROWS;
		edge.points[point] = Between(row[0].at, row[1].at, share);
		edge.normals[point] = Normalised(Lerp(row[0].normal, row[1].normal, share));
	}
	return edge;
}

static void AddBankColumns(TerrainMesh &mesh, std::span<const BankEdge> columns, double mark)
{
	for (size_t column = 0; column + 1 < columns.size(); column++) AddBankStrip(mesh, columns[column], columns[column + 1], mark);
}

static Vec3 Upright(const MapVector &facing)
{
	return {facing.x, facing.y, 0.0};
}

static uint32_t AddStoneCorners(TerrainMesh &mesh, std::span<const WorldPoint> corners, const Vec3 &normal)
{
	uint32_t first = mesh.Add(corners[0], normal, MASONRY_MARK);
	for (size_t corner = 1; corner < corners.size(); corner++) mesh.Add(corners[corner], normal, MASONRY_MARK);
	return first;
}

static void AddStoneQuad(TerrainMesh &mesh, const std::array<WorldPoint, 4> &corners, const Vec3 &normal)
{
	uint32_t first = AddStoneCorners(mesh, corners, normal);
	mesh.Quad(first, first + 1, first + 2, first + 3);
}

static void AddStoneTriangle(TerrainMesh &mesh, const std::array<WorldPoint, 3> &corners, const Vec3 &normal)
{
	uint32_t first = AddStoneCorners(mesh, corners, normal);
	mesh.Triangle(first, first + 1, first + 2);
}

/* A stone face from a line of points down to the feet under them, facing out one way. */
static void AddMasonry(TerrainMesh &mesh, std::span<const WorldPoint> line, std::span<const WorldPoint> feet, const MapVector &facing)
{
	Vec3 outward = Upright(facing);
	for (size_t i = 0; i + 1 < line.size(); i++) AddStoneQuad(mesh, {line[i], line[i + 1], feet[i + 1], feet[i]}, outward);
}

static WorldPoint Grounded(const Landform &land, const WorldPoint &at)
{
	return {at.x, at.y, land.LevelAt(at.x, at.y) - BANK_FOOT_DIP};
}

static std::vector<WorldPoint> GroundedLine(const Landform &land, std::span<const WorldPoint> line)
{
	std::vector<WorldPoint> feet;
	for (const WorldPoint &at : line) feet.push_back(Grounded(land, at));
	return feet;
}

struct TileStep {
	int x;
	int y;
};

static int SignOf(double value)
{
	return (value > 0.0) - (value < 0.0);
}

struct StepBank {
	StepFace step;
	EarthBank bank;
	int high_x;
	int high_y;
	int low_x;
	int low_y;
};

static std::optional<StepBank> StepBankOn(int tx, int ty, DiagDirection side)
{
	if (!OnMap(tx, ty)) return std::nullopt;
	std::optional<StepFace> step = StepFaceOf(tx, ty, side);
	if (!step.has_value()) return std::nullopt;
	TileIndexDiffC across = TileIndexDiffCByDiagDir(side);
	int nx = tx + across.x;
	int ny = ty + across.y;
	if (StandsInSea(nx, ny)) return std::nullopt;
	std::optional<EarthBank> bank = EarthBankOf(tx, ty, nx, ny);
	if (!bank.has_value()) return std::nullopt;
	return StepBank{*step, *bank, tx, ty, nx, ny};
}

static TileStep BeyondEnd(const StepFace &step, size_t end)
{
	const WorldPoint &first = step.ring[STEP_ENDS[0][0]];
	const WorldPoint &last = step.ring[STEP_ENDS[1][0]];
	int along_x = SignOf(last.x - first.x);
	int along_y = SignOf(last.y - first.y);
	return end == 0 ? TileStep{-along_x, -along_y} : TileStep{along_x, along_y};
}

static std::array<double, 2> EndRuns(int tx, int ty, DiagDirection side, const StepBank &own)
{
	std::array<double, 2> runs;
	for (size_t end = 0; end < runs.size(); end++) {
		TileStep beyond = BeyondEnd(own.step, end);
		std::optional<StepBank> next = StepBankOn(tx + beyond.x, ty + beyond.y, side);
		runs[end] = next.has_value() ? std::min(own.bank.run, next->bank.run) : own.bank.run;
	}
	return runs;
}

struct BankedStep {
	StepBank stepped;
	std::array<double, 2> runs;
};

static std::optional<BankedStep> BankOnPlain(int tx, int ty, DiagDirection side)
{
	std::optional<StepBank> stepped = StepBankOn(tx, ty, side);
	if (!stepped.has_value()) return std::nullopt;
	std::array<double, 2> runs = EndRuns(tx, ty, side, *stepped);
	if (!stepped->bank.Holds(stepped->step, runs)) return std::nullopt;
	return BankedStep{*stepped, runs};
}

static bool IsRiverWater(int tx, int ty)
{
	return OnMap(tx, ty) && (IsChannel(tx, ty) || IsRapids(tx, ty));
}

template <class BankOnSide>
static bool Walled(int tx, int ty, DiagDirection side, BankOnSide bank_on)
{
	return StepFaceOf(tx, ty, side).has_value() && !bank_on(tx, ty, side).has_value();
}

static std::optional<double> StepTopAt(const StepFace &step, const WorldPoint &corner)
{
	for (const std::array<size_t, 2> &end : STEP_ENDS) {
		if (SameSpot(step.ring[end[0]], corner)) return step.ring[end[0]].level;
	}
	return std::nullopt;
}

template <class Meets>
static bool AnyStepAtCorner(const WorldPoint &corner, const Meets &meets)
{
	for (const CornerTile &around : TilesAtCorner(static_cast<int>(corner.x), static_cast<int>(corner.y))) {
		if (!OnMap(around.tx, around.ty)) continue;
		for (DiagDirection side = DIAGDIR_BEGIN; side < DIAGDIR_END; side++) {
			std::optional<StepFace> step = StepFaceOf(around.tx, around.ty, side);
			if (step.has_value() && meets(around.tx, around.ty, side, *step)) return true;
		}
	}
	return false;
}

template <class Admits>
static bool WallRisesAt(int tx, int ty, DiagDirection own_side, const WorldPoint &corner, const Admits &admits)
{
	return AnyStepAtCorner(corner, [&](int around_x, int around_y, DiagDirection side, const StepFace &step) {
		bool own = around_x == tx && around_y == ty && side == own_side;
		std::optional<double> top = StepTopAt(step, corner);
		return !own && top.has_value() && admits(*top) && Walled(around_x, around_y, side, BankOnPlain);
	});
}

static bool IsPortalTile(int tx, int ty)
{
	return OnMap(tx, ty) && IsTunnelTile(TileXY(tx, ty));
}

static double RiseAtEnd(const StepFace &step, const std::array<size_t, 2> &end)
{
	return step.ring[end[0]].level - step.ring[end[1]].level;
}

static bool ShutOutByCutting(int tx, int ty, DiagDirection side, const StepFace &step)
{
	double lowest_rise = std::min(RiseAtEnd(step, STEP_ENDS[0]), RiseAtEnd(step, STEP_ENDS[1]));
	for (const std::array<size_t, 2> &end : STEP_ENDS) {
		const WorldPoint &corner = step.ring[end[0]];
		if (!AtCorner(corner)) continue;
		std::array<CornerTile, 4> around = TilesAtCorner(static_cast<int>(corner.x), static_cast<int>(corner.y));
		bool at_foot_end = RiseAtEnd(step, end) <= lowest_rise;
		bool on_river = at_foot_end && std::ranges::any_of(around, [](const CornerTile &tile) { return IsRiverWater(tile.tx, tile.ty); });
		bool on_portal = std::ranges::any_of(around, [](const CornerTile &tile) { return IsPortalTile(tile.tx, tile.ty); });
		if (!on_river && !on_portal) continue;
		if (WallRisesAt(tx, ty, side, corner, [&](double top) { return on_river || top <= corner.level - SAME_POINT; })) return true;
	}
	return false;
}

static bool TaperingOnLevelFoot(const StepFace &step)
{
	if (std::abs(step.ring[STEP_ENDS[0][1]].level - step.ring[STEP_ENDS[1][1]].level) > SAME_POINT) return false;
	double low_rise = std::min(RiseAtEnd(step, STEP_ENDS[0]), RiseAtEnd(step, STEP_ENDS[1]));
	double high_rise = std::max(RiseAtEnd(step, STEP_ENDS[0]), RiseAtEnd(step, STEP_ENDS[1]));
	return low_rise < SAME_POINT && high_rise > SAME_POINT;
}

static bool MeetAtHighEnd(const StepFace &first, const StepFace &second)
{
	for (const std::array<size_t, 2> &first_end : STEP_ENDS) {
		for (const std::array<size_t, 2> &second_end : STEP_ENDS) {
			bool high = RiseAtEnd(first, first_end) > SAME_POINT && RiseAtEnd(second, second_end) > SAME_POINT;
			if (high && SameSpot(first.ring[first_end[0]], second.ring[second_end[0]]) && AtCorner(first.ring[first_end[0]])) return true;
		}
	}
	return false;
}

static bool FoldsIntoPit(const StepBank &own)
{
	if (WaterFormOf(own.low_x, own.low_y) != WaterForm::Dry || !TaperingOnLevelFoot(own.step)) return false;
	for (DiagDirection toward = DIAGDIR_BEGIN; toward < DIAGDIR_END; toward++) {
		TileIndexDiffC across = TileIndexDiffCByDiagDir(toward);
		int high_x = own.low_x + across.x;
		int high_y = own.low_y + across.y;
		if (high_x == own.high_x && high_y == own.high_y) continue;
		std::optional<StepBank> other = StepBankOn(high_x, high_y, ReverseDiagDir(toward));
		if (other.has_value() && TaperingOnLevelFoot(other->step) && MeetAtHighEnd(own.step, other->step)) return true;
	}
	return false;
}

static std::optional<BankedStep> BankOn(int tx, int ty, DiagDirection side)
{
	std::optional<BankedStep> site = BankOnPlain(tx, ty, side);
	return site.has_value() && !ShutOutByCutting(tx, ty, side, site->stepped.step) && !FoldsIntoPit(site->stepped) ? site : std::nullopt;
}

class BankColumns {
public:
	BankColumns(const Landform &land, const BankedStep &site) :
		land(land), step(site.stepped.step), runs(site.runs), high_x(site.stepped.high_x), high_y(site.stepped.high_y), low_x(site.stepped.low_x), low_y(site.stepped.low_y), low(land, site.stepped.low_x, site.stepped.low_y)
	{
	}

	std::array<BankEdge, 2> Ends() const
	{
		return {this->EndAt(0), this->EndAt(1)};
	}

	std::vector<BankEdge> Across(const std::array<BankEdge, 2> &ends) const
	{
		double length = Length(Flat(this->step.ring[STEP_ENDS[1][0]]) - Flat(this->step.ring[STEP_ENDS[0][0]]));
		int count = std::max(1, static_cast<int>(std::ceil(length * BANK_COLUMNS_PER_TILE)));
		LaidGround high(this->land, this->high_x, this->high_y);
		std::vector<BankEdge> columns = {ends[0]};
		for (int column = 1; column < count; column++) columns.push_back(this->Interior(high, static_cast<double>(column) / count));
		columns.push_back(ends[1]);
		return columns;
	}

private:
	BankEdge EndAt(size_t end) const
	{
		return this->Column(this->step.ring[STEP_ENDS[end][0]], this->step.ring[STEP_ENDS[end][1]], static_cast<double>(end));
	}

	BankEdge Interior(const LaidGround &high, double share) const
	{
		WorldPoint top = ::Between(this->step.ring[STEP_ENDS[0][0]], this->step.ring[STEP_ENDS[1][0]], share);
		WorldPoint foot = ::Between(this->step.ring[STEP_ENDS[0][1]], this->step.ring[STEP_ENDS[1][1]], share);
		top.level = high.Level(top.x, top.y);
		foot.level = this->low.Level(foot.x, foot.y);
		return this->Column(top, foot, share);
	}

	BankEdge Column(const WorldPoint &top, const WorldPoint &foot, double share) const
	{
		double run = std::lerp(this->runs[0], this->runs[1], share);
		auto ground_under = [this](double x, double y, double out) { return out <= 1.0 ? MeshLevelIn(this->land, this->low, this->low_x, this->low_y, x, y) : MeshLevelAt(this->land, x, y); };
		MapVector top_climb = this->land.Climb(top.x - this->step.outward.x * BANK_TOP_INSET, top.y - this->step.outward.y * BANK_TOP_INSET);
		double foot_dip = std::lerp(SHALLOW_BANK_FOOT_DIP, EARTH_BANK_FOOT_DIP, std::clamp((top.level - foot.level) / SHALLOW_BANK_RISE, 0.0, 1.0));
		return BankEnd(this->land, ground_under, top, foot, this->step.outward, EarthReach(top, foot, EARTH_BANK_RUN_PER_RISE, run), EASED_PROFILE, top_climb, foot_dip);
	}

	const Landform &land;
	StepFace step;
	std::array<double, 2> runs;
	int high_x;
	int high_y;
	int low_x;
	int low_y;
	LaidGround low;
};

static double MarkOf(const EarthBank &bank)
{
	return bank.fill ? FILL_MARK : DRY_MARK;
}

static bool CoversWall(const BankEdge &edge, const StepFace &step, const StepFace &wall)
{
	const WorldPoint &corner = edge.points[0];
	MapVector line = Unit(Flat(step.ring[STEP_ENDS[1][0]]) - Flat(step.ring[STEP_ENDS[0][0]]));
	if (std::abs(Dot(wall.outward, line)) < PARALLEL_LEAST || std::abs(Dot(Flat(wall.ring[0]) - Flat(corner), wall.outward)) > SAME_POINT) return false;
	auto reach = [&](const WorldPoint &at) { return Dot(Flat(at) - Flat(corner), step.outward); };
	double wall_from = reach(wall.ring[0]);
	double wall_to = reach(wall.ring[3]);
	if (wall_from == wall_to) return false;
	for (int row = 0; row <= EARTH_BANK_ROWS; row++) {
		double share = (reach(edge.points[row]) - wall_from) / (wall_to - wall_from);
		if (share < 0.0 || share > 1.0) continue;
		double wall_top = std::lerp(wall.ring[0].level, wall.ring[3].level, share);
		double wall_foot = std::lerp(wall.ring[1].level, wall.ring[2].level, share);
		double covered = std::min(edge.points[row].level, wall_top) - std::max(edge.ground[row] - BANK_FOOT_DIP, wall_foot);
		if (covered > WALL_OVERLAP_LEAST) return true;
	}
	return false;
}

struct PlacedBank {
	BankedStep site;
	std::vector<BankEdge> columns;
	std::array<BankEdge, 2> edges;
};

static bool Converted(const BankedStep &site, const std::array<BankEdge, 2> &edges)
{
	for (size_t end = 0; end < edges.size(); end++) {
		const WorldPoint &corner = edges[end].points[0];
		if (!AtCorner(corner)) continue;
		auto covers = [&](int around_x, int around_y, DiagDirection side, const StepFace &wall) {
			return Walled(around_x, around_y, side, BankOn) && CoversWall(edges[end], site.stepped.step, wall);
		};
		if (AnyStepAtCorner(corner, covers)) return true;
	}
	return false;
}

static std::optional<PlacedBank> BankAt(const Landform &land, int tx, int ty, DiagDirection side)
{
	std::optional<BankedStep> site = BankOn(tx, ty, side);
	if (!site.has_value()) return std::nullopt;
	BankColumns columns(land, *site);
	std::array<BankEdge, 2> edges = columns.Ends();
	if (Converted(*site, edges)) return std::nullopt;
	return PlacedBank{*site, columns.Across(edges), edges};
}

static bool MeetAtCorner(const BankEdge &first, const BankEdge &second)
{
	return SameSpot(first.points[0], second.points[0]) && AtCorner(first.points[0]);
}

/* Two banks grading out from the sides of a corner leave a wedge open between their ends, which a strip closes. */
static void CloseBankCorners(TerrainMesh &mesh, std::span<const PlacedBank> banks)
{
	for (size_t a = 0; a < banks.size(); a++) {
		for (size_t b = a + 1; b < banks.size(); b++) {
			for (const BankEdge &first : banks[a].edges) {
				for (const BankEdge &second : banks[b].edges) {
					if (MeetAtCorner(first, second)) AddBankStrip(mesh, first, second, std::max(MarkOf(banks[a].site.stepped.bank), MarkOf(banks[b].site.stepped.bank)));
				}
			}
		}
	}
}

static bool ClosedAtCorner(std::span<const PlacedBank> banks, size_t own, size_t end)
{
	for (size_t other = 0; other < banks.size(); other++) {
		if (other == own) continue;
		if (std::ranges::any_of(banks[other].edges, [&](const BankEdge &edge) { return MeetAtCorner(banks[own].edges[end], edge); })) return true;
	}
	return false;
}

static bool SameEnd(const BankEdge &first, const BankEdge &second)
{
	return SamePlace(first.points[0], second.points[0]) && SamePlace(first.points[EARTH_BANK_ROWS], second.points[EARTH_BANK_ROWS]);
}

static bool RunsOn(const Landform &land, int tx, int ty, DiagDirection side, const PlacedBank &bank, size_t end)
{
	TileStep beyond = BeyondEnd(bank.site.stepped.step, end);
	std::optional<PlacedBank> next = BankAt(land, tx + beyond.x, ty + beyond.y, side);
	return next.has_value() && SameEnd(bank.edges[end], next->edges[1 - end]);
}

static bool Reaches(const BankEdge &edge)
{
	return !SameSpot(edge.points[0], edge.points[EARTH_BANK_ROWS]);
}

static void AddBankEnd(TerrainMesh &mesh, const BankEdge &edge, const MapVector &facing)
{
	std::array<WorldPoint, EARTH_BANK_ROWS + 1> feet;
	for (size_t row = 0; row < feet.size(); row++) feet[row] = {edge.points[row].x, edge.points[row].y, std::min(edge.points[row].level, edge.ground[row] - BANK_FOOT_DIP)};
	AddMasonry(mesh, edge.points, feet, facing);
}

static WorldPoint Shifted(const WorldPoint &at, const MapVector &by, double rise)
{
	return {at.x + by.x, at.y + by.y, at.level + rise / LevelRise()};
}

struct CopingEnd {
	WorldPoint front_foot;
	WorldPoint front_top;
	WorldPoint back_foot;
	WorldPoint back_top;
};

static CopingEnd CopingAt(const WorldPoint &at, const MapVector &facing, double depth)
{
	MapVector front = facing * COPING_OVERHANG;
	MapVector back = facing * -depth;
	return {Shifted(at, front, 0.0), Shifted(at, front, COPING_RISE), Shifted(at, back, 0.0), Shifted(at, back, COPING_RISE)};
}

static std::vector<WorldPoint> ShiftedLine(std::span<const WorldPoint> line, const MapVector &by)
{
	std::vector<WorldPoint> shifted;
	for (const WorldPoint &at : line) shifted.push_back(Shifted(at, by, 0.0));
	return shifted;
}

static void CloseCopingEnd(TerrainMesh &mesh, const WorldPoint &at, const MapVector &facing, double depth, const MapVector &end_facing)
{
	CopingEnd end = CopingAt(at, facing, depth);
	AddStoneQuad(mesh, {end.front_foot, end.front_top, end.back_top, end.back_foot}, Upright(end_facing));
}

static void CloseWallEnd(TerrainMesh &mesh, const Landform &land, const WorldPoint &front, const WorldPoint &back, const MapVector &facing, const MapVector &end_facing, double thickness)
{
	AddStoneQuad(mesh, {front, back, Grounded(land, back), Grounded(land, front)}, Upright(end_facing));
	CloseCopingEnd(mesh, front, facing, CopingDepthFor(thickness), end_facing);
}

template <class FacingAt>
static void AddCoping(TerrainMesh &mesh, std::span<const WorldPoint> front, const FacingAt &facing_at, double depth)
{
	for (size_t i = 0; i + 1 < front.size(); i++) {
		MapVector facing = Unit(facing_at(i) + facing_at(i + 1));
		Vec3 outward = Upright(facing);
		Vec3 inward = outward * -1.0;
		Vec3 along = {front[i + 1].x - front[i].x, front[i + 1].y - front[i].y, (front[i + 1].level - front[i].level) * LevelRise()};
		Vec3 cap = Normalised(Cross(inward, along));
		if (Length(cap) == 0.0) cap = UP;
		if (cap.z < 0.0) cap = cap * -1.0;
		CopingEnd from = CopingAt(front[i], facing_at(i), depth);
		CopingEnd to = CopingAt(front[i + 1], facing_at(i + 1), depth);
		AddStoneQuad(mesh, {from.front_top, to.front_top, to.front_foot, from.front_foot}, outward);
		AddStoneQuad(mesh, {from.back_top, to.back_top, to.front_top, from.front_top}, cap);
		AddStoneQuad(mesh, {from.back_foot, to.back_foot, to.back_top, from.back_top}, inward);
	}
}

static void AddCoping(TerrainMesh &mesh, std::span<const WorldPoint> front, const MapVector &facing, double depth)
{
	AddCoping(mesh, front, [&](size_t) { return facing; }, depth);
}

/* A wing wall is a slab of stone as thick as given, its face and back running down to the ground and its ends closed, under a coping along its top and round its end. */
static void AddWingWall(TerrainMesh &mesh, const Landform &land, std::span<const WorldPoint> front, const MapVector &facing, const MapVector &onward, bool closed_both, double thickness)
{
	std::vector<WorldPoint> back = ShiftedLine(front, facing * -thickness);
	AddMasonry(mesh, front, GroundedLine(land, front), facing);
	std::ranges::reverse(back);
	AddMasonry(mesh, back, GroundedLine(land, back), facing * -1.0);
	std::ranges::reverse(back);
	CloseWallEnd(mesh, land, front.back(), back.back(), facing, onward, thickness);
	if (closed_both) CloseWallEnd(mesh, land, front.front(), back.front(), facing, onward * -1.0, thickness);
	AddCoping(mesh, front, facing, CopingDepthFor(thickness));
}

static std::optional<std::array<double, 2>> CopedShares(double from_rise, double to_rise)
{
	if (std::max(from_rise, to_rise) < COPING_RISE) return std::nullopt;
	double first = from_rise >= COPING_RISE ? 0.0 : (COPING_RISE - from_rise) / (to_rise - from_rise);
	double last = to_rise >= COPING_RISE ? 1.0 : (from_rise - COPING_RISE) / (from_rise - to_rise);
	return std::array<double, 2>{first, last};
}

static void AddCopingWhereTall(TerrainMesh &mesh, std::span<const WorldPoint> line, std::span<const WorldPoint> feet, const MapVector &facing, double depth)
{
	auto rise = [&](size_t i) { return (line[i].level - feet[i].level) * LevelRise(); };
	for (size_t i = 0; i + 1 < line.size(); i++) {
		std::optional<std::array<double, 2>> shares = CopedShares(rise(i), rise(i + 1));
		if (!shares.has_value()) continue;
		std::array<WorldPoint, 2> top = {Between(line[i], line[i + 1], (*shares)[0]), Between(line[i], line[i + 1], (*shares)[1])};
		AddCoping(mesh, top, facing, depth);
		MapVector onward = Unit(Flat(top[1]) - Flat(top[0]));
		if (i == 0 || (*shares)[0] > 0.0) CloseCopingEnd(mesh, top[0], facing, depth, onward * -1.0);
		if (i + 2 == line.size() || (*shares)[1] < 1.0) CloseCopingEnd(mesh, top[1], facing, depth, onward);
	}
}

template <class FacingAt>
static void AddCopingAlong(TerrainMesh &mesh, std::span<const WorldPoint> line, const FacingAt &facing_at, double depth)
{
	AddCoping(mesh, line, facing_at, depth);
	size_t last = line.size() - 1;
	CloseCopingEnd(mesh, line[0], facing_at(0), depth, Unit(Flat(line[0]) - Flat(line[1])));
	CloseCopingEnd(mesh, line[last], facing_at(last), depth, Unit(Flat(line[last]) - Flat(line[last - 1])));
}

static void AddWall(TerrainMesh &mesh, const StepFace &step)
{
	Vec3 outward = {step.outward.x, step.outward.y, 0.0};
	std::array<uint32_t, 4> ring;
	std::ranges::transform(step.ring, ring.begin(), [&](const WorldPoint &corner) { return mesh.Add(corner, outward, WALL_MARK); });
	mesh.Quad(ring[0], ring[1], ring[2], ring[3]);
	std::array<WorldPoint, 2> line = {step.ring[STEP_ENDS[0][0]], step.ring[STEP_ENDS[1][0]]};
	std::array<WorldPoint, 2> feet = {step.ring[STEP_ENDS[0][1]], step.ring[STEP_ENDS[1][1]]};
	AddCopingWhereTall(mesh, line, feet, step.outward, COPING_OVERHANG);
}

static void AddWalls(TerrainMesh &mesh, const Landform &land, int tx, int ty)
{
	std::vector<PlacedBank> banks;
	std::vector<DiagDirection> sides;
	for (DiagDirection side = DIAGDIR_BEGIN; side < DIAGDIR_END; side++) {
		std::optional<StepFace> step = StepFaceOf(tx, ty, side);
		if (!step.has_value()) continue;
		std::optional<PlacedBank> placed = BankAt(land, tx, ty, side);
		if (!placed.has_value()) {
			AddWall(mesh, *step);
			continue;
		}
		banks.push_back(*placed);
		sides.push_back(side);
		AddBankColumns(mesh, placed->columns, MarkOf(placed->site.stepped.bank));
	}
	CloseBankCorners(mesh, banks);
	for (size_t own = 0; own < banks.size(); own++) {
		const StepFace &step = banks[own].site.stepped.step;
		for (size_t end = 0; end < banks[own].edges.size(); end++) {
			const BankEdge &edge = banks[own].edges[end];
			if (!Reaches(edge) || ClosedAtCorner(banks, own, end) || RunsOn(land, tx, ty, sides[own], banks[own], end)) continue;
			AddBankEnd(mesh, edge, Unit(Flat(step.ring[STEP_ENDS[end][0]]) - Flat(step.ring[STEP_ENDS[1 - end][0]])));
		}
	}
}

struct Crossing {
	double before;
	double after;
};

template <class Test>
static std::optional<Crossing> FirstHolding(const Test &holds)
{
	double step = 1.0 / FORMATION_TOE_STEPS;
	double high = step;
	while (high <= 1.0 && !holds(high)) high += step;
	if (high > 1.0) return std::nullopt;
	double low = high - step;
	for (int refine = 0; refine < FORMATION_TOE_REFINES; refine++) {
		double middle = (low + high) / 2.0;
		(holds(middle) ? high : low) = middle;
	}
	return Crossing{low, high};
}

static bool StopsEarth(int tx, int ty)
{
	return OnMap(tx, ty) && (StandsBlocked(tx, ty) || GroundworkAt(tx, ty) == Groundwork::Way || WaterFormOf(tx, ty) != WaterForm::Dry);
}

struct FormationSide {
	WorldPoint crest;
	Vec3 crest_normal;
	WorldPoint toe;
	Vec3 toe_normal;
	std::optional<BankEdge> section = std::nullopt;
	bool walled = false;
	bool shelved = false;
};

using FormationRow = std::array<FormationSide, 2>;

class RampShape {
public:
	enum class Flank : uint8_t {
		Merged,
		Shelf,
		Bank,
		Wall,
	};

	struct FlankPlan {
		Flank kind;
		double run;
		std::optional<StepFace> step = std::nullopt;
	};

	RampShape(const Landform &land, int tx, int ty, const RampTexel &ramp) :
		land(land), tx(tx), ty(ty), ramp(ramp), onto(static_cast<DiagDirection>(ramp.onto)),
		footing(RampFooting(TileXY(tx, ty), this->onto, ramp.deck)), run(RampRun(TileXY(tx, ty), this->onto)), facing(Outward(this->onto))
	{
	}

	const Landform &Land() const { return this->land; }
	const MapVector &Facing() const { return this->facing; }
	MapVector SideOutward(double side) const { return this->run.Right() * side; }

	static double ShareOfRow(int row) { return static_cast<double>(row) / RAMP_BANK_ROWS; }

	WorldPoint Crest(double share, double side) const
	{
		MapVector at = this->run.At(share, side * DECK_HALF);
		return {at.x, at.y, this->footing(at.x, at.y) + WAY_FOOT / LevelRise()};
	}

	StripVertex CrestVertex(double share, double side) const
	{
		return CrestVertexOn(this->footing, this->Crest(share, side));
	}

	FlankPlan PlanOf(double side) const
	{
		TileStep beside = this->SideStep(side);
		int sx = this->tx + beside.x;
		int sy = this->ty + beside.y;
		if (this->BesideParallelRamp(sx, sy)) return {Flank::Merged, 0.0};
		if (Walled(this->tx, this->ty, this->SideToward(side), BankOn)) return {Flank::Shelf, 0.0, StepFaceOf(this->tx, this->ty, this->SideToward(side))};
		if (StopsEarth(sx, sy) || StopsEarth(sx + static_cast<int>(this->facing.x), sy + static_cast<int>(this->facing.y))) return {Flank::Wall, 0.0};
		std::optional<EarthBank> bank = EarthBankOf(this->tx, this->ty, sx, sy);
		if (!bank.has_value()) return {Flank::Wall, 0.0};
		double reach = std::min(RAMP_BANK_RUN, HALF_TILE - DECK_HALF + bank->run);
		return bank->HoldsFall(this->HeadFall(side), reach) ? FlankPlan{Flank::Bank, reach} : FlankPlan{Flank::Wall, 0.0};
	}

	BankEdge EdgeAt(int row, double side, double run_limit) const
	{
		WorldPoint top = this->Crest(ShareOfRow(row), side);
		WorldPoint foot = Grounded(this->land, top);
		MapVector outward = this->SideOutward(side);
		double natural = EarthReach(top, foot, RAMP_BANK_RUN_PER_RISE, run_limit);
		auto ground_under = [this](double x, double y, double) { return MeshLevelAt(this->land, x, y); };
		return BankEnd(this->land, ground_under, top, foot, outward, this->ReachBefore(top, outward, natural), STRAIGHT_PROFILE, ClimbOf(this->footing, top.x, top.y));
	}

	WorldPoint ShelfEdge(double share, double side) const
	{
		WorldPoint crest = this->Crest(share, side);
		MapVector at = this->run.At(share, side * HALF_TILE);
		return {at.x, at.y, crest.level};
	}

	/* Where the earth between two heads side by side reaches their shared border, level with its own crest. */
	WorldPoint MergedEdge(double share, double side) const
	{
		MapVector at = this->run.At(share, side * HALF_TILE);
		return {at.x, at.y, this->footing(at.x, at.y) + WAY_FOOT / LevelRise()};
	}

	/* The other head's earth at the shared border, which a higher head's stands above. */
	WorldPoint MergedFoot(double share, double side) const
	{
		MapVector at = this->run.At(share, side * HALF_TILE);
		Footing beside = this->BesideFooting(side);
		return {at.x, at.y, std::min(this->footing(at.x, at.y), beside(at.x, at.y)) + WAY_FOOT / LevelRise()};
	}

	/* Where the earth between two heads meets the ways leading up to them, settled onto the ground where it barely stands above it. */
	WorldPoint MergedToe(double side) const
	{
		WorldPoint foot = this->MergedFoot(0.0, side);
		WorldPoint ground = Grounded(this->land, foot);
		return foot.level - ground.level < SHALLOWEST_CAP ? ground : foot;
	}

	WorldPoint BandEdge(Flank kind, double share, double side) const
	{
		return kind == Flank::Merged ? this->MergedEdge(share, side) : this->ShelfEdge(share, side);
	}

	WorldPoint ShelfFoot(const StepFace &step, double share, double side) const
	{
		WorldPoint edge = this->ShelfEdge(share, side);
		const WorldPoint &first = step.ring[STEP_ENDS[0][0]];
		const WorldPoint &last = step.ring[STEP_ENDS[1][0]];
		MapVector top = Flat(last) - Flat(first);
		double along = std::clamp(Dot(Flat(edge) - Flat(first), top) / Dot(top, top), 0.0, 1.0);
		edge.level = std::min(edge.level, std::lerp(first.level, last.level, along));
		return edge;
	}

	FormationSide OuterSide(double side) const
	{
		FlankPlan plan = this->PlanOf(side);
		if (plan.kind == Flank::Shelf || plan.kind == Flank::Merged) {
			Vec3 normal = this->CrestVertex(0.0, side).normal;
			WorldPoint toe = plan.kind == Flank::Merged ? this->MergedToe(side) : this->ShelfEdge(0.0, side);
			return {this->Crest(0.0, side), normal, toe, normal, std::nullopt, false, plan.kind == Flank::Shelf};
		}
		if (plan.kind == Flank::Wall) {
			WorldPoint crest = this->Crest(0.0, side);
			WorldPoint toe = Grounded(this->land, crest);
			return {crest, this->CrestVertex(0.0, side).normal, toe, ClimbNormal(this->land.Climb(toe.x, toe.y)), std::nullopt, true};
		}
		BankEdge edge = this->EdgeAt(0, side, plan.run);
		return {edge.points.front(), edge.normals.front(), edge.points.back(), edge.normals.back(), edge};
	}

private:
	TileStep SideStep(double side) const
	{
		MapVector right = this->SideOutward(side);
		return {static_cast<int>(std::lround(right.x)), static_cast<int>(std::lround(right.y))};
	}

	DiagDirection SideToward(double side) const
	{
		TileStep beside = this->SideStep(side);
		return DiagdirBetweenTiles(TileXY(this->tx, this->ty), TileXY(this->tx + beside.x, this->ty + beside.y));
	}

	double HeadFall(double side) const
	{
		double fall = 0.0;
		for (int row = 0; row <= RAMP_BANK_ROWS; row++) {
			WorldPoint top = this->Crest(ShareOfRow(row), side);
			fall = std::max(fall, top.level - this->land.LevelAt(top.x, top.y));
		}
		return fall;
	}

	bool BesideParallelRamp(int sx, int sy) const
	{
		if (!OnMap(sx, sy)) return false;
		RampTexel beside = _world_tiles.RampAt(TileXY(sx, sy));
		return beside.Present() && beside.onto == this->ramp.onto;
	}

	Footing BesideFooting(double side) const
	{
		TileStep step = this->SideStep(side);
		TileIndex beside = TileXY(this->tx + step.x, this->ty + step.y);
		return RampFooting(beside, this->onto, _world_tiles.RampAt(beside).deck);
	}

	double ReachBefore(const WorldPoint &top, const MapVector &outward, double reach) const
	{
		auto stopped = [&](double share) {
			double x = top.x + outward.x * reach * share;
			double y = top.y + outward.y * reach * share;
			return WaterFormOf(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y))) != WaterForm::Dry || this->land.LevelAt(x, y) >= top.level;
		};
		std::optional<Crossing> cut = FirstHolding(stopped);
		return cut.has_value() ? reach * cut->before : reach;
	}

	const Landform &land;
	int tx;
	int ty;
	RampTexel ramp;
	DiagDirection onto;
	Footing footing;
	Stretch run;
	MapVector facing;
};

class RampBank {
public:
	RampBank(TerrainMesh &mesh, const Landform &land, int tx, int ty, const RampTexel &ramp) : mesh(mesh), shape(land, tx, ty, ramp)
	{
	}

	void Lay() const
	{
		this->LayCrestBand();
		for (double side : {-1.0, 1.0}) this->LayFlank(side);
		std::array<WorldPoint, 2> abutment = {this->shape.Crest(1.0, -1.0), this->shape.Crest(1.0, 1.0)};
		AddMasonry(this->mesh, abutment, GroundedLine(this->shape.Land(), abutment), this->shape.Facing());
	}

private:
	void LayCrestBand() const
	{
		std::vector<StripRow> rows;
		for (int row = 0; row <= RAMP_BANK_ROWS; row++) rows.push_back({this->shape.CrestVertex(RampShape::ShareOfRow(row), -1.0), this->shape.CrestVertex(RampShape::ShareOfRow(row), 1.0)});
		AddStrip(this->mesh, rows);
	}

	/* Earth from the crest out to the tile's side, closed by stone under the deck's end. */
	std::vector<StripRow> LayBand(RampShape::Flank kind, double side) const
	{
		std::vector<StripRow> rows;
		for (int row = 0; row <= RAMP_BANK_ROWS; row++) {
			double share = RampShape::ShareOfRow(row);
			StripVertex crest = this->shape.CrestVertex(share, side);
			rows.push_back({crest, StripVertex{this->shape.BandEdge(kind, share, side), crest.normal, FILL_MARK}});
		}
		AddStrip(this->mesh, rows);
		std::array<WorldPoint, 2> end = {rows.back()[0].at, rows.back()[1].at};
		AddMasonry(this->mesh, end, GroundedLine(this->shape.Land(), end), this->shape.Facing());
		return rows;
	}

	void LayShelf(const RampShape::FlankPlan &plan, double side) const
	{
		std::vector<StripRow> rows = this->LayBand(plan.kind, side);
		std::vector<WorldPoint> line;
		std::vector<WorldPoint> feet;
		for (int row = RAMP_BANK_ROWS; row >= 0; row--) {
			line.push_back(rows[row][1].at);
			feet.push_back(this->shape.ShelfFoot(*plan.step, RampShape::ShareOfRow(row), side));
		}
		MapVector right = this->shape.SideOutward(side);
		AddMasonry(this->mesh, line, feet, right);
		AddCopingWhereTall(this->mesh, line, feet, right, COPING_DEPTH);
	}

	/* Where the other head stands lower, this one's earth is held at the border by stone down to the other's, closed at the outer end. */
	void LayMerged(double side) const
	{
		std::vector<StripRow> rows = this->LayBand(RampShape::Flank::Merged, side);
		std::vector<WorldPoint> feet;
		for (int row = 0; row <= RAMP_BANK_ROWS; row++) feet.push_back(this->shape.MergedFoot(RampShape::ShareOfRow(row), side));
		auto stands = [&](int row) { return rows[row][1].at.level - feet[row].level > LEAST_STEP; };
		Vec3 right = Upright(this->shape.SideOutward(side));
		for (int row = RAMP_BANK_ROWS; row > 0; row--) {
			const WorldPoint &near = rows[row][1].at, &far = rows[row - 1][1].at;
			if (stands(row) && stands(row - 1)) AddStoneQuad(this->mesh, {near, far, feet[row - 1], feet[row]}, right);
			else if (stands(row)) AddStoneTriangle(this->mesh, {near, far, feet[row]}, right);
			else if (stands(row - 1)) AddStoneTriangle(this->mesh, {near, far, feet[row - 1]}, right);
		}
		WorldPoint toe = this->shape.MergedToe(side);
		if (rows[0][1].at.level - toe.level > LEAST_STEP) AddStoneTriangle(this->mesh, {rows[0][1].at, rows[0][0].at, toe}, Upright(this->shape.Facing() * -1.0));
	}

	void LayFlank(double side) const
	{
		RampShape::FlankPlan plan = this->shape.PlanOf(side);
		MapVector right = this->shape.SideOutward(side);
		if (plan.kind == RampShape::Flank::Shelf) {
			this->LayShelf(plan, side);
			return;
		}
		if (plan.kind == RampShape::Flank::Merged) {
			this->LayMerged(side);
			return;
		}
		if (plan.kind == RampShape::Flank::Wall) {
			std::vector<WorldPoint> line;
			for (int row = RAMP_BANK_ROWS; row >= 0; row--) line.push_back(this->shape.Crest(RampShape::ShareOfRow(row), side));
			AddWingWall(this->mesh, this->shape.Land(), line, right, this->shape.Facing() * -1.0, true, DECK_WING_THICKNESS);
			return;
		}
		std::vector<BankEdge> edges;
		for (int row = 0; row <= RAMP_BANK_ROWS; row++) edges.push_back(this->shape.EdgeAt(row, side, plan.run));
		AddBankColumns(this->mesh, edges, FILL_MARK);
		if (plan.kind == RampShape::Flank::Bank) AddWingWall(this->mesh, this->shape.Land(), edges.back().points, this->shape.Facing(), right, false, WING_THICKNESS);
	}

	TerrainMesh &mesh;
	RampShape shape;
};

static bool CoversTile(int tx, int ty, int x, int y)
{
	for (DiagDirection side = DIAGDIR_BEGIN; side < DIAGDIR_END; side++) {
		std::optional<BankedStep> site = BankOn(x, y, side);
		if (!site.has_value()) continue;
		int reach = static_cast<int>(std::ceil(std::max(site->runs[0], site->runs[1])));
		const StepBank &stepped = site->stepped;
		for (int beyond = 0; beyond < reach; beyond++) {
			if (tx == stepped.low_x + beyond * (stepped.low_x - x) && ty == stepped.low_y + beyond * (stepped.low_y - y)) return true;
		}
	}
	return false;
}

static bool BesideWay(int x, int y)
{
	if (_world_tiles.RampAt(TileXY(x, y)).Present()) return true;
	for (Track track : SetTrackBitIterator(static_cast<TrackBits>(_world_tiles.NetworkAt(TileXY(x, y)).track))) {
		if (std::optional<WayCourse> course = WayCourse::OfTrack(x, y, track); course.has_value() && course->Raised()) return true;
	}
	std::optional<WayCourse> road = WayCourse::OfRoad(x, y);
	return road.has_value() && road->Raised();
}

template <class Meets>
static bool AnyAround(int tx, int ty, int reach, const Meets &meets)
{
	for (int y = ty - reach; y <= ty + reach; y++) {
		for (int x = tx - reach; x <= tx + reach; x++) {
			if (OnMap(x, y) && meets(x, y)) return true;
		}
	}
	return false;
}

bool IsEarthBanked(int tx, int ty)
{
	return AnyAround(tx, ty, NEIGHBOUR_WAY_REACH, BesideWay) || AnyAround(tx, ty, BANK_SOURCE_REACH, [&](int x, int y) { return CoversTile(tx, ty, x, y); });
}

static void AddRampBank(TerrainMesh &mesh, const Landform &land, int tx, int ty)
{
	RampTexel ramp = _world_tiles.RampAt(TileXY(tx, ty));
	if (ramp.Present()) RampBank(mesh, land, tx, ty, ramp).Lay();
}

static double Unfolded(const Bend &line, double share, double lateral)
{
	double curvature = line.Curvature(share);
	return lateral * curvature > INNER_OFFSET_LIMIT ? INNER_OFFSET_LIMIT / curvature : lateral;
}

static double CrestLevel(const WayCourse &course, double share)
{
	return course.Level(share) + WAY_FOOT / LevelRise();
}

static double LevelOn(const WayCourse &course, const MapVector &at)
{
	return course.Level(course.ShareAt(at));
}

static int TileOf(double coordinate, int own)
{
	int tile = static_cast<int>(std::floor(coordinate));
	return coordinate == tile && own < tile ? tile - 1 : tile;
}

static bool OnTileEdge(double coordinate)
{
	return std::abs(coordinate - std::round(coordinate)) < EDGE_SNAP;
}

static int AcrossEdge(double coordinate, int near)
{
	return near == static_cast<int>(std::round(coordinate)) ? near - 1 : near + 1;
}

struct WayId {
	int tx;
	int ty;
	std::optional<Track> track;
};

struct WayReach {
	double distance;
	double half;
	double level;
};

/* The nearest crest edge of a raised track beside a point, the middle of that track, and its course. */
struct TrackBeside {
	WorldPoint edge;
	MapVector middle;
	WayId id;
	WayCourse course;
};

class NeighbouringWays {
public:
	NeighbouringWays(const Landform &land, const WayId &own) : land(land)
	{
		for (int ty = own.ty - NEIGHBOUR_WAY_REACH; ty <= own.ty + NEIGHBOUR_WAY_REACH; ty++) {
			for (int tx = own.tx - NEIGHBOUR_WAY_REACH; tx <= own.tx + NEIGHBOUR_WAY_REACH; tx++) {
				if (IsMapTile(tx, ty)) this->Collect(tx, ty, own);
			}
		}
	}

	std::optional<WayReach> Nearest(const MapVector &at) const
	{
		std::optional<WayReach> nearest;
		for (const DrawnWay &way : this->ways) {
			MapVector centre = way.line.Centre(way.line.ShareOf(at));
			double distance = Length(at - centre);
			if (nearest.has_value() && nearest->distance <= distance) continue;
			nearest = WayReach{distance, way.half, this->FaceLevel(way, at)};
		}
		return nearest;
	}

	/* The nearest raised track running alongside straight out from a point, square to the heading out. */
	std::optional<TrackBeside> RaisedTrackAlongside(const MapVector &at, const MapVector &outward, double crest_half) const
	{
		std::optional<TrackBeside> nearest;
		for (const DrawnWay &way : this->ways) {
			if (!way.id.track.has_value()) continue;
			double share = way.line.ShareOf(at);
			MapVector middle = way.line.Centre(share);
			MapVector toward = middle - at;
			MapVector along = Unit(way.line.Along(share));
			bool square = Dot(Unit(toward), outward) >= ALONGSIDE_LEAST_DOT && std::abs(Dot(along, outward)) <= ALONGSIDE_SKEW && std::abs(Dot(toward, along)) <= ALONGSIDE_SLIP;
			if (!square || Length(toward) <= crest_half) continue;
			MapVector edge = middle - Unit(toward) * crest_half;
			if (nearest.has_value() && Length(Flat(nearest->edge) - at) <= Length(edge - at)) continue;
			std::optional<WayCourse> course = WayCourse::OfTrack(way.id.tx, way.id.ty, *way.id.track);
			if (!course.has_value() || !course->Raised()) continue;
			nearest = TrackBeside{{edge.x, edge.y, CrestLevel(*course, course->ShareAt(edge))}, middle, way.id, *course};
		}
		return nearest;
	}

private:
	struct DrawnWay {
		Bend line;
		std::optional<WayCourse> course;
		double half;
		WayId id = {};
	};

	void Collect(int tx, int ty, const WayId &own)
	{
		bool own_tile = tx == own.tx && ty == own.ty;
		for (Track track : SetTrackBitIterator(static_cast<TrackBits>(_world_tiles.NetworkAt(TileXY(tx, ty)).track))) {
			if (own_tile && own.track == track) continue;
			const DrawnTrack &drawn = DrawnTrack::Of(tx, ty, track);
			this->ways.push_back({drawn.line, drawn.course, BALLAST_HALF, {tx, ty, track}});
		}
		if (own_tile && !own.track.has_value()) return;
		if (std::optional<WayCourse> course = WayCourse::OfRoad(tx, ty); course.has_value()) this->ways.push_back({course->Line(), course, ROAD_HALF});
	}

	double FaceLevel(const DrawnWay &way, const MapVector &at) const
	{
		if (!way.course.has_value()) return this->land.LevelAt(at.x, at.y);
		return CrestLevel(*way.course, way.course->ShareAt(at));
	}

	const Landform &land;
	std::vector<DrawnWay> ways;
};

struct RaisedWay {
	WayCourse course;
	WayId id;
	double half;
	bool middle;
};

static std::vector<RaisedWay> RaisedWaysOf(int tx, int ty)
{
	std::vector<RaisedWay> ways;
	if (!OnMap(tx, ty)) return ways;
	for (Track track : SetTrackBitIterator(static_cast<TrackBits>(_world_tiles.NetworkAt(TileXY(tx, ty)).track))) {
		if (std::optional<WayCourse> course = WayCourse::OfTrack(tx, ty, track); course.has_value() && course->Raised()) ways.push_back({*course, {tx, ty, track}, BALLAST_HALF + WAY_CESS, true});
	}
	if (std::optional<WayCourse> course = WayCourse::OfRoad(tx, ty); course.has_value() && course->Raised()) ways.push_back({*course, {tx, ty, std::nullopt}, ROAD_HALF, false});
	return ways;
}

static Vec3 SlopeNormal(const WorldPoint &crest, const WorldPoint &toe)
{
	MapVector across = Flat(toe) - Flat(crest);
	double run = Length(across);
	if (run == 0.0) return UP;
	double fall = (crest.level - toe.level) * LevelRise();
	return Normalised({across.x / run * fall, across.y / run * fall, run});
}

using CrestPair = std::array<WorldPoint, 2>;

static WorldPoint CrestAt(const WayCourse &course, double half, double share, double sign)
{
	const Bend &line = course.Line();
	MapVector at = line.At(share, Unfolded(line, share, sign * half));
	return {at.x, at.y, CrestLevel(course, share)};
}

static CrestPair CrestsOf(const WayCourse &course, double half, double share)
{
	return {CrestAt(course, half, share, -1.0), CrestAt(course, half, share, 1.0)};
}

static bool Adjoins(const WorldPoint &first, const WorldPoint &second)
{
	return Length(Flat(first) - Flat(second)) <= JOINT_REACH && std::abs(first.level - second.level) <= SAME_POINT;
}

static bool Joins(const CrestPair &first, const CrestPair &second)
{
	return (Adjoins(first[0], second[0]) && Adjoins(first[1], second[1])) || (Adjoins(first[0], second[1]) && Adjoins(first[1], second[0]));
}

static int TrackKey(const WayId &id)
{
	return id.track.has_value() ? static_cast<int>(*id.track) : -1;
}

static bool Precedes(const WayId &first, const WayId &second)
{
	return std::make_tuple(first.tx, first.ty, TrackKey(first)) < std::make_tuple(second.tx, second.ty, TrackKey(second));
}

class FormationProfile {
public:
	FormationProfile(const Landform &land, const WayCourse &course, const WayId &id, double half) :
		land(land), course(course), line(course.Line()), id(id), own(id.tx, id.ty), half(half), ways(land, id)
	{
	}

	FormationRow RowAt(double share) const
	{
		return {this->SideAt(share, -1.0), this->SideAt(share, 1.0)};
	}

private:
	struct Toe {
		WorldPoint at;
		Vec3 normal;
	};

	enum class Seat {
		Wall,
		Portal,
		Way,
		Ground,
	};

	struct Stop {
		Seat seat;
		Crossing crossing;
	};

	FormationSide SideAt(double share, double sign) const
	{
		WorldPoint crest = CrestAt(this->course, this->half, share, sign);
		MapVector inside = this->WithinOwn(crest.x, crest.y);
		double depth = std::max(crest.level - this->own.Level(inside.x, inside.y) + BANK_FOOT_DIP, 0.0);
		double spread = this->half + depth * LevelRise() * FORMATION_RUN;
		MapVector out = this->line.At(share, Unfolded(this->line, share, sign * spread));
		WorldPoint foot = {out.x, out.y, crest.level - depth};
		Toe toe = this->ToeOf(crest, foot);
		return {crest, Normalised(Lerp(SlopeNormal(crest, toe.at), UP, CREST_ROUNDING)), toe.at, toe.normal};
	}

	Toe ToeOf(const WorldPoint &crest, const WorldPoint &foot) const
	{
		Toe natural = this->NaturalToe(crest, foot);
		MapVector outward = Unit(Flat(foot) - Flat(crest));
		std::optional<Toe> filled = this->FilledToeAt(Flat(crest), outward);
		double share = filled.has_value() ? this->FillShare(Flat(crest), outward) : 0.0;
		if (share == 0.0) return natural;
		WorldPoint toe = Between(natural.at, filled->at, share);
		return {toe, SlopeNormal(crest, toe)};
	}

	Toe NaturalToe(const WorldPoint &crest, const WorldPoint &foot) const
	{
		auto at = [&](double share) { return Between(crest, foot, share); };
		std::optional<Stop> earliest = this->FirstStop(crest, foot, true);
		if (earliest.has_value()) return this->ToeAt(earliest->seat, crest, at(earliest->crossing.after));
		WorldPoint seated = Grounded(this->land, foot);
		return {seated, ClimbNormal(this->land.Climb(seated.x, seated.y))};
	}

	/* Where earth running out from a crest toward a point first stops: at a wall, a portal's cutting or another way, and where it may settle, on the ground. */
	std::optional<Stop> FirstStop(const WorldPoint &crest, const WorldPoint &end, bool settles) const
	{
		auto at = [&](double share) { return Between(crest, end, share); };
		std::optional<Stop> earliest;
		auto consider = [&](Seat seat, const auto &holds) {
			std::optional<Crossing> crossing = FirstHolding(holds);
			if (crossing.has_value() && (!earliest.has_value() || crossing->after < earliest->crossing.after)) earliest = Stop{seat, *crossing};
		};
		consider(Seat::Wall, [&](double share) { return this->Blocked(Flat(at(share))); });
		consider(Seat::Portal, [&](double share) { return this->PortalBeyondEdge(at(share), crest); });
		consider(Seat::Way, [&](double share) { return this->Crossed(at(share), crest); });
		if (settles) consider(Seat::Ground, [&](double share) { return this->Buried(at(share)); });
		return earliest;
	}

	Toe ToeAt(Seat seat, const WorldPoint &crest, const WorldPoint &where) const
	{
		switch (seat) {
			case Seat::Wall: return this->ToeAtWall(where);
			case Seat::Portal: return this->ToeAtPortal(where);
			case Seat::Way: return this->ToeAtWay(crest, where);
			case Seat::Ground: return this->ToeOnGround(crest, where);
		}
		NOT_REACHED();
	}

	bool Blocked(const MapVector &at) const
	{
		int tx = TileOf(at.x, this->id.tx);
		int ty = TileOf(at.y, this->id.ty);
		if ((tx == this->id.tx && ty == this->id.ty) || !OnMap(tx, ty)) return false;
		if (StandsBlocked(tx, ty)) return true;
		return GroundworkAt(tx, ty) == Groundwork::Way && TileGround(tx, ty).Level(at.x, at.y) < this->own.Level(at.x, at.y);
	}

	/* A row lying along the edge of its own tile also borders, beside that tile, a tunnel portal across the edge, whose cutting the earth may not spill into. */
	bool PortalBeyondEdge(const WorldPoint &at, const WorldPoint &crest) const
	{
		int tx = TileOf(at.x, this->id.tx);
		int ty = TileOf(at.y, this->id.ty);
		if (!OnMap(tx, ty) || TileGround(tx, ty).Level(at.x, at.y) >= crest.level) return false;
		return (ty != this->id.ty && OnTileEdge(at.x) && IsPortalTile(AcrossEdge(at.x, tx), ty)) || (tx != this->id.tx && OnTileEdge(at.y) && IsPortalTile(tx, AcrossEdge(at.y, ty)));
	}

	/* Where this side's earth meets a raised track close alongside, no higher than this side, when it fills the gap to it from a point of its crest where no wall parts the two. */
	std::optional<Toe> FilledToeAt(const MapVector &at, const MapVector &outward) const
	{
		if (!this->id.track.has_value()) return std::nullopt;
		std::optional<TrackBeside> beside = this->ways.RaisedTrackAlongside(at, outward, this->half);
		if (!beside.has_value() || Length(Flat(beside->edge) - at) > WIDEST_FILLED_GAP) return std::nullopt;
		WorldPoint crest = {at.x, at.y, CrestLevel(this->course, this->course.ShareAt(at))};
		bool higher = crest.level > beside->edge.level + SAME_CREST_LEVELS || (crest.level >= beside->edge.level - SAME_CREST_LEVELS && Precedes(this->id, beside->id));
		if (!higher || this->WalledFrom(at, Flat(beside->edge))) return std::nullopt;
		return this->ToeOnLowerTrack(crest, outward, *beside);
	}

	/* How fully this side's earth fills the gap to the lower track beside a point of its crest: wholly along a stretch beside it, easing off over its last rows so the earth meets the rows beyond without a step, and over more rows before a wall. */
	double FillShare(const MapVector &crest, const MapVector &outward) const
	{
		const WayCourse &lower = this->ways.RaisedTrackAlongside(crest, outward, this->half)->course;
		int rows = FILL_EASING_ROWS;
		for (double way : {-1.0, 1.0}) {
			for (int row = 1; row < 2 * FILL_EASING_ROWS - 1; row++) {
				MapVector at = RowsAlong(crest, outward, way * row);
				if (this->FilledToeAt(at, outward).has_value()) continue;
				std::optional<TrackBeside> beside = this->ways.RaisedTrackAlongside(at, outward, this->half);
				if (beside.has_value()) rows = std::min(rows, this->WalledFrom(at, Flat(beside->edge)) ? row - (FILL_EASING_ROWS - 1) : row);
				else if (RunsBeside(lower, at)) rows = std::min(rows, row);
				break;
			}
		}
		double eased = static_cast<double>(std::max(rows, 0)) / FILL_EASING_ROWS;
		return eased * eased;
	}

	/* Whether a point lies beside a way's line rather than beyond either end of it. */
	static bool RunsBeside(const WayCourse &course, const MapVector &at)
	{
		const Bend &line = course.Line();
		double share = line.ShareOf(at);
		return std::abs(Dot(line.Centre(share) - at, Unit(line.Along(share)))) <= ALONGSIDE_SLIP;
	}

	static MapVector RowsAlong(const MapVector &at, const MapVector &outward, double rows)
	{
		return at + RightOf(outward) * (rows * ROW_SPACING);
	}

	/* Whether a wall parts the ground under a crest from the ground under the track beside it. */
	bool WalledFrom(const MapVector &near, const MapVector &far) const
	{
		TileGround under(TileOf(near.x, this->id.tx), TileOf(near.y, this->id.ty));
		int bx = static_cast<int>(std::floor(far.x));
		int by = static_cast<int>(std::floor(far.y));
		return this->Blocked(far) || !OnMap(bx, by) || StandsBlocked(bx, by) || TileGround(bx, by).Level(far.x, far.y) < under.Level(far.x, far.y);
	}

	/* Earth over the gap to a lower track runs on past the ground to where it crosses toward that track, reaching at least its crest and meeting it at its level; none if a wall or a portal's cutting stops it first. */
	std::optional<Toe> ToeOnLowerTrack(const WorldPoint &crest, const MapVector &outward, const TrackBeside &lower) const
	{
		MapVector beyond = Flat(crest) + outward * Length(lower.middle - Flat(crest));
		WorldPoint end = {beyond.x, beyond.y, crest.level};
		std::optional<Stop> stop = this->FirstStop(crest, end, false);
		if (!stop.has_value() || stop->seat != Seat::Way) return std::nullopt;
		WorldPoint crossed = Between(crest, end, stop->crossing.after);
		double run = std::max(Length(Flat(crossed) - Flat(crest)), Dot(Flat(lower.edge) - Flat(crest), outward));
		MapVector out = Flat(crest) + outward * run;
		double face = CrestLevel(lower.course, lower.course.ShareAt(out));
		WorldPoint toe = {out.x, out.y, std::min(crest.level, std::max(this->land.LevelAt(out.x, out.y), face))};
		return Toe{toe, SlopeNormal(crest, toe)};
	}

	bool Crossed(const WorldPoint &at, const WorldPoint &crest) const
	{
		std::optional<WayReach> other = this->ways.Nearest(Flat(at));
		return other.has_value() && other->distance + WAY_MIDLINE_MARGIN < Length(Flat(at) - Flat(crest));
	}

	bool Buried(const WorldPoint &at) const
	{
		double surface = this->land.LevelAt(at.x, at.y);
		if (std::optional<WayReach> other = this->ways.Nearest(Flat(at)); other.has_value() && other->distance <= other->half) surface = std::max(surface, other->level);
		return at.level <= surface;
	}

	MapVector WithinOwn(double x, double y) const
	{
		return {std::clamp(x, static_cast<double>(this->id.tx), this->id.tx + 1.0), std::clamp(y, static_cast<double>(this->id.ty), this->id.ty + 1.0)};
	}

	Toe ToeAtWall(const WorldPoint &at) const
	{
		auto [x, y] = this->WithinOwn(at.x, at.y);
		WorldPoint seated = {x, y, this->own.Level(x, y) - BANK_FOOT_DIP};
		return {seated, ClimbNormal(ClimbOf([this](double px, double py) { return this->own.Level(px, py); }, x, y))};
	}

	/* Earth stopped beside a portal meets the open ground at the corner of its own tile. */
	Toe ToeAtPortal(const WorldPoint &at) const
	{
		TileGround beside(TileOf(at.x, this->id.tx), TileOf(at.y, this->id.ty));
		auto [x, y] = this->WithinOwn(at.x, at.y);
		WorldPoint seated = {x, y, beside.Level(x, y) - BANK_FOOT_DIP};
		return {seated, UP};
	}

	Toe ToeAtWay(const WorldPoint &crest, const WorldPoint &at) const
	{
		double face = this->ways.Nearest(Flat(at)).value_or(WayReach{0.0, 0.0, crest.level}).level;
		WorldPoint toe = {at.x, at.y, std::min(crest.level, std::max(this->land.LevelAt(at.x, at.y), face))};
		return {toe, SlopeNormal(crest, toe)};
	}

	Toe ToeOnGround(const WorldPoint &crest, const WorldPoint &at) const
	{
		WorldPoint grounded = Grounded(this->land, at);
		if (grounded.level - at.level > SEAT_STEP_LEAST) return {at, SlopeNormal(crest, at)};
		return {grounded, ClimbNormal(this->land.Climb(at.x, at.y))};
	}

	const Landform &land;
	const WayCourse &course;
	const Bend &line;
	WayId id;
	TileGround own;
	double half;
	NeighbouringWays ways;
};

class Formation {
public:
	Formation(TerrainMesh &mesh, const Landform &land, const WayCourse &course, const WayId &id, double half) :
		mesh(mesh), land(land), course(course), id(id), half(half), profile(land, course, id, half),
		rows(std::max(2, static_cast<int>(std::ceil(course.Length() * FORMATION_ROWS_PER_TILE))))
	{
	}

	void Lay(bool middle)
	{
		std::vector<FormationRow> table;
		for (int row = 0; row <= this->rows; row++) table.push_back(this->profile.RowAt(static_cast<double>(row) / this->rows));
		Joint start = this->JointAt(0.0, this->course.Before() * -1.0);
		Joint finish = this->JointAt(1.0, this->course.After());
		table.front() = start.row;
		table.back() = finish.row;
		for (size_t side = 0; side < table.front().size(); side++) this->Unreversed(table, side);
		for (size_t side = 0; side < table.front().size(); side++) this->LaySide(table, side);
		if (middle) this->LayMiddle(table);
		if (!start.continued) this->Close(start.row, this->course.Before() * -1.0);
		if (!finish.continued) this->Close(finish.row, this->course.After());
	}

private:
	void KeepAdvancing(std::vector<FormationRow> &table, size_t side, WorldPoint FormationSide::*member, size_t row, size_t neighbour, double sign) const
	{
		WorldPoint &at = table[row][side].*member;
		MapVector heading = this->course.Line().Along(static_cast<double>(row) / this->rows);
		double progress = Dot(Flat(at) - Flat(table[neighbour][side].*member), heading);
		if (sign * progress >= 0.0) return;
		at.x -= heading.x * progress;
		at.y -= heading.y * progress;
	}

	void Unreversed(std::vector<FormationRow> &table, size_t side) const
	{
		for (WorldPoint FormationSide::*member : {&FormationSide::crest, &FormationSide::toe}) {
			for (size_t row = 1; row + 1 < table.size(); row++) this->KeepAdvancing(table, side, member, row, row - 1, 1.0);
			for (size_t row = table.size() - 2; row > 0; row--) this->KeepAdvancing(table, side, member, row, row + 1, -1.0);
		}
	}

	static double EarthWidth(const FormationSide &side)
	{
		return Length(Flat(side.toe) - Flat(side.crest));
	}

	static double WallFallOf(const FormationSide &side)
	{
		return FallBeyondEarth(side.crest.level - side.toe.level, EarthWidth(side));
	}

	static size_t AbutmentRow(size_t nth, size_t count, bool at_front)
	{
		return at_front ? nth : count - 1 - nth;
	}

	template <typename Holds, typename Visit>
	static void ForEachRun(size_t count, Holds holds, Visit visit)
	{
		size_t first = 0;
		while (first < count) {
			if (!holds(first)) {
				first++;
				continue;
			}
			size_t last = first;
			while (last < count && holds(last)) last++;
			visit(first, last);
			first = last;
		}
	}

	template <typename Holds>
	static size_t LeadingRun(size_t count, bool at_front, Holds holds)
	{
		size_t length = 0;
		while (length < count && holds(AbutmentRow(length, count, at_front))) length++;
		return length;
	}

	static std::vector<double> WallFalls(std::span<const FormationSide> sides)
	{
		std::vector<double> walls;
		for (const FormationSide &side : sides) {
			double fall = WallFallOf(side);
			walls.push_back(fall >= LEAST_WALL_FALL ? fall : 0.0);
		}
		return walls;
	}

	static bool StandsNaturally(std::span<const double> walls, std::span<const double> abutment, size_t row)
	{
		return walls[row] > 0.0 && abutment[row] == 0.0;
	}

	static void DropShortWalls(std::vector<double> &walls, std::span<const double> abutment)
	{
		auto stands = [&](size_t row) { return StandsNaturally(walls, abutment, row); };
		ForEachRun(walls.size(), stands, [&](size_t first, size_t last) {
			if (last - first < LEAST_WALL_ROWS) std::fill(walls.begin() + first, walls.begin() + last, 0.0);
		});
	}

	static WorldPoint WallTop(const FormationSide &side, double wall)
	{
		return {side.toe.x, side.toe.y, std::min(side.crest.level, side.toe.level + wall)};
	}

	static std::vector<bool> StandsAtFoot(std::span<const double> walls, std::span<const double> abutment)
	{
		std::vector<bool> at_foot(walls.size(), false);
		auto stands = [&](size_t row) { return StandsNaturally(walls, abutment, row); };
		ForEachRun(walls.size(), stands, [&](size_t first, size_t last) {
			if (first == 0 || last == walls.size() || abutment[first - 1] != 0.0 || abutment[last] != 0.0) return;
			std::fill(at_foot.begin() + first, at_foot.begin() + last, true);
		});
		return at_foot;
	}

	static StripRow EarthRowOf(const FormationSide &side, double wall, bool at_foot)
	{
		if (at_foot) {
			WorldPoint top = WallTop(side, wall);
			Vec3 crest_normal = Normalised(Lerp(SlopeNormal(side.crest, top), UP, CREST_ROUNDING));
			return {{{side.crest, crest_normal, FILL_MARK}, {top, side.toe_normal, FILL_MARK}}};
		}
		WorldPoint shoulder = {side.crest.x, side.crest.y, side.crest.level - wall};
		Vec3 shoulder_normal = Normalised(Lerp(SlopeNormal(shoulder, side.toe), UP, CREST_ROUNDING));
		return {{{shoulder, shoulder_normal, FILL_MARK}, {side.toe, side.toe_normal, FILL_MARK}}};
	}

	WorldPoint TowardCrest(const FormationSide &side, double share) const
	{
		WorldPoint moved = Between(side.toe, {side.crest.x, side.crest.y, side.toe.level}, share);
		moved.level += this->land.LevelAt(moved.x, moved.y) - this->land.LevelAt(side.toe.x, side.toe.y);
		return moved;
	}

	static std::vector<double> ReachFromJoint(std::span<const FormationSide> sides, bool at_front)
	{
		std::vector<double> reach = {0.0};
		for (size_t nth = 1; nth < sides.size(); nth++) {
			const WorldPoint &from = sides[AbutmentRow(nth - 1, sides.size(), at_front)].crest;
			const WorldPoint &to = sides[AbutmentRow(nth, sides.size(), at_front)].crest;
			reach.push_back(reach.back() + Length(Flat(to) - Flat(from)));
		}
		return reach;
	}

	static double CrowdedWidth(std::span<const FormationSide> sides, std::span<const double> reach, bool at_front)
	{
		double crowded = 0.0;
		for (size_t nth = 0; nth < sides.size(); nth++) {
			double width = EarthWidth(sides[AbutmentRow(nth, sides.size(), at_front)]);
			if (width > reach[nth]) crowded = std::max(crowded, width);
		}
		return crowded;
	}

	static void TaperFromJoint(std::span<const FormationSide> sides, std::span<const double> walls, bool at_front, std::vector<double> &shares)
	{
		size_t joint_row = AbutmentRow(0, sides.size(), at_front);
		if (!sides[joint_row].walled || walls[joint_row] == 0.0) return;
		size_t standing = LeadingRun(sides.size(), at_front, [&](size_t row) { return walls[row] > 0.0; });
		if (standing >= LEAST_WALL_ROWS) return;
		std::vector<double> reach = ReachFromJoint(sides, at_front);
		double beyond = standing < sides.size() ? reach[standing] : reach.back();
		double taper = std::min(std::max(CrowdedWidth(sides, reach, at_front), beyond), reach.back());
		for (size_t nth = 0; nth < sides.size() && reach[nth] < taper; nth++) {
			size_t row = AbutmentRow(nth, sides.size(), at_front);
			shares[row] = std::max(shares[row], BankFall(1.0 - reach[nth] / taper));
		}
	}

	static std::vector<double> AbutmentShares(std::span<const FormationSide> sides, std::span<const double> walls)
	{
		std::vector<double> shares(sides.size(), 0.0);
		for (bool at_front : {true, false}) TaperFromJoint(sides, walls, at_front, shares);
		return shares;
	}

	void WallAbutment(std::span<FormationSide> sides, std::vector<double> &walls, std::span<const double> abutment) const
	{
		for (size_t row = 0; row < sides.size(); row++) {
			if (abutment[row] == 0.0) continue;
			sides[row].toe = this->TowardCrest(sides[row], abutment[row]);
			walls[row] = std::max(walls[row], (sides[row].crest.level - sides[row].toe.level) * abutment[row]);
		}
	}

	static void CarryAbutmentSlope(std::span<StripRow> earth, std::span<const double> abutment, bool at_front)
	{
		size_t length = LeadingRun(earth.size(), at_front, [&](size_t row) { return abutment[row] > 0.0; });
		if (length == 0 || length == earth.size()) return;
		const StripRow &sloping = earth[AbutmentRow(length, earth.size(), at_front)];
		for (size_t nth = 0; nth < length; nth++) {
			StripRow &carried = earth[AbutmentRow(nth, earth.size(), at_front)];
			for (size_t point = 0; point < carried.size(); point++) carried[point].normal = sloping[point].normal;
		}
	}

	static MapVector SideFacing(std::span<const FormationRow> table, size_t row, size_t side)
	{
		return Unit(Flat(table[row][side].crest) - Flat(table[row][1 - side].crest));
	}

	template <typename Holds, typename TopOf>
	void CopeRuns(std::span<const FormationRow> table, size_t side, Holds holds, TopOf top_of) const
	{
		ForEachRun(table.size(), holds, [&](size_t first, size_t last) {
			if (last - first < 2) return;
			std::vector<WorldPoint> line;
			std::vector<MapVector> facings;
			for (size_t at = first; at < last; at++) {
				line.push_back(top_of(at));
				facings.push_back(SideFacing(table, at, side));
			}
			AddCopingAlong(this->mesh, line, [&](size_t nth) { return facings[nth]; }, COPING_DEPTH);
		});
	}

	void LayWallFaces(std::span<const FormationRow> table, std::span<const FormationSide> sides, std::span<const StripRow> earth, const std::vector<bool> &at_foot, size_t side) const
	{
		for (size_t row = 0; row + 1 < table.size(); row++) {
			bool by_foot = at_foot[row] || at_foot[row + 1];
			auto top_of = [&](size_t at) { return by_foot ? earth[at][1].at : table[at][side].crest; };
			auto foot_of = [&](size_t at) { return by_foot ? sides[at].toe : earth[at][0].at; };
			this->AddWallFace({top_of(row), top_of(row + 1)}, {foot_of(row), foot_of(row + 1)}, SideFacing(table, row, side));
		}
	}

	/* A head's stone shelf, met by earth whose next row already stands on the ground, goes on down to that row's toe. */
	void CarryShelfWall(std::span<const FormationRow> table, std::span<const FormationSide> sides, size_t side, bool at_front) const
	{
		size_t joint = AbutmentRow(0, sides.size(), at_front);
		const WorldPoint &top = sides[joint].toe;
		WorldPoint foot = {top.x, top.y, this->OwnGroundUnder(top)};
		if (!sides[joint].shelved || top.level - foot.level < SHALLOWEST_FACE) return;
		const WorldPoint &next = sides[AbutmentRow(1, sides.size(), at_front)].toe;
		std::array<WorldPoint, 2> line = at_front ? std::array{top, next} : std::array{next, top};
		AddStoneTriangle(this->mesh, {line[0], line[1], foot}, Upright(SideFacing(table, joint, side)));
	}

	void LaySide(std::span<const FormationRow> table, size_t side) const
	{
		std::vector<FormationSide> sides;
		for (const FormationRow &row : table) sides.push_back(row[side]);
		std::vector<double> walls = WallFalls(sides);
		std::vector<double> abutment = AbutmentShares(sides, walls);
		this->WallAbutment(sides, walls, abutment);
		DropShortWalls(walls, abutment);
		std::vector<bool> at_foot = StandsAtFoot(walls, abutment);
		std::vector<StripRow> earth;
		for (size_t row = 0; row < table.size(); row++) earth.push_back(EarthRowOf(sides[row], walls[row], at_foot[row]));
		for (bool at_front : {true, false}) CarryAbutmentSlope(earth, abutment, at_front);
		const std::optional<BankEdge> &head = table.front()[side].section;
		const std::optional<BankEdge> &tail = table.back()[side].section;
		size_t first = head.has_value() ? 1 : 0;
		size_t end = tail.has_value() ? earth.size() - 1 : earth.size();
		AddStrip(this->mesh, std::span<const StripRow>(earth).subspan(first, end - first));
		if (head.has_value()) AddBankStrip(this->mesh, *head, Spanned(earth[first]), FILL_MARK);
		if (tail.has_value()) AddBankStrip(this->mesh, Spanned(earth[end - 1]), *tail, FILL_MARK);
		this->LayWallFaces(table, sides, earth, at_foot, side);
		for (bool at_front : {true, false}) this->CarryShelfWall(table, sides, side, at_front);
		this->CopeRuns(table, side, [&](size_t row) { return abutment[row] > 0.0; }, [&](size_t row) { return sides[row].crest; });
		this->CopeRuns(table, side, [&](size_t row) { return at_foot[row]; }, [&](size_t row) { return earth[row][1].at; });
	}

	void LayMiddle(std::span<const FormationRow> table) const
	{
		auto footing = [this](double x, double y) { return LevelOn(this->course, {x, y}); };
		std::vector<StripRow> rows;
		for (const FormationRow &row : table) rows.push_back({CrestVertexOn(footing, row[0].crest), CrestVertexOn(footing, row[1].crest)});
		AddStrip(this->mesh, rows);
	}

	double OwnGroundUnder(const WorldPoint &at) const
	{
		double x = std::clamp(at.x, this->id.tx + OWN_TILE_INSET, this->id.tx + 1.0 - OWN_TILE_INSET);
		double y = std::clamp(at.y, this->id.ty + OWN_TILE_INSET, this->id.ty + 1.0 - OWN_TILE_INSET);
		return this->land.LevelAt(x, y);
	}

	WorldPoint Seated(const WorldPoint &at) const
	{
		return {at.x, at.y, std::min(at.level, this->OwnGroundUnder(at) - BANK_FOOT_DIP)};
	}

	struct Joint {
		FormationRow row;
		bool continued;
	};

	static FormationRow Aligned(FormationRow row, const FormationRow &to)
	{
		if (Length(Flat(row[0].crest) - Flat(to[0].crest)) > Length(Flat(row[1].crest) - Flat(to[0].crest))) std::swap(row[0], row[1]);
		return row;
	}

	Joint JointAt(double end, const MapVector &facing) const
	{
		FormationRow own = this->profile.RowAt(end);
		CrestPair crests = {own[0].crest, own[1].crest};
		MapVector middle = Flat(Between(crests[0], crests[1], MIDWAY)) + facing * END_PROBE;
		Joint joint = {own, false};
		WayId owner = this->id;
		for (const RaisedWay &way : RaisedWaysOf(static_cast<int>(std::floor(middle.x)), static_cast<int>(std::floor(middle.y)))) {
			if (way.half != this->half) continue;
			for (double at : {0.0, 1.0}) {
				if (!Joins(crests, CrestsOf(way.course, way.half, at))) continue;
				joint.continued = true;
				if (!Precedes(way.id, owner)) continue;
				owner = way.id;
				joint.row = Aligned(FormationProfile(this->land, way.course, way.id, way.half).RowAt(at), own);
			}
		}
		if (joint.continued) return joint;
		std::optional<FormationRow> head = this->RampHeadRow(middle, facing);
		if (head.has_value()) joint = {Aligned(*head, own), true};
		return joint;
	}

	std::optional<FormationRow> RampHeadRow(const MapVector &middle, const MapVector &facing) const
	{
		int tx = static_cast<int>(std::floor(middle.x));
		int ty = static_cast<int>(std::floor(middle.y));
		if (!OnMap(tx, ty)) return std::nullopt;
		RampTexel ramp = _world_tiles.RampAt(TileXY(tx, ty));
		if (!ramp.Present() || Dot(Outward(static_cast<DiagDirection>(ramp.onto)), facing) < PARALLEL_LEAST) return std::nullopt;
		RampShape shape(this->land, tx, ty, ramp);
		return FormationRow{shape.OuterSide(-1.0), shape.OuterSide(1.0)};
	}

	void AddWallFace(const std::array<WorldPoint, 2> &line, const std::array<WorldPoint, 2> &feet, const MapVector &facing, double shallowest = SHALLOWEST_FACE) const
	{
		double depth = std::max(line[0].level - feet[0].level, line[1].level - feet[1].level);
		if (depth >= shallowest) AddMasonry(this->mesh, line, feet, facing);
	}

	void Close(const FormationRow &row, const MapVector &facing) const
	{
		for (const FormationSide &side : row) this->AddWallFace({side.crest, side.toe}, {this->Seated(side.crest), side.toe}, facing, SHALLOWEST_CAP);
		this->AddWallFace({row[0].crest, row[1].crest}, {this->Seated(row[0].crest), this->Seated(row[1].crest)}, facing, SHALLOWEST_CAP);
	}

	TerrainMesh &mesh;
	const Landform &land;
	const WayCourse &course;
	WayId id;
	double half;
	FormationProfile profile;
	int rows;
};

static void AddFormations(TerrainMesh &mesh, const Landform &land, int tx, int ty)
{
	for (const RaisedWay &way : RaisedWaysOf(tx, ty)) Formation(mesh, land, way.course, way.id, way.half).Lay(way.middle);
}

/* Every step-th corner from the block's first to past its last tile, the last one always included. */
static std::vector<int> LatticeLines(int first, int last, int step)
{
	std::vector<int> lines;
	for (int line = first; line < last + 1; line += step) lines.push_back(line);
	lines.push_back(last + 1);
	return lines;
}

static TerrainMesh BuildLattice(const TileSpan &tiles, const Seabed &bed, int step)
{
	auto laid = [&bed](double x, double y) { return GroundLevel(x, y) - bed.Sink(static_cast<int>(x), static_cast<int>(y)); };
	std::vector<int> xs = LatticeLines(tiles.tx0, tiles.tx1, step);
	std::vector<int> ys = LatticeLines(tiles.ty0, tiles.ty1, step);
	size_t columns = xs.size();
	TerrainMesh mesh;
	std::vector<uint32_t> grid;
	for (int y : ys) {
		for (int x : xs) grid.push_back(mesh.Add({static_cast<double>(x), static_cast<double>(y), laid(x, y)}, ClimbNormal(ClimbOf(laid, x, y, step)), SunkMark(bed.Sink(x, y))));
	}
	auto at = [&](size_t i, size_t j) { return grid[j * columns + i]; };
	for (size_t j = 0; j + 1 < ys.size(); j++) {
		for (size_t i = 0; i + 1 < columns; i++) mesh.Quad(at(i, j), at(i + 1, j), at(i + 1, j + 1), at(i, j + 1));
	}
	SkirtOpenSides(mesh, grid, columns, OpenSidesOf(tiles, tiles), SKIRT_LEVELS_PER_STEP * step);
	return mesh;
}

/* One edge of the map: the line it runs along, across the axis it bounds, which way the sea lies from it, and the corners of a block's side along it. */
struct MapEdge {
	bool bounds_x;
	float line;
	double outward;
	int first;
	int last;

	WorldPoint At(double along, double level, double out = 0.0) const
	{
		double across = this->line + this->outward * out;
		return this->bounds_x ? WorldPoint{across, along, level} : WorldPoint{along, across, level};
	}
};

/* The edges of the map a block of tiles lies along. */
static std::vector<MapEdge> EdgesOf(const TileSpan &tiles)
{
	int last_x = static_cast<int>(Map::MaxX());
	int last_y = static_cast<int>(Map::MaxY());
	std::vector<MapEdge> edges;
	if (tiles.tx0 == 0) edges.push_back({true, 0.0f, -1.0, tiles.ty0, tiles.ty1 + 1});
	if (tiles.tx1 == last_x) edges.push_back({true, static_cast<float>(last_x + 1), 1.0, tiles.ty0, tiles.ty1 + 1});
	if (tiles.ty0 == 0) edges.push_back({false, 0.0f, -1.0, tiles.tx0, tiles.tx1 + 1});
	if (tiles.ty1 == last_y) edges.push_back({false, static_cast<float>(last_y + 1), 1.0, tiles.tx0, tiles.tx1 + 1});
	return edges;
}

/* Where the ground meets the map's edge it is cut down to the sea floor in a face of earth, under the highest ground the block laid along the edge. */
static void AddEdgeFace(TerrainMesh &mesh, const MapEdge &edge)
{
	std::map<float, float> tops;
	for (const TerrainVertex &vertex : mesh.vertices) {
		if ((edge.bounds_x ? vertex.x : vertex.y) != edge.line) continue;
		auto [top, fresh] = tops.try_emplace(edge.bounds_x ? vertex.y : vertex.x, vertex.level);
		if (!fresh) top->second = std::max(top->second, vertex.level);
	}
	if (tops.size() < 2) return;

	Vec3 normal = edge.bounds_x ? Vec3{edge.outward, 0.0, 0.0} : Vec3{0.0, edge.outward, 0.0};
	auto add = [&](double along, double level) { return mesh.Add(edge.At(along, level), normal, EDGE_MARK); };
	for (auto from = tops.begin(), to = std::next(from); to != tops.end(); from = to++) {
		mesh.Quad(add(from->first, from->second), add(to->first, to->second), add(to->first, SEA_FLOOR), add(from->first, SEA_FLOOR));
	}
}

static uint32_t AddSeaFloor(TerrainMesh &mesh, double x, double y)
{
	return mesh.Add({x, y, SEA_FLOOR}, UP, SunkMark(-SEA_FLOOR));
}

/* A corner of the map's edge, sunk as the bed at the border; along the edge it leans as the bed inside does, so the two are lit alike where they meet. */
static uint32_t AddRim(TerrainMesh &mesh, const Seabed &bed, const WorldPoint &at)
{
	int cx = static_cast<int>(at.x);
	int cy = static_cast<int>(at.y);
	double sink = CornerSink(cx, cy);
	return mesh.Add({at.x, at.y, -sink}, sink > 0.0 ? bed.Normal(2 * cx, 2 * cy, 1) : UP, SunkMark(sink));
}

/* Beside a block on the map's edge the seabed falls to the open sea's floor over a shelf, which also fills the nook outside a corner of the map. */
static void AddShelves(TerrainMesh &mesh, const Seabed &bed, std::span<const MapEdge> edges)
{
	double shelf = SHELF_TILES;
	for (const MapEdge &edge : edges) {
		for (int along = edge.first; along < edge.last; along++) {
			WorldPoint far = edge.At(along, SEA_FLOOR, shelf);
			WorldPoint far_next = edge.At(along + 1, SEA_FLOOR, shelf);
			uint32_t near = AddRim(mesh, bed, edge.At(along, 0.0));
			uint32_t next = AddRim(mesh, bed, edge.At(along + 1, 0.0));
			mesh.Quad(near, next, AddSeaFloor(mesh, far_next.x, far_next.y), AddSeaFloor(mesh, far.x, far.y));
		}
	}
	for (const MapEdge &across : edges) {
		for (const MapEdge &down : edges) {
			if (!across.bounds_x || down.bounds_x) continue;
			double x = across.line;
			double y = down.line;
			double dx = across.outward * shelf;
			double dy = down.outward * shelf;
			mesh.Quad(AddRim(mesh, bed, {x, y, 0.0}), AddSeaFloor(mesh, x + dx, y), AddSeaFloor(mesh, x + dx, y + dy), AddSeaFloor(mesh, x, y + dy));
		}
	}
}

TerrainBuild::TerrainBuild(const TileSpan &tiles, GroundGrain grain) :
	tiles(tiles), grain(grain), next_row(tiles.ty0), land(tiles), normals(tiles), shore_cuts(tiles.tx0 - SHORE_CUTS_MARGIN, tiles.ty0 - SHORE_CUTS_MARGIN, tiles.tx1 - tiles.tx0 + 1 + 2 * SHORE_CUTS_MARGIN, tiles.ty1 - tiles.ty0 + 1 + 2 * SHORE_CUTS_MARGIN)
{
}

void TerrainBuild::Advance()
{
	if (this->grain.step > 1) {
		this->mesh = BuildLattice(this->tiles, this->land.Bed(), this->grain.step);
		this->next_row = this->tiles.ty1 + 1;
		return;
	}
	int ty = this->next_row++;
	for (int tx = this->tiles.tx0; tx <= this->tiles.tx1; tx++) {
		this->AddTile(tx, ty);
		AddWalls(this->mesh, this->land, tx, ty);
		AddFormations(this->mesh, this->land, tx, ty);
		AddRampBank(this->mesh, this->land, tx, ty);
	}
}

TerrainMesh TerrainBuild::Finish()
{
	std::vector<MapEdge> edges = EdgesOf(this->tiles);
	for (const MapEdge &edge : edges) AddEdgeFace(this->mesh, edge);
	AddShelves(this->mesh, this->land.Bed(), edges);
	return std::move(this->mesh);
}

TerrainMesh BuildOuterBed(Dimension map, double reach)
{
	TerrainMesh mesh;
	SeaFrame frame(map, SHELF_TILES, reach);
	for (const MapVector &point : frame.points) AddSeaFloor(mesh, point.x, point.y);
	for (const auto &[a, b, c] : frame.triangles) mesh.Triangle(a, b, c);
	return mesh;
}
