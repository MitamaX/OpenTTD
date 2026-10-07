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
#include "../map/network_style.h"
#include "../map/tile_shapes.h"
#include "../map/way_course.h"
#include "../map/world_tiles.h"
#include "bridge_models.h"
#include "sea_frame.h"
#include "seabed.h"
#include "shore_relief.h"
#include "track_models.h"
#include "way_shapes.h"

#include "../../safeguards.h"

static constexpr double NORMAL_SCALE = 127.0;
static constexpr double SKIRT_LEVELS_PER_STEP = 2.0;
static constexpr double STEEP_BANK_RUN = 0.35;
static constexpr double LONGEST_BANK_RUN = 1.0;
static constexpr double LEAST_BANK_FALL = 0.25;
static constexpr double BANK_PROBE = 0.25;
static constexpr double EARTH_BANK_RUN_PER_RISE = 2.5;
static constexpr double OPEN_EARTH_BANK_RUN = 1.0;
static constexpr double FAR_EARTH_BANK_RUN = 2.0;
static constexpr double WAYSIDE_EARTH_BANK_RUN = 0.2;
static constexpr int EARTH_BANK_ROWS = 5;
/* Earth stands no steeper than this, in tile widths of fall for each of run; a way banked up any steeper over a way below stands on a retaining wall. */
static constexpr double STEEPEST_EARTH = 1.6;
static constexpr int RAMP_BANK_ROWS = 4;
static constexpr double RAMP_BANK_RUN = 1.2;
static constexpr double GROUND_PROBE = 0.05;
/* A bank's foot dips this far into the ground, in levels, so the two part on a clean line instead of flickering where the bank runs out over it. */
static constexpr double BANK_FOOT_DIP = 0.03;
static constexpr int COAST_DIVISIONS = 4;
static constexpr int CHANNEL_DIVISIONS = 6;
static constexpr double FORMATION_DEPTH = 1.0;
static constexpr double FORMATION_RUN = 2.0;
static constexpr double FORMATION_ROWS_PER_TILE = 4.0;
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
		bool sunk = SharesBasin(tx, ty, tx + side.across_x, ty + side.across_y) && !IsChannel(tx + side.across_x, ty + side.across_y);
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

/* A tile whose ground curves is laid as a fine grid over it, sharing the tile's own corners wherever a neighbour keeps them. */
static void AddRelief(TerrainMesh &mesh, const TileRelief &relief, const std::array<uint32_t, CORNER_COUNT> &corners, int tx, int ty, int divisions)
{
	double water = SurfaceLevelOf(tx, ty);
	int side = divisions + 1;
	std::vector<uint32_t> grid(static_cast<size_t>(side) * side);
	for (int j = 0; j < side; j++) {
		for (int i = 0; i < side; i++) {
			double x = tx + static_cast<double>(i) / divisions;
			double y = ty + static_cast<double>(j) / divisions;
			bool on_corner = (i % divisions == 0) && (j % divisions == 0);
			if (on_corner && relief.Pinned(x, y)) {
				grid[j * side + i] = corners[CornerAt(i / divisions, j / divisions)];
				continue;
			}
			double level = relief.Level(x, y);
			grid[j * side + i] = mesh.Add({x, y, level}, relief.Normal(x, y), SunkMark(std::max(water - level, 0.0)));
		}
	}
	for (int j = 0; j < divisions; j++) {
		for (int i = 0; i < divisions; i++) mesh.Quad(grid[j * side + i], grid[j * side + i + 1], grid[(j + 1) * side + i + 1], grid[(j + 1) * side + i]);
	}
}

static std::array<double, CORNER_COUNT> CornerLevels(const TerrainMesh &mesh, const std::array<uint32_t, CORNER_COUNT> &corners)
{
	std::array<double, CORNER_COUNT> levels;
	std::ranges::transform(corners, levels.begin(), [&](uint32_t index) { return static_cast<double>(mesh.vertices[index].level); });
	return levels;
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
	if (IsRiverside(tx, ty)) {
		AddRelief(mesh, ChannelRelief(bed, tx, ty, CornerLevels(mesh, corners)), corners, tx, ty, CHANNEL_DIVISIONS);
		return;
	}
	if (WaterFormOf(tx, ty) == WaterForm::Open) {
		AddBasin(mesh, bed, corners, tx, ty);
		return;
	}
	if (IsBareCoast(tx, ty)) {
		AddRelief(mesh, ShoreRelief(bed, tx, ty, CornerLevels(mesh, corners)), corners, tx, ty, COAST_DIVISIONS);
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

/* Where a sea bank's top reaches a corner of its tile: the corner, how far out the bank's foot leans there and the way its face looks. */
struct BankCorner {
	WorldPoint top;
	WorldPoint foot;
	Vec3 face;
};

/* A step whose foot stands in the sea leans out into it as a bank of the ground above, instead of standing upright; along its top it shades on as the ground it hangs from. */
static std::array<BankCorner, 2> AddBank(TerrainMesh &mesh, const Seabed &bed, int tx, int ty, const StepFace &step)
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
	return {{{ring[0], ring[1], face}, {ring[3], ring[2], face}}};
}

/* Two sea banks leaning out from the sides of a corner part around it, which a face between their feet closes. */
static void CloseSeaBankCorners(TerrainMesh &mesh, const Seabed &bed, std::span<const BankCorner> corners)
{
	for (size_t a = 0; a < corners.size(); a++) {
		for (size_t b = a + 1; b < corners.size(); b++) {
			const BankCorner &first = corners[a];
			const BankCorner &second = corners[b];
			if (first.top.x != second.top.x || first.top.y != second.top.y || !AtCorner(first.top)) continue;
			Vec3 face = Normalised(first.face + second.face);
			mesh.Triangle(mesh.Add(Settled(bed, first.top), face, DRY_MARK), mesh.Add(Settled(bed, first.foot), face, DRY_MARK), mesh.Add(Settled(bed, second.foot), face, DRY_MARK));
		}
	}
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

/* Ways climb on embankments and run through cuttings whose earth sides lean out over the lower tile, short of any way along it there,
 * and an embankment grades out over open ground as far as the tile beyond when that is open too; buildings keep the stone walls of their foundations. */
static std::optional<EarthBank> EarthBankOf(int tx, int ty, int nx, int ny)
{
	if (!OnMap(nx, ny)) return std::nullopt;
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
static double GroundBeyond(const TileGround &low, double x, double y, double reach)
{
	return reach < 1.0 ? low.Level(x, y) : GroundLevel(x, y);
}

/* How the ground climbs along map x and map y at a point, in levels per tile. */
static MapVector GroundClimb(double x, double y)
{
	double east = GroundLevel(x + GROUND_PROBE, y) - GroundLevel(x - GROUND_PROBE, y);
	double south = GroundLevel(x, y + GROUND_PROBE) - GroundLevel(x, y - GROUND_PROBE);
	return MapVector{east, south} * (0.5 / GROUND_PROBE);
}

static Vec3 ClimbNormal(const MapVector &climb)
{
	double rise = LevelRise();
	return Normalised(Vec3{-climb.x * rise, -climb.y * rise, 1.0});
}

/* One end of an earth bank: from the step's top it grades out over the lower ground, as far as its run allows for the height it falls, onto the ground there;
 * the ground is read a point and how far out it lies. */
template <class GroundUnder>
static BankEdge BankEnd(const GroundUnder &ground_under, const WorldPoint &top, const WorldPoint &foot, const MapVector &outward, double run)
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
		MapVector climb = GroundClimb(x, y) * fall - outward * ((top.level - ground) * BankFallSlope(share) / std::max(reach, GROUND_PROBE));
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
static std::array<BankEdge, 2> AddEarthBank(TerrainMesh &mesh, const TileGround &low, const StepFace &step, const EarthBank &bank)
{
	auto ground_under = [&low](double x, double y, double out) { return GroundBeyond(low, x, y, out); };
	BankEdge first = BankEnd(ground_under, step.ring[0], step.ring[1], step.outward, bank.run);
	BankEdge second = BankEnd(ground_under, step.ring[3], step.ring[2], step.outward, bank.run);
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

static void AddWalls(TerrainMesh &mesh, const Seabed &bed, int tx, int ty)
{
	std::vector<BankEdge> bank_edges;
	std::vector<double> bank_marks;
	std::vector<BankCorner> sea_corners;
	for (DiagDirection side = DIAGDIR_BEGIN; side < DIAGDIR_END; side++) {
		std::optional<StepFace> step = StepFaceOf(tx, ty, side);
		if (!step.has_value()) continue;
		TileIndexDiffC across = TileIndexDiffCByDiagDir(side);
		int nx = tx + across.x;
		int ny = ty + across.y;
		if (StandsInSea(nx, ny)) {
			std::array<BankCorner, 2> ends = AddBank(mesh, bed, tx, ty, *step);
			sea_corners.insert(sea_corners.end(), ends.begin(), ends.end());
		} else if (std::optional<EarthBank> bank = EarthBankOf(tx, ty, nx, ny); bank.has_value() && bank->Holds(*step)) {
			std::array<BankEdge, 2> edges = AddEarthBank(mesh, TileGround(nx, ny), *step, *bank);
			bank_edges.insert(bank_edges.end(), edges.begin(), edges.end());
			bank_marks.insert(bank_marks.end(), 2, bank->fill ? FILL_MARK : DRY_MARK);
		} else {
			AddWall(mesh, *step);
		}
	}
	CloseBankCorners(mesh, bank_edges, bank_marks);
	CloseSeaBankCorners(mesh, bed, sea_corners);
}

static WorldPoint Grounded(const WorldPoint &at)
{
	return {at.x, at.y, GroundLevel(at.x, at.y) - BANK_FOOT_DIP};
}

/* A stone face from a line of points down to the ground under them, facing out one way. */
static void AddMasonry(TerrainMesh &mesh, std::span<const WorldPoint> line, const MapVector &facing)
{
	Vec3 outward = {facing.x, facing.y, 0.0};
	for (size_t i = 0; i + 1 < line.size(); i++) {
		uint32_t top = mesh.Add(line[i], outward, WALL_MARK);
		mesh.Add(line[i + 1], outward, WALL_MARK);
		mesh.Add(Grounded(line[i + 1]), outward, WALL_MARK);
		mesh.Add(Grounded(line[i]), outward, WALL_MARK);
		mesh.Quad(top, top + 1, top + 2, top + 3);
	}
}

/* A bridge head's ramp climbs on a bank of the ground's own earth, grading out over the ground on either side, and meets the span at a stone abutment whose wing walls close the bank's end. */
static void AddRampBank(TerrainMesh &mesh, int tx, int ty)
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
	auto ground_under = [](double x, double y, double) { return GroundLevel(x, y); };

	std::vector<WorldPoint> abutment;
	for (double side : {-1.0, 1.0}) {
		std::vector<BankEdge> edges;
		for (int row = 0; row <= RAMP_BANK_ROWS; row++) {
			WorldPoint top = crest(static_cast<double>(row) / RAMP_BANK_ROWS, side);
			edges.push_back(BankEnd(ground_under, top, Grounded(top), run.Right() * side, RAMP_BANK_RUN));
		}
		for (size_t i = 0; i + 1 < edges.size(); i++) AddBankStrip(mesh, edges[i], edges[i + 1], FILL_MARK);
		const auto &end = edges.back().points;
		if (side < 0.0) {
			abutment.insert(abutment.end(), end.rbegin(), end.rend());
		} else {
			abutment.insert(abutment.end(), end.begin(), end.end());
		}
	}
	AddMasonry(mesh, abutment, Outward(onto));
}

/* A run of samples along a course's middle line, carried on a step beyond each end along the course's heading there, so its offsets end square to it. */
static std::vector<MapVector> FormationLine(const WayCourse &course, int rows)
{
	std::vector<MapVector> line;
	line.push_back(course.Centre(0.0) - course.Before());
	for (int row = 0; row <= rows; row++) line.push_back(course.Centre(static_cast<double>(row) / rows));
	line.push_back(course.Centre(1.0) + course.After());
	return line;
}

/* An eased way stands on a bank of the ground's own earth: a top under the way level with its foot, and sides leaning out and down until the ground swallows them. */
static void AddFormation(TerrainMesh &mesh, const WayCourse &course, double half)
{
	int rows = std::max(2, static_cast<int>(std::ceil(course.Length() * FORMATION_ROWS_PER_TILE)));
	std::vector<MapVector> line = FormationLine(course, rows);
	double rise = LevelRise();
	double spread = half + FORMATION_DEPTH * rise * FORMATION_RUN;

	struct Rim {
		double lateral;
		double drop;
	};
	auto strip = [&](Rim inner, Rim outer) {
		std::vector<MapVector> inside = Offset(line, inner.lateral);
		std::vector<MapVector> outside = Offset(line, outer.lateral);
		uint32_t previous = 0;
		for (int row = 0; row <= rows; row++) {
			double top = course.Level(static_cast<double>(row) / rows) + WAY_FOOT / rise;
			MapVector at = inside[row + 1];
			MapVector out = outside[row + 1];
			MapVector side = Unit(out - at);
			bool flat = inner.drop == outer.drop;
			Vec3 normal = flat ? UPRIGHT : Normalised(Vec3{side.x, side.y, FORMATION_RUN});
			uint32_t near = mesh.Add({at.x, at.y, top - inner.drop}, normal, FILL_MARK);
			mesh.Add({out.x, out.y, top - outer.drop}, normal, FILL_MARK);
			if (row > 0) mesh.Quad(previous, previous + 1, near + 1, near);
			previous = near;
		}
	};
	strip({-half, 0.0}, {-spread, FORMATION_DEPTH});
	strip({-half, 0.0}, {half, 0.0});
	strip({half, 0.0}, {spread, FORMATION_DEPTH});
}

static void AddFormations(TerrainMesh &mesh, int tx, int ty)
{
	for (Track track : SetTrackBitIterator(static_cast<TrackBits>(_world_tiles.NetworkAt(TileXY(tx, ty)).track))) {
		if (std::optional<WayCourse> course = WayCourse::OfTrack(tx, ty, track); course.has_value() && course->Raised()) AddFormation(mesh, *course, BALLAST_HALF);
	}
	if (std::optional<WayCourse> course = WayCourse::OfRoad(tx, ty); course.has_value() && course->Raised()) AddFormation(mesh, *course, ROAD_HALF);
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
		if (tiles.ty0 > 0) skirt(at(i, 0), at(i + 1, 0));
		if (tiles.ty1 < static_cast<int>(Map::MaxY())) skirt(at(i, last_row), at(i + 1, last_row));
	}
	for (size_t j = 0; j + 1 < ys.size(); j++) {
		if (tiles.tx0 > 0) skirt(at(0, j), at(0, j + 1));
		if (tiles.tx1 < static_cast<int>(Map::MaxX())) skirt(at(columns - 1, j), at(columns - 1, j + 1));
	}
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

TerrainBuild::TerrainBuild(const TileSpan &tiles, int step) : tiles(tiles), step(step), next_row(tiles.ty0), bed(tiles)
{
}

void TerrainBuild::Advance()
{
	if (this->step > 1) {
		this->mesh = BuildLattice(this->tiles, this->bed, this->step);
		this->next_row = this->tiles.ty1 + 1;
		return;
	}
	int ty = this->next_row++;
	for (int tx = this->tiles.tx0; tx <= this->tiles.tx1; tx++) {
		AddTile(this->mesh, this->bed, tx, ty);
		AddWalls(this->mesh, this->bed, tx, ty);
		AddFormations(this->mesh, tx, ty);
		AddRampBank(this->mesh, tx, ty);
	}
}

TerrainMesh TerrainBuild::Finish()
{
	std::vector<MapEdge> edges = EdgesOf(this->tiles);
	for (const MapEdge &edge : edges) AddEdgeFace(this->mesh, edge);
	AddShelves(this->mesh, this->bed, edges);
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
