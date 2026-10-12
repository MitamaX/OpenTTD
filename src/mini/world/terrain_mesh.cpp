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
/* Earth stands no steeper than this, in tile widths of fall for each of run; a way banked up any steeper over a way below stands on a retaining wall. */
static constexpr double STEEPEST_EARTH = 1.6;
static constexpr int RAMP_BANK_ROWS = 12;
static constexpr double RAMP_BANK_RUN = 1.2;
static constexpr double RAMP_BANK_RUN_PER_RISE = 1.5;
static constexpr double COPING_RISE = 0.025;
static constexpr double COPING_OVERHANG = 0.015;
static constexpr double WING_THICKNESS = 0.12;
static constexpr double COPING_DEPTH = WING_THICKNESS + COPING_OVERHANG;
/* A bank's foot dips this far into the ground, in levels, so the two part on a clean line instead of flickering where the bank runs out over it. */
static constexpr double BANK_FOOT_DIP = 0.03;
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
static constexpr double WAY_CESS = DECK_HALF - BALLAST_HALF;
static constexpr double SEA_FLOOR = -DEEPEST_SINK;
static constexpr double CREST_ROUNDING = 0.5;
static constexpr double END_PROBE = 0.05;
static constexpr double MIDWAY = 0.5;
static constexpr double OWN_TILE_INSET = 1e-3;
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

Vec3 CornerNormals::At(const Landform &land, int cx, int cy, uint8_t level)
{
	Known &corner = this->corners[static_cast<size_t>(cy - this->tiles.ty0) * this->columns + (cx - this->tiles.tx0)];
	if (!corner.known) corner = {land.RollsRound(cx, cy) ? land.RolledNormal(cx, cy) : SmoothNormal(cx, cy, level), level, true};
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

static Vec3 FacetedNormal(const TerrainMesh &mesh, const std::array<uint32_t, CORNER_COUNT> &corners, double fx, double fy)
{
	Vec3 north = Lerp(VertexNormal(mesh, corners[NORTH]), VertexNormal(mesh, corners[WEST]), fx);
	Vec3 south = Lerp(VertexNormal(mesh, corners[EAST]), VertexNormal(mesh, corners[SOUTH]), fx);
	return Lerp(north, south, fy);
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

/* How a bank between a tile and a lower one beside it is laid: how far it may reach out over the lower ground, and whether it is earth banked up under a way. */
struct EarthBank {
	double run;
	bool fill;

	bool HoldsFall(double fall, double reach) const
	{
		return !this->fill || fall * LevelRise() <= reach * STEEPEST_EARTH;
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

/* The ground under a point beyond a step's foot: the lower tile's own as far as it reaches, and whatever tile lies there further out. */
static double GroundBeyond(const Landform &land, const LaidGround &low, double x, double y, double reach)
{
	return reach <= 1.0 ? low.Level(x, y) : land.LevelAt(x, y);
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

static double EarthReach(const WorldPoint &top, const WorldPoint &foot, double run)
{
	return std::clamp((top.level - foot.level) * LevelRise() * EARTH_BANK_RUN_PER_RISE, 0.0, run);
}

/* One end of a bank: from its top it grades out over the lower ground, a reach out, onto the ground there;
 * the ground is read a point and how far out it lies. */
template <class GroundUnder>
static BankEdge BankEnd(const Landform &land, const GroundUnder &ground_under, const WorldPoint &top, const WorldPoint &foot, const MapVector &outward, double reach, const BankProfile &profile, const MapVector &top_climb = {})
{
	BankEdge edge;
	for (int row = 0; row <= EARTH_BANK_ROWS; row++) {
		double share = static_cast<double>(row) / EARTH_BANK_ROWS;
		double out = reach * share;
		double x = foot.x + outward.x * out;
		double y = foot.y + outward.y * out;
		double ground = std::min(ground_under(x, y, out) - BANK_FOOT_DIP * share, top.level);
		double fall = profile.fall(share);
		MapVector climb = land.Climb(x, y) * fall + top_climb * (1.0 - fall) - outward * ((top.level - ground) * profile.slope(share) / std::max(reach, CLIMB_PROBE));
		edge.points[row] = {x, y, top.level + (ground - top.level) * fall};
		edge.normals[row] = ClimbNormal(climb);
		edge.ground[row] = ground;
	}
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

static Vec3 Upright(const MapVector &facing)
{
	return {facing.x, facing.y, 0.0};
}

static void AddStoneQuad(TerrainMesh &mesh, const std::array<WorldPoint, 4> &corners, const Vec3 &normal)
{
	uint32_t first = mesh.Add(corners[0], normal, MASONRY_MARK);
	for (size_t corner = 1; corner < corners.size(); corner++) mesh.Add(corners[corner], normal, MASONRY_MARK);
	mesh.Quad(first, first + 1, first + 2, first + 3);
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
	return StepBank{*step, *bank, nx, ny};
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

static bool WalledPlain(int tx, int ty, DiagDirection side)
{
	return StepFaceOf(tx, ty, side).has_value() && !BankOnPlain(tx, ty, side).has_value();
}

static bool StepEndsAt(const StepFace &step, const WorldPoint &corner)
{
	return std::ranges::any_of(STEP_ENDS, [&](const std::array<size_t, 2> &end) { return step.ring[end[0]].x == corner.x && step.ring[end[0]].y == corner.y; });
}

static bool WallRisesAt(int tx, int ty, DiagDirection own_side, const WorldPoint &corner)
{
	for (const CornerTile &around : TilesAtCorner(static_cast<int>(corner.x), static_cast<int>(corner.y))) {
		if (!OnMap(around.tx, around.ty)) continue;
		for (DiagDirection side = DIAGDIR_BEGIN; side < DIAGDIR_END; side++) {
			if (around.tx == tx && around.ty == ty && side == own_side) continue;
			std::optional<StepFace> step = StepFaceOf(around.tx, around.ty, side);
			if (step.has_value() && StepEndsAt(*step, corner) && WalledPlain(around.tx, around.ty, side)) return true;
		}
	}
	return false;
}

static bool ShutOutByRiver(int tx, int ty, DiagDirection side, const StepFace &step)
{
	for (const std::array<size_t, 2> &end : STEP_ENDS) {
		const WorldPoint &corner = step.ring[end[0]];
		if (!AtCorner(corner)) continue;
		auto touches_river = [](const CornerTile &around) { return IsRiverWater(around.tx, around.ty); };
		bool on_river = std::ranges::any_of(TilesAtCorner(static_cast<int>(corner.x), static_cast<int>(corner.y)), touches_river);
		if (on_river && WallRisesAt(tx, ty, side, corner)) return true;
	}
	return false;
}

static std::optional<BankedStep> BankOn(int tx, int ty, DiagDirection side)
{
	std::optional<BankedStep> site = BankOnPlain(tx, ty, side);
	return site.has_value() && !ShutOutByRiver(tx, ty, side, site->stepped.step) ? site : std::nullopt;
}

static std::array<BankEdge, 2> BankEdgesOf(const Landform &land, const BankedStep &site)
{
	const StepFace &step = site.stepped.step;
	LaidGround low(land, site.stepped.low_x, site.stepped.low_y);
	auto ground_under = [&](double x, double y, double out) { return GroundBeyond(land, low, x, y, out); };
	auto end_of = [&](size_t end) {
		const WorldPoint &top = step.ring[STEP_ENDS[end][0]];
		const WorldPoint &foot = step.ring[STEP_ENDS[end][1]];
		return BankEnd(land, ground_under, top, foot, step.outward, EarthReach(top, foot, site.runs[end]), EASED_PROFILE);
	};
	return {end_of(0), end_of(1)};
}

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
	std::array<BankEdge, 2> edges = BankEdgesOf(land, *site);
	if (Converted(*site, edges)) return std::nullopt;
	return PlacedBank{*site, edges};
}

static void AddWall(TerrainMesh &mesh, const StepFace &step)
{
	Vec3 outward = {step.outward.x, step.outward.y, 0.0};
	std::array<uint32_t, 4> ring;
	std::ranges::transform(step.ring, ring.begin(), [&](const WorldPoint &corner) { return mesh.Add(corner, outward, WALL_MARK); });
	mesh.Quad(ring[0], ring[1], ring[2], ring[3]);
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
	for (size_t row = 0; row < feet.size(); row++) feet[row] = {edge.points[row].x, edge.points[row].y, edge.ground[row] - BANK_FOOT_DIP};
	AddMasonry(mesh, edge.points, feet, facing);
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
		AddBankStrip(mesh, placed->edges[0], placed->edges[1], MarkOf(placed->site.stepped.bank));
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

static CopingEnd CopingAt(const WorldPoint &at, const MapVector &facing)
{
	MapVector front = facing * COPING_OVERHANG;
	MapVector back = facing * -COPING_DEPTH;
	return {Shifted(at, front, 0.0), Shifted(at, front, COPING_RISE), Shifted(at, back, 0.0), Shifted(at, back, COPING_RISE)};
}

static std::vector<WorldPoint> ShiftedLine(std::span<const WorldPoint> line, const MapVector &by)
{
	std::vector<WorldPoint> shifted;
	for (const WorldPoint &at : line) shifted.push_back(Shifted(at, by, 0.0));
	return shifted;
}

static void CloseWallEnd(TerrainMesh &mesh, const Landform &land, const WorldPoint &front, const WorldPoint &back, const MapVector &facing, const MapVector &end_facing)
{
	Vec3 end_normal = Upright(end_facing);
	AddStoneQuad(mesh, {front, back, Grounded(land, back), Grounded(land, front)}, end_normal);
	CopingEnd end = CopingAt(front, facing);
	AddStoneQuad(mesh, {end.front_foot, end.front_top, end.back_top, end.back_foot}, end_normal);
}

static void AddWingWall(TerrainMesh &mesh, const Landform &land, std::span<const WorldPoint> front, const MapVector &facing, const MapVector &onward, bool closed_both)
{
	std::vector<WorldPoint> back = ShiftedLine(front, facing * -WING_THICKNESS);
	AddMasonry(mesh, front, GroundedLine(land, front), facing);
	std::ranges::reverse(back);
	AddMasonry(mesh, back, GroundedLine(land, back), facing * -1.0);
	std::ranges::reverse(back);
	CloseWallEnd(mesh, land, front.back(), back.back(), facing, onward);
	if (closed_both) CloseWallEnd(mesh, land, front.front(), back.front(), facing, onward * -1.0);

	Vec3 outward = Upright(facing);
	Vec3 inward = outward * -1.0;
	for (size_t i = 0; i + 1 < front.size(); i++) {
		Vec3 along = {front[i + 1].x - front[i].x, front[i + 1].y - front[i].y, (front[i + 1].level - front[i].level) * LevelRise()};
		Vec3 cap = Normalised(Cross(inward, along));
		if (Length(cap) == 0.0) cap = UP;
		if (cap.z < 0.0) cap = cap * -1.0;
		CopingEnd from = CopingAt(front[i], facing);
		CopingEnd to = CopingAt(front[i + 1], facing);
		AddStoneQuad(mesh, {from.front_top, to.front_top, to.front_foot, from.front_foot}, outward);
		AddStoneQuad(mesh, {from.back_top, to.back_top, to.front_top, from.front_top}, cap);
		AddStoneQuad(mesh, {from.back_foot, to.back_foot, to.back_top, from.back_top}, inward);
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

class RampBank {
public:
	RampBank(TerrainMesh &mesh, const Landform &land, int tx, int ty, const RampTexel &ramp) :
		mesh(mesh), land(land), tx(tx), ty(ty), ramp(ramp), onto(static_cast<DiagDirection>(ramp.onto)),
		footing(RampFooting(TileXY(tx, ty), this->onto, ramp.deck)), run(RampRun(TileXY(tx, ty), this->onto)), facing(Outward(this->onto))
	{
	}

	void Lay() const
	{
		this->LayCrestBand();
		for (double side : {-1.0, 1.0}) this->LayFlank(side);
		std::array<WorldPoint, 2> abutment = {this->Crest(1.0, -1.0), this->Crest(1.0, 1.0)};
		AddMasonry(this->mesh, abutment, GroundedLine(this->land, abutment), this->facing);
	}

private:
	enum class Flank : uint8_t {
		Parallel,
		Bank,
		Wall,
	};

	struct FlankPlan {
		Flank kind;
		double run;
	};

	static double ShareOfRow(int row) { return static_cast<double>(row) / RAMP_BANK_ROWS; }

	WorldPoint Crest(double share, double side) const
	{
		MapVector at = this->run.At(share, side * DECK_HALF);
		return {at.x, at.y, this->footing(at.x, at.y) + WAY_FOOT / LevelRise()};
	}

	StripVertex CrestVertex(double share, double side) const
	{
		WorldPoint at = this->Crest(share, side);
		return {at, ClimbNormal(ClimbOf(this->footing, at.x, at.y)), FILL_MARK};
	}

	void LayCrestBand() const
	{
		std::vector<StripRow> rows;
		for (int row = 0; row <= RAMP_BANK_ROWS; row++) rows.push_back({this->CrestVertex(ShareOfRow(row), -1.0), this->CrestVertex(ShareOfRow(row), 1.0)});
		AddStrip(this->mesh, rows);
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

	FlankPlan PlanOf(double side) const
	{
		MapVector right = this->run.Right() * side;
		int sx = this->tx + static_cast<int>(std::lround(right.x));
		int sy = this->ty + static_cast<int>(std::lround(right.y));
		if (this->BesideParallelRamp(sx, sy)) return {Flank::Parallel, HALF_TILE - DECK_HALF};
		if (StopsEarth(sx, sy) || StopsEarth(sx + static_cast<int>(this->facing.x), sy + static_cast<int>(this->facing.y))) return {Flank::Wall, 0.0};
		std::optional<EarthBank> bank = EarthBankOf(this->tx, this->ty, sx, sy);
		if (!bank.has_value()) return {Flank::Wall, 0.0};
		double reach = std::min(RAMP_BANK_RUN, HALF_TILE - DECK_HALF + bank->run);
		return bank->HoldsFall(this->HeadFall(side), reach) ? FlankPlan{Flank::Bank, reach} : FlankPlan{Flank::Wall, 0.0};
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

	BankEdge EdgeAt(int row, double side, double run_limit) const
	{
		WorldPoint top = this->Crest(ShareOfRow(row), side);
		WorldPoint foot = Grounded(this->land, top);
		MapVector outward = this->run.Right() * side;
		double natural = std::clamp((top.level - foot.level) * LevelRise() * RAMP_BANK_RUN_PER_RISE, 0.0, run_limit);
		auto ground_under = [this](double x, double y, double) { return this->land.LevelAt(x, y); };
		return BankEnd(this->land, ground_under, top, foot, outward, this->ReachBefore(top, outward, natural), STRAIGHT_PROFILE, ClimbOf(this->footing, top.x, top.y));
	}

	void LayFlank(double side) const
	{
		FlankPlan plan = this->PlanOf(side);
		MapVector right = this->run.Right() * side;
		if (plan.kind == Flank::Wall) {
			std::vector<WorldPoint> line;
			for (int row = RAMP_BANK_ROWS; row >= 0; row--) line.push_back(this->Crest(ShareOfRow(row), side));
			AddWingWall(this->mesh, this->land, line, right, this->facing * -1.0, true);
			return;
		}
		std::vector<BankEdge> edges;
		for (int row = 0; row <= RAMP_BANK_ROWS; row++) edges.push_back(this->EdgeAt(row, side, plan.run));
		for (size_t i = 0; i + 1 < edges.size(); i++) AddBankStrip(this->mesh, edges[i], edges[i + 1], FILL_MARK);
		if (plan.kind == Flank::Bank) AddWingWall(this->mesh, this->land, edges.back().points, this->facing, right, false);
	}

	TerrainMesh &mesh;
	const Landform &land;
	int tx;
	int ty;
	RampTexel ramp;
	DiagDirection onto;
	Footing footing;
	Stretch run;
	MapVector facing;
};

static void AddRampBank(TerrainMesh &mesh, const Landform &land, int tx, int ty)
{
	RampTexel ramp = _world_tiles.RampAt(TileXY(tx, ty));
	if (ramp.Present()) RampBank(mesh, land, tx, ty, ramp).Lay();
}

static double Unfolded(const Bend &line, double share, double lateral)
{
	double curvature = line.Curvature(share);
	return lateral * curvature > 1.0 ? 1.0 / curvature : lateral;
}

static int TileOf(double coordinate, int own)
{
	int tile = static_cast<int>(std::floor(coordinate));
	return coordinate == tile && own < tile ? tile - 1 : tile;
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

private:
	struct DrawnWay {
		Bend line;
		std::optional<WayCourse> course;
		double half;
	};

	void Collect(int tx, int ty, const WayId &own)
	{
		bool own_tile = tx == own.tx && ty == own.ty;
		for (Track track : SetTrackBitIterator(static_cast<TrackBits>(_world_tiles.NetworkAt(TileXY(tx, ty)).track))) {
			if (own_tile && own.track == track) continue;
			const DrawnTrack &drawn = DrawnTrack::Of(tx, ty, track);
			this->ways.push_back({drawn.line, drawn.course, BALLAST_HALF});
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

struct FormationSide {
	WorldPoint crest;
	Vec3 crest_normal;
	WorldPoint toe;
	Vec3 toe_normal;
};

using FormationRow = std::array<FormationSide, 2>;

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
		auto at = [&](double share) { return Between(crest, foot, share); };
		auto stop_of = [](const std::optional<Crossing> &crossing) { return crossing.has_value() ? crossing->after : NEVER_STOPPED; };
		double wall = stop_of(FirstHolding([&](double share) { return this->Walled(Flat(at(share))); }));
		double cross = stop_of(FirstHolding([&](double share) { return this->Crossed(at(share), crest); }));
		double ground = stop_of(FirstHolding([&](double share) { return this->Buried(at(share)); }));
		if (wall < NEVER_STOPPED && wall <= cross && wall <= ground) return this->ToeAtWall(at(wall));
		if (cross < NEVER_STOPPED && cross <= ground) return this->ToeAtWay(crest, at(cross));
		if (ground < NEVER_STOPPED) return this->ToeOnGround(crest, at(ground));
		WorldPoint seated = Grounded(this->land, foot);
		return {seated, ClimbNormal(this->land.Climb(seated.x, seated.y))};
	}

	Toe ToeAt(Seat seat, const WorldPoint &crest, const WorldPoint &where) const
	{
		switch (seat) {
			case Seat::Wall: return this->ToeAtWall(where);
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
		for (size_t side = 0; side < table.front().size(); side++) this->LaySide(table, side);
		if (middle) this->LayMiddle(table);
		if (!start.continued) this->Close(start.row, this->course.Before() * -1.0);
		if (!finish.continued) this->Close(finish.row, this->course.After());
	}

private:
	void LaySide(std::span<const FormationRow> table, size_t side) const
	{
		std::vector<StripRow> rows;
		for (const FormationRow &row : table) rows.push_back({StripVertex{row[side].crest, row[side].crest_normal, FILL_MARK}, StripVertex{row[side].toe, row[side].toe_normal, FILL_MARK}});
		AddStrip(this->mesh, rows);
	}

	void LayMiddle(std::span<const FormationRow> table) const
	{
		std::vector<StripRow> rows;
		for (const FormationRow &row : table) rows.push_back({StripVertex{row[0].crest, UP, FILL_MARK}, StripVertex{row[1].crest, UP, FILL_MARK}});
		AddStrip(this->mesh, rows);
	}

	void Close(const FormationRow &row, const MapVector &facing) const
	{
		for (const FormationSide &side : row) {
			std::array<WorldPoint, 2> line = {side.crest, side.toe};
			AddMasonry(this->mesh, line, std::array<WorldPoint, 2>{this->Seated(side.crest), side.toe}, facing);
		}
		std::array<WorldPoint, 2> line = {row[0].crest, row[1].crest};
		AddMasonry(this->mesh, line, std::array<WorldPoint, 2>{this->Seated(row[0].crest), this->Seated(row[1].crest)}, facing);
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
