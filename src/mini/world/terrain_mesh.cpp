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
static constexpr double COPING_RISE = 0.025;
static constexpr double COPING_OVERHANG = 0.015;
static constexpr double WING_THICKNESS = 0.12;
static constexpr double COPING_DEPTH = WING_THICKNESS + COPING_OVERHANG;
static constexpr double WING_LEAST_RISE = 0.05;
/* A bank's foot dips this far into the ground, in levels, so the two part on a clean line instead of flickering where the bank runs out over it. */
static constexpr double BANK_FOOT_DIP = 0.03;
static constexpr int LEAST_SHORE_DIVISIONS = 2;
static constexpr int BANK_DIVISIONS_FACTOR = 2;
static constexpr int BASIN_SIDE_CUTS = 2;
static constexpr int NO_SHORE_CUTS = 0;
static constexpr ShoreCut NO_SHORE_CUT = {NO_SHORE_CUTS, false};
static constexpr int SHORE_CUTS_MARGIN = 1;
static constexpr double FORMATION_DEPTH = 1.0;
static constexpr double FORMATION_RUN = 2.0;
static constexpr double FORMATION_ROWS_PER_TILE = 4.0;
static constexpr int FORMATION_TOE_STEPS = 8;
static constexpr int FORMATION_TOE_REFINES = 4;
static constexpr Vec3 UPRIGHT = {0.0, 0.0, 1.0};
static constexpr double SEA_FLOOR = -DEEPEST_SINK;

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
	return Length(sum) > 0.0 ? Normalised(sum) : UPRIGHT;
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
		return mesh.Add({top.x, top.y, top.level - drop}, UPRIGHT, DRY_MARK);
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
	LaidGround laid(land, tx, ty);
	if (laid.Rolls()) return {grained, false};
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
	if (IsRiverside(tx, ty)) {
		AddRelief(mesh, ChannelRelief(bed, tx, ty, settled), corners, tx, ty, CHANNEL_DIVISIONS);
		return;
	}
	LaidGround laid(this->land, tx, ty);
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

static Groundwork GroundworkAt(int tx, int ty)
{
	TileIndex tile = TileXY(tx, ty);
	return GroundworkOf(_world_tiles.GroundAt(tile), _world_tiles.NetworkAt(tile));
}

/* How a bank between a tile and a lower one beside it is laid: how far it may reach out over the lower ground, and whether it is earth banked up under a way. */
struct EarthBank {
	double run;
	bool fill;

	/* Whether earth could hold a fall this deep over the bank's run; a cutting is left rock, however steep. */
	bool Holds(const StepFace &step) const
	{
		double fall = std::max(step.ring[0].level - step.ring[1].level, step.ring[3].level - step.ring[2].level) * LevelRise();
		return !this->fill || fall <= this->run * STEEPEST_EARTH;
	}
};

/* Earth banks lean out over the lower tile under ways, short of a way along it and farther over open ground; buildings and tunnel entrances keep stone walls. */
static std::optional<EarthBank> EarthBankOf(int tx, int ty, int nx, int ny)
{
	if (!OnMap(nx, ny) || IsTunnelTile(TileXY(nx, ny))) return std::nullopt;
	Groundwork high = GroundworkAt(tx, ty);
	Groundwork low = GroundworkAt(nx, ny);
	if (high == Groundwork::Built || low == Groundwork::Built || (high == Groundwork::Open && low == Groundwork::Open)) return std::nullopt;
	bool fill = high == Groundwork::Way;
	if (low == Groundwork::Way) return EarthBank{WAYSIDE_EARTH_BANK_RUN, fill};
	int fx = 2 * nx - tx;
	int fy = 2 * ny - ty;
	bool open_beyond = OnMap(fx, fy) && GroundworkAt(fx, fy) == Groundwork::Open;
	return EarthBank{open_beyond ? FAR_EARTH_BANK_RUN : OPEN_EARTH_BANK_RUN, fill};
}

/* The points down one end of a bank, from its top at the step's top to its foot on the ground, and their normals. */
struct BankEdge {
	std::array<WorldPoint, EARTH_BANK_ROWS + 1> points;
	std::array<Vec3, EARTH_BANK_ROWS + 1> normals;
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
	return reach < 1.0 ? low.Level(x, y) : land.LevelAt(x, y);
}

/* One end of an earth bank: from the step's top it grades out over the lower ground, as far as its run allows for the height it falls, onto the ground there;
 * the ground is read a point and how far out it lies. */
template <class GroundUnder>
static BankEdge BankEnd(const Landform &land, const GroundUnder &ground_under, const WorldPoint &top, const WorldPoint &foot, const MapVector &outward, double run, const MapVector &top_climb = {})
{
	double reach = std::clamp((top.level - foot.level) * LevelRise() * EARTH_BANK_RUN_PER_RISE, 0.0, run);
	BankEdge edge;
	for (int row = 0; row <= EARTH_BANK_ROWS; row++) {
		double share = static_cast<double>(row) / EARTH_BANK_ROWS;
		double out = reach * share;
		double x = foot.x + outward.x * out;
		double y = foot.y + outward.y * out;
		double ground = std::min(ground_under(x, y, out) - BANK_FOOT_DIP * share, top.level);
		double fall = BankFall(share);
		MapVector climb = land.Climb(x, y) * fall + top_climb * (1.0 - fall) - outward * ((top.level - ground) * BankFallSlope(share) / std::max(reach, CLIMB_PROBE));
		edge.points[row] = {x, y, top.level + (ground - top.level) * fall};
		edge.normals[row] = ClimbNormal(climb);
	}
	if (AtCorner(top)) edge.normals[0] = SmoothNormal(static_cast<int>(top.x), static_cast<int>(top.y), static_cast<uint8_t>(top.level));
	return edge;
}

static void AddBankStrip(TerrainMesh &mesh, const BankEdge &first, const BankEdge &second, double mark)
{
	uint32_t previous = 0;
	for (int row = 0; row <= EARTH_BANK_ROWS; row++) {
		uint32_t near = mesh.Add(first.points[row], first.normals[row], mark);
		mesh.Add(second.points[row], second.normals[row], mark);
		if (row > 0) mesh.Quad(previous, previous + 1, near + 1, near);
		previous = near;
	}
}

/* An earth bank: its top along the step's top, grading out over the lower tile and on beyond it where its run reaches. */
static std::array<BankEdge, 2> AddEarthBank(TerrainMesh &mesh, const Landform &land, const LaidGround &low, const StepFace &step, const EarthBank &bank)
{
	auto ground_under = [&](double x, double y, double out) { return GroundBeyond(land, low, x, y, out); };
	BankEdge first = BankEnd(land, ground_under, step.ring[0], step.ring[1], step.outward, bank.run);
	BankEdge second = BankEnd(land, ground_under, step.ring[3], step.ring[2], step.outward, bank.run);
	AddBankStrip(mesh, first, second, bank.fill ? FILL_MARK : DRY_MARK);
	return {first, second};
}

/* Two banks grading out from the sides of a corner leave a wedge open between their ends, which a strip closes. */
static void CloseBankCorners(TerrainMesh &mesh, std::span<const BankEdge> edges, std::span<const double> marks)
{
	for (size_t a = 0; a < edges.size(); a++) {
		for (size_t b = a + 1; b < edges.size(); b++) {
			const WorldPoint &top = edges[a].points[0];
			const WorldPoint &other = edges[b].points[0];
			if (top.x != other.x || top.y != other.y || !AtCorner(top)) continue;
			AddBankStrip(mesh, edges[a], edges[b], std::max(marks[a], marks[b]));
		}
	}
}

static void AddWall(TerrainMesh &mesh, const StepFace &step)
{
	Vec3 outward = {step.outward.x, step.outward.y, 0.0};
	std::array<uint32_t, 4> ring;
	std::ranges::transform(step.ring, ring.begin(), [&](const WorldPoint &corner) { return mesh.Add(corner, outward, WALL_MARK); });
	mesh.Quad(ring[0], ring[1], ring[2], ring[3]);
}

static void AddWalls(TerrainMesh &mesh, const Landform &land, int tx, int ty)
{
	std::vector<BankEdge> bank_edges;
	std::vector<double> bank_marks;
	for (DiagDirection side = DIAGDIR_BEGIN; side < DIAGDIR_END; side++) {
		std::optional<StepFace> step = StepFaceOf(tx, ty, side);
		if (!step.has_value()) continue;
		TileIndexDiffC across = TileIndexDiffCByDiagDir(side);
		int nx = tx + across.x;
		int ny = ty + across.y;
		std::optional<EarthBank> bank = StandsInSea(nx, ny) ? std::nullopt : EarthBankOf(tx, ty, nx, ny);
		if (bank.has_value() && bank->Holds(*step)) {
			std::array<BankEdge, 2> edges = AddEarthBank(mesh, land, LaidGround(land, nx, ny), *step, *bank);
			bank_edges.insert(bank_edges.end(), edges.begin(), edges.end());
			bank_marks.insert(bank_marks.end(), 2, bank->fill ? FILL_MARK : DRY_MARK);
		} else {
			AddWall(mesh, *step);
		}
	}
	CloseBankCorners(mesh, bank_edges, bank_marks);
}

static WorldPoint Grounded(const Landform &land, const WorldPoint &at)
{
	return {at.x, at.y, land.LevelAt(at.x, at.y) - BANK_FOOT_DIP};
}

static WorldPoint Shifted(const WorldPoint &at, const MapVector &by, double rise)
{
	return {at.x + by.x, at.y + by.y, at.level + rise / LevelRise()};
}

static void AddStoneQuad(TerrainMesh &mesh, const std::array<WorldPoint, 4> &corners, const Vec3 &normal)
{
	uint32_t first = mesh.Add(corners[0], normal, MASONRY_MARK);
	for (size_t corner = 1; corner < corners.size(); corner++) mesh.Add(corners[corner], normal, MASONRY_MARK);
	mesh.Quad(first, first + 1, first + 2, first + 3);
}

static Vec3 Upright(const MapVector &facing)
{
	return {facing.x, facing.y, 0.0};
}

/* A stone face from a line of points down to the ground under them, facing out one way. */
static void AddMasonry(TerrainMesh &mesh, const Landform &land, std::span<const WorldPoint> line, const MapVector &facing)
{
	Vec3 outward = Upright(facing);
	for (size_t i = 0; i + 1 < line.size(); i++) AddStoneQuad(mesh, {line[i], line[i + 1], Grounded(land, line[i + 1]), Grounded(land, line[i])}, outward);
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

/* A wing wall's top follows the bank it holds, but stands at least a little over the ground all the way, so it ends in a short upright end rather than running out to a point. */
static std::vector<WorldPoint> WingTop(const Landform &land, std::span<const WorldPoint> line)
{
	std::vector<WorldPoint> top;
	for (const WorldPoint &at : line) top.push_back({at.x, at.y, std::max(at.level, land.LevelAt(at.x, at.y) + WING_LEAST_RISE / LevelRise())});
	return top;
}

static std::vector<WorldPoint> ShiftedLine(std::span<const WorldPoint> line, const MapVector &by)
{
	std::vector<WorldPoint> shifted;
	for (const WorldPoint &at : line) shifted.push_back(Shifted(at, by, 0.0));
	return shifted;
}

/* A wing wall is a slab of stone as thick as its coping, its face and back running down to the ground and its outer end closed, under a coping along its top and round its end. */
static void AddWingWall(TerrainMesh &mesh, const Landform &land, std::span<const WorldPoint> line, const MapVector &facing, const MapVector &onward)
{
	std::vector<WorldPoint> front = WingTop(land, line);
	std::vector<WorldPoint> back = ShiftedLine(front, facing * -WING_THICKNESS);
	AddMasonry(mesh, land, front, facing);
	std::ranges::reverse(back);
	AddMasonry(mesh, land, back, facing * -1.0);
	std::ranges::reverse(back);
	const WorldPoint &front_end = front.back();
	const WorldPoint &back_end = back.back();
	Vec3 end_facing = Upright(onward);
	AddStoneQuad(mesh, {front_end, back_end, Grounded(land, back_end), Grounded(land, front_end)}, end_facing);

	Vec3 outward = Upright(facing);
	Vec3 inward = outward * -1.0;
	for (size_t i = 0; i + 1 < front.size(); i++) {
		Vec3 along = {front[i + 1].x - front[i].x, front[i + 1].y - front[i].y, (front[i + 1].level - front[i].level) * LevelRise()};
		Vec3 cap = Normalised(Cross(inward, along));
		if (cap.z < 0.0) cap = cap * -1.0;
		CopingEnd from = CopingAt(front[i], facing);
		CopingEnd to = CopingAt(front[i + 1], facing);
		AddStoneQuad(mesh, {from.front_top, to.front_top, to.front_foot, from.front_foot}, outward);
		AddStoneQuad(mesh, {from.back_top, to.back_top, to.front_top, from.front_top}, cap);
		AddStoneQuad(mesh, {from.back_foot, to.back_foot, to.back_top, from.back_top}, inward);
	}
	CopingEnd end = CopingAt(front_end, facing);
	AddStoneQuad(mesh, {end.front_foot, end.front_top, end.back_top, end.back_foot}, end_facing);
}

static void AddRampBank(TerrainMesh &mesh, const Landform &land, int tx, int ty)
{
	TileIndex head = TileXY(tx, ty);
	RampTexel ramp = _world_tiles.RampAt(head);
	if (!ramp.Present()) return;

	DiagDirection onto = static_cast<DiagDirection>(ramp.onto);
	Footing footing = RampFooting(head, onto, ramp.deck);
	Stretch run = RampRun(head, onto);
	double crown = EMBANKMENT_CROWN_DEPTH / LevelRise();
	auto crest = [&](double share, double side) {
		MapVector at = run.At(share, side * DECK_HALF);
		return WorldPoint{at.x, at.y, footing(at.x, at.y) - crown};
	};
	auto ground_under = [&land](double x, double y, double) { return land.LevelAt(x, y); };
	MapVector facing = Outward(onto);

	std::array<WorldPoint, 2> abutment;
	for (double side : {-1.0, 1.0}) {
		std::vector<BankEdge> edges;
		for (int row = 0; row <= RAMP_BANK_ROWS; row++) {
			WorldPoint top = crest(static_cast<double>(row) / RAMP_BANK_ROWS, side);
			edges.push_back(BankEnd(land, ground_under, top, Grounded(land, top), run.Right() * side, RAMP_BANK_RUN, ClimbOf(footing, top.x, top.y)));
		}
		for (size_t i = 0; i + 1 < edges.size(); i++) AddBankStrip(mesh, edges[i], edges[i + 1], FILL_MARK);
		const auto &end = edges.back().points;
		abutment[side < 0.0 ? 0 : 1] = end.front();
		AddWingWall(mesh, land, end, facing, run.Right() * side);
	}
	AddMasonry(mesh, land, abutment, facing);
}

static double Unfolded(const Bend &line, double share, double lateral)
{
	double curvature = line.Curvature(share);
	return lateral * curvature > 1.0 ? 1.0 / curvature : lateral;
}

/* Where a formation's side, falling from its crest toward its foot, first meets the ground, if it does before its foot. */
static std::optional<WorldPoint> Swallowed(const Landform &land, const WorldPoint &crest, const WorldPoint &foot)
{
	auto buried = [&](double share) {
		WorldPoint at = Between(crest, foot, share);
		return at.level <= land.LevelAt(at.x, at.y);
	};
	double step = 1.0 / FORMATION_TOE_STEPS;
	double high = step;
	while (high <= 1.0 && !buried(high)) high += step;
	if (high > 1.0) return std::nullopt;
	double low = high - step;
	for (int refine = 0; refine < FORMATION_TOE_REFINES; refine++) {
		double middle = (low + high) / 2.0;
		(buried(middle) ? high : low) = middle;
	}
	return Between(crest, foot, high);
}

/* An eased way's bank of the ground's own earth, its sides leaning out and down until the ground swallows them. */
static void AddFormation(TerrainMesh &mesh, const Landform &land, const WayCourse &course, double half)
{
	int rows = std::max(2, static_cast<int>(std::ceil(course.Length() * FORMATION_ROWS_PER_TILE)));
	const Bend &line = course.Line();
	double rise = LevelRise();
	double spread = half + FORMATION_DEPTH * rise * FORMATION_RUN;

	struct Rim {
		double lateral;
		double drop;
	};
	auto strip = [&](Rim inner, Rim outer) {
		uint32_t previous = 0;
		for (int row = 0; row <= rows; row++) {
			double share = static_cast<double>(row) / rows;
			double top = course.Level(share) + WAY_FOOT / rise;
			MapVector at = line.At(share, Unfolded(line, share, inner.lateral));
			MapVector out = line.At(share, Unfolded(line, share, outer.lateral));
			MapVector side = Unit(out - at);
			bool flat = inner.drop == outer.drop;
			Vec3 normal = flat ? UPRIGHT : Normalised(Vec3{side.x, side.y, FORMATION_RUN});
			WorldPoint crest = {at.x, at.y, top - inner.drop};
			WorldPoint foot = {out.x, out.y, top - outer.drop};
			std::optional<WorldPoint> toe = flat ? std::nullopt : Swallowed(land, crest, foot);
			uint32_t near = mesh.Add(crest, normal, FILL_MARK);
			if (toe.has_value()) {
				mesh.Add(Grounded(land, *toe), ClimbNormal(land.Climb(toe->x, toe->y)), FILL_MARK);
			} else {
				mesh.Add(foot, normal, FILL_MARK);
			}
			if (row > 0) mesh.Quad(previous, previous + 1, near + 1, near);
			previous = near;
		}
	};
	strip({-half, 0.0}, {-spread, FORMATION_DEPTH});
	strip({-half, 0.0}, {half, 0.0});
	strip({half, 0.0}, {spread, FORMATION_DEPTH});
}

static void AddFormations(TerrainMesh &mesh, const Landform &land, int tx, int ty)
{
	for (Track track : SetTrackBitIterator(static_cast<TrackBits>(_world_tiles.NetworkAt(TileXY(tx, ty)).track))) {
		if (std::optional<WayCourse> course = WayCourse::OfTrack(tx, ty, track); course.has_value() && course->Raised()) AddFormation(mesh, land, *course, BALLAST_HALF);
	}
	if (std::optional<WayCourse> course = WayCourse::OfRoad(tx, ty); course.has_value() && course->Raised()) AddFormation(mesh, land, *course, ROAD_HALF);
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
	return mesh.Add({x, y, SEA_FLOOR}, UPRIGHT, SunkMark(-SEA_FLOOR));
}

/* A corner of the map's edge, sunk as the bed at the border; along the edge it leans as the bed inside does, so the two are lit alike where they meet. */
static uint32_t AddRim(TerrainMesh &mesh, const Seabed &bed, const WorldPoint &at)
{
	int cx = static_cast<int>(at.x);
	int cy = static_cast<int>(at.y);
	double sink = CornerSink(cx, cy);
	return mesh.Add({at.x, at.y, -sink}, sink > 0.0 ? bed.Normal(2 * cx, 2 * cy, 1) : UPRIGHT, SunkMark(sink));
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
