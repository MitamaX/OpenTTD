/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mesh_check.cpp A survey of the ground and water meshes of the loaded map, built by the same code the renderer builds them with, for the faults a viewer would see. */

#include "../../stdafx.h"
#include "mesh_check.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <cstdlib>
#include <limits>
#include <map>
#include <numeric>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "../../core/string_consumer.hpp"
#include "../../map_func.h"
#include "../../openttd.h"
#include "../../string_func.h"
#include "../../track_func.h"
#include "../core/tuning.h"
#include "../map/way_course.h"
#include "../map/world_tiles.h"
#include "bridge_models.h"
#include "chunk_grid.h"
#include "terrain_mesh.h"
#include "water_mesh.h"
#include "water_outline.h"

#include "../../safeguards.h"

static constexpr const char OUTPUT_VARIABLE[] = "OTTD_MINI_MESHCHECK";
static constexpr const char AREA_VARIABLE[] = "OTTD_MINI_MESHCHECK_AREA";

/* Blocks are built this far past the surveyed area so faults on its rim see their neighbours. */
static constexpr int AREA_MARGIN = 2;
static constexpr double GRID_CELL = 0.25;
static constexpr double POINT_QUANTUM = 1.0 / 4096.0;
static constexpr double STEEP_NZ = 0.1;
static constexpr double DEGENERATE_AREA = 1e-7;

static constexpr double GAP_PROBE = 0.01;
static constexpr double GAP_TOLERANCE_LEVELS = 0.04;
static constexpr double CLOSING_REACH = 0.03;
static constexpr double COPLANAR_DOT = 0.9995;
static constexpr double COPLANAR_DISTANCE = 0.002;
static constexpr double OVERLAP_LEAST_AREA = 1e-3;
/* Overlapping faces of one layer flicker only where they are lit apart. */
static constexpr double OVERLAP_SHADE_DEGREES = 10.0;
static constexpr double FOLD_DEGREES = 40.0;
/* Crests and toes of banks run level along their ways and may fold as steeply as the steepest earth stands. */
static constexpr double CREASE_INCLINE_DEGREES = 15.0;
static constexpr double CREASE_DEGREES = 65.0;
static constexpr double FIN_DEGREES = 120.0;
static constexpr double SHADE_SEAM_DEGREES = 25.0;
static constexpr double FLIPPED_NORMAL_DOT = 0.0;
static constexpr double WATER_FLOAT_LEVELS = 0.25;
static constexpr double CAP_NZ = 0.7;
static constexpr double SLAB_REST_LEVELS = 0.1;
/* As far as a coping reaches back over the wall it caps. */
static constexpr double SLAB_SUPPORT_REACH = 0.16;

static constexpr int WAY_SAMPLES = 16;
static constexpr double WAY_JOINT_REACH = 0.002;
static constexpr double WAY_STEP_REACH = 0.25;
static constexpr double WAY_KINK_DEGREES = 6.0;
static constexpr double WAY_GRADE_KINK_DEGREES = 3.0;
static constexpr double WAY_BEND_DEGREES_PER_TILE = 120.0;
static constexpr double WAY_CREST_DEGREES_PER_TILE = 30.0;

static constexpr double WAY_COVER_MARGIN = 0.05;
static constexpr double WAY_COVER_ABOVE_LEVELS = 0.05;
static constexpr double WAY_COVER_DEPTH_LEVELS = 0.6;

static constexpr double RADIANS_TO_DEGREES = 180.0 / HALF_TURN;

enum class Layer : uint8_t { Sunk, Ground, Armour, Fill, Edge, Masonry, Wall };

static const char *LayerName(Layer layer)
{
	static constexpr std::array<const char *, 7> NAMES = {"sunk", "ground", "armour", "fill", "edge", "masonry", "wall"};
	return NAMES[static_cast<size_t>(layer)];
}

static Layer LayerOf(int8_t mark)
{
	if (mark == SignedByte(WALL_MARK)) return Layer::Wall;
	if (mark == SignedByte(MASONRY_MARK)) return Layer::Masonry;
	if (mark == SignedByte(EDGE_MARK)) return Layer::Edge;
	if (mark == SignedByte(FILL_MARK)) return Layer::Fill;
	if (mark == SignedByte(ARMOUR_MARK)) return Layer::Armour;
	return mark < 0 ? Layer::Sunk : Layer::Ground;
}

static bool IsEarth(Layer layer)
{
	return layer <= Layer::Fill;
}

static bool IsStone(Layer layer)
{
	return layer == Layer::Masonry || layer == Layer::Wall;
}

static double Degrees(double cosine)
{
	return std::acos(std::clamp(cosine, -1.0, 1.0)) * RADIANS_TO_DEGREES;
}

struct Face {
	std::array<Vec3, 3> at;
	std::array<Vec3, 3> normals;
	std::array<uint32_t, 3> points;
	Vec3 normal;
	double area;
	Layer layer;

	Vec3 Centroid() const { return (this->at[0] + this->at[1] + this->at[2]) * (1.0 / 3.0); }
	bool Steep() const { return std::abs(this->normal.z) < STEEP_NZ; }

	/* The face's height above a map point, if it lies over it. */
	std::optional<double> HeightAt(double x, double y) const
	{
		const Vec3 &a = this->at[0], &b = this->at[1], &c = this->at[2];
		double det = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
		if (std::abs(det) < DEGENERATE_AREA) return std::nullopt;
		double u = ((b.y - c.y) * (x - c.x) + (c.x - b.x) * (y - c.y)) / det;
		double v = ((c.y - a.y) * (x - c.x) + (a.x - c.x) * (y - c.y)) / det;
		double w = 1.0 - u - v;
		constexpr double SLACK = -1e-9;
		if (u < SLACK || v < SLACK || w < SLACK) return std::nullopt;
		return u * a.z + v * b.z + w * c.z;
	}
};

/* Earth hung straight down from a block's open side, hidden under the ground beside it. */
static bool IsSkirt(const Face &face)
{
	return IsEarth(face.layer) && face.Steep();
}

static Vec3 ClosestOnTriangle(const Vec3 &p, const Vec3 &a, const Vec3 &b, const Vec3 &c)
{
	Vec3 ab = b - a, ac = c - a, ap = p - a;
	double d1 = Dot(ab, ap), d2 = Dot(ac, ap);
	if (d1 <= 0 && d2 <= 0) return a;
	Vec3 bp = p - b;
	double d3 = Dot(ab, bp), d4 = Dot(ac, bp);
	if (d3 >= 0 && d4 <= d3) return b;
	double vc = d1 * d4 - d3 * d2;
	if (vc <= 0 && d1 >= 0 && d3 <= 0) return a + ab * (d1 / (d1 - d3));
	Vec3 cp = p - c;
	double d5 = Dot(ab, cp), d6 = Dot(ac, cp);
	if (d6 >= 0 && d5 <= d6) return c;
	double vb = d5 * d2 - d1 * d6;
	if (vb <= 0 && d2 >= 0 && d6 <= 0) return a + ac * (d2 / (d2 - d6));
	double va = d3 * d6 - d5 * d4;
	if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
	double denom = 1.0 / (va + vb + vc);
	return a + ab * (vb * denom) + ac * (vc * denom);
}

static double DistanceTo(const Face &face, const Vec3 &p)
{
	return Length(p - ClosestOnTriangle(p, face.at[0], face.at[1], face.at[2]));
}

struct PointKey {
	int32_t x, y, z;
	bool operator==(const PointKey &) const = default;
};

struct PointKeyHash {
	size_t operator()(const PointKey &key) const
	{
		uint64_t h = static_cast<uint32_t>(key.x);
		h = h * 0x9E3779B97F4A7C15ULL ^ static_cast<uint32_t>(key.y);
		h = h * 0x9E3779B97F4A7C15ULL ^ static_cast<uint32_t>(key.z);
		return static_cast<size_t>(h ^ (h >> 29));
	}
};

static int64_t TileKey(double x, double y)
{
	return static_cast<int64_t>(std::floor(x)) << 32 | static_cast<uint32_t>(std::floor(y));
}

static int32_t Quantised(double value)
{
	return static_cast<int32_t>(std::lround(value / POINT_QUANTUM));
}

/* Tiles of the map a survey reports on; faults beyond it are left out. */
struct Area {
	int x0, y0, x1, y1;
	bool Holds(int x, int y) const { return x >= this->x0 && y >= this->y0 && x <= this->x1 && y <= this->y1; }
};

struct Finding {
	double value = 0.0;
	int count = 0;
	std::string detail;
};

class Findings {
public:
	explicit Findings(const Area &area) : area(area) {}

	void Note(const std::string &kind, double x, double y, double value, const std::string &detail = {})
	{
		int tx = static_cast<int>(std::floor(x));
		int ty = static_cast<int>(std::floor(y));
		if (!this->area.Holds(tx, ty)) return;
		Finding &finding = this->found[{kind, tx, ty}];
		finding.count++;
		if (finding.count == 1 || value > finding.value) {
			finding.value = value;
			finding.detail = detail;
		}
	}

	const std::map<std::tuple<std::string, int, int>, Finding> &All() const { return this->found; }

private:
	Area area;
	std::map<std::tuple<std::string, int, int>, Finding> found;
};

/* Faces of the map filed under the cells their footprint covers. */
class FaceGrid {
public:
	FaceGrid(const std::vector<Face> &faces, int width, int height) : faces(faces), columns(static_cast<int>(width / GRID_CELL)), rows(static_cast<int>(height / GRID_CELL)), cells(static_cast<size_t>(columns) * rows)
	{
		for (uint32_t index = 0; index < faces.size(); index++) {
			auto [x0, y0, x1, y1] = this->CellsOf(faces[index]);
			for (int y = y0; y <= y1; y++) {
				for (int x = x0; x <= x1; x++) this->cells[this->Cell(x, y)].push_back(index);
			}
		}
	}

	std::tuple<int, int, int, int> CellsOf(const Face &face) const
	{
		double lx = std::min({face.at[0].x, face.at[1].x, face.at[2].x}), hx = std::max({face.at[0].x, face.at[1].x, face.at[2].x});
		double ly = std::min({face.at[0].y, face.at[1].y, face.at[2].y}), hy = std::max({face.at[0].y, face.at[1].y, face.at[2].y});
		return {this->Column(lx), this->Row(ly), this->Column(hx), this->Row(hy)};
	}

	/* Every face whose cell lies within a reach of a map point. */
	template <class Visit>
	void Near(double x, double y, double reach, Visit visit) const
	{
		for (int row = this->Row(y - reach); row <= this->Row(y + reach); row++) {
			for (int column = this->Column(x - reach); column <= this->Column(x + reach); column++) {
				for (uint32_t index : this->cells[this->Cell(column, row)]) visit(index, this->faces[index]);
			}
		}
	}

	int Columns() const { return this->columns; }
	int Rows() const { return this->rows; }
	const std::vector<uint32_t> &At(int column, int row) const { return this->cells[this->Cell(column, row)]; }

private:
	int Column(double x) const { return std::clamp(static_cast<int>(std::floor(x / GRID_CELL)), 0, this->columns - 1); }
	int Row(double y) const { return std::clamp(static_cast<int>(std::floor(y / GRID_CELL)), 0, this->rows - 1); }
	size_t Cell(int column, int row) const { return static_cast<size_t>(row) * this->columns + column; }

	const std::vector<Face> &faces;
	int columns;
	int rows;
	std::vector<std::vector<uint32_t>> cells;
};

static std::string Pair(Layer first, Layer second)
{
	return std::string(LayerName(std::min(first, second))) + "/" + LayerName(std::max(first, second));
}

class MeshSurvey {
public:
	MeshSurvey(const Area &area, Findings &findings) : area(area), findings(findings), rise(LevelRise()), width(static_cast<int>(Map::SizeX())), height(static_cast<int>(Map::SizeY())) {}

	void Build()
	{
		ChunkGrid blocks(GROUND_BLOCK_TILES);
		blocks.Lay(Dimension(this->width, this->height));
		GroundGrain finest{1, BlockTiles().set()};
		for (size_t index = 0; index < blocks.Count(); index++) {
			TileSpan tiles = blocks.TilesOf(index);
			if (tiles.tx1 < this->area.x0 - AREA_MARGIN || tiles.tx0 > this->area.x1 + AREA_MARGIN || tiles.ty1 < this->area.y0 - AREA_MARGIN || tiles.ty0 > this->area.y1 + AREA_MARGIN) continue;
			TerrainBuild build(tiles, finest);
			while (!build.Done()) build.Advance();
			this->AddTerrain(build.Finish());
			this->AddWater(BuildWaterSurface(tiles));
		}
		this->grid.emplace(this->faces, this->width, this->height);
		this->LayWays();
	}

	void CheckEdges()
	{
		struct Side {
			uint64_t edge;
			uint32_t face;
			uint8_t corner;
		};
		std::vector<Side> sides;
		sides.reserve(this->faces.size() * 3);
		for (uint32_t index = 0; index < this->faces.size(); index++) {
			const Face &face = this->faces[index];
			if (face.area < DEGENERATE_AREA) continue;
			for (uint8_t corner = 0; corner < 3; corner++) {
				uint32_t a = face.points[(corner + 1) % 3], b = face.points[(corner + 2) % 3];
				if (a == b) continue;
				sides.push_back({static_cast<uint64_t>(std::min(a, b)) << 32 | std::max(a, b), index, corner});
			}
		}
		std::ranges::sort(sides, {}, [](const Side &side) { return std::make_tuple(side.edge, side.face, side.corner); });
		for (size_t first = 0; first < sides.size();) {
			size_t last = first;
			while (last < sides.size() && sides[last].edge == sides[first].edge) last++;
			if (last - first == 1) this->CheckOpenEdge(sides[first].face, sides[first].corner);
			if (last - first == 2) this->CheckJoin(sides[first].face, sides[first].corner, sides[first + 1].face, sides[first + 1].corner);
			first = last;
		}
	}

	void CheckFaces()
	{
		for (const Face &face : this->faces) {
			Vec3 centre = face.Centroid();
			if (face.area < DEGENERATE_AREA) {
				this->findings.Note("degenerate", centre.x, centre.y, 1.0, LayerName(face.layer));
				continue;
			}
			if (!IsEarth(face.layer) || face.Steep()) continue;
			Vec3 up = face.normal.z < 0.0 ? face.normal * -1.0 : face.normal;
			double worst = 1.0;
			for (const Vec3 &normal : face.normals) worst = std::min(worst, Dot(Normalised(normal), up));
			if (worst < FLIPPED_NORMAL_DOT) this->findings.Note("normal_flip", centre.x, centre.y, Degrees(worst), LayerName(face.layer));
		}
	}

	void CheckOverlaps()
	{
		const FaceGrid &grid = *this->grid;
		for (int row = 0; row < grid.Rows(); row++) {
			for (int column = 0; column < grid.Columns(); column++) {
				const std::vector<uint32_t> &cell = grid.At(column, row);
				for (size_t i = 0; i < cell.size(); i++) {
					for (size_t j = i + 1; j < cell.size(); j++) this->CheckOverlap(cell[i], cell[j], column, row);
				}
			}
		}
	}

	void CheckWater()
	{
		for (const Vec3 &at : this->water_points) {
			std::optional<std::pair<double, Layer>> ground = this->EarthTop(at.x, at.y);
			if (!ground.has_value() || ground->second == Layer::Sunk) continue;
			double gap = (at.z - ground->first) / this->rise;
			if (gap > WATER_FLOAT_LEVELS) this->findings.Note("water_float", at.x, at.y, gap, LayerName(ground->second));
		}
	}

	/* A cap of stone rests on the earth or on the wall of another piece under it; one with neither hangs loose. */
	void CheckSlabs()
	{
		std::vector<uint32_t> piece = this->StonePieces();
		for (uint32_t index = 0; index < this->faces.size(); index++) {
			const Face &cap = this->faces[index];
			if (cap.layer != Layer::Masonry || std::abs(cap.normal.z) < CAP_NZ || cap.area < DEGENERATE_AREA) continue;
			Vec3 centre = cap.Centroid();
			std::optional<std::pair<double, Layer>> ground = this->EarthTop(centre.x, centre.y);
			double hang = ground.has_value() ? (centre.z - ground->first) / this->rise : 0.0;
			if (hang <= SLAB_REST_LEVELS) continue;
			bool held = false;
			this->grid->Near(centre.x, centre.y, SLAB_SUPPORT_REACH, [&](uint32_t other, const Face &face) {
				if (held || !IsStone(face.layer) || !face.Steep() || piece[other] == piece[index]) return;
				if (DistanceTo(face, centre) <= SLAB_SUPPORT_REACH) held = true;
			});
			if (!held) this->findings.Note("slab_float", centre.x, centre.y, hang);
		}
	}

	void CheckWays()
	{
		std::vector<WayEnd> ends;
		for (const WayTrace &way : this->ways) this->CheckWay(way, ends);
		this->CheckJoints(ends);
	}

	size_t FaceCount() const { return this->faces.size(); }

private:
	struct WayTrace {
		std::string kind;
		std::array<Vec3, WAY_SAMPLES + 1> line;
	};

	struct WayEnd {
		std::string kind;
		Vec3 at;
		MapVector outward;
		double grade;
	};

	Vec3 Render(double x, double y, double level) const { return {x, y, level * this->rise}; }

	uint32_t PointOf(const Vec3 &at)
	{
		auto [found, fresh] = this->points.try_emplace(PointKey{Quantised(at.x), Quantised(at.y), Quantised(at.z)}, static_cast<uint32_t>(this->points.size()));
		return found->second;
	}

	bool OnMap(const Vec3 &at) const
	{
		return at.x >= 0.0 && at.y >= 0.0 && at.x <= this->width && at.y <= this->height;
	}

	void AddTerrain(const TerrainMesh &mesh)
	{
		for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
			Face face;
			Layer layer = Layer::Sunk;
			for (size_t corner = 0; corner < 3; corner++) {
				const TerrainVertex &vertex = mesh.vertices[mesh.indices[i + corner]];
				face.at[corner] = this->Render(vertex.x, vertex.y, vertex.level);
				face.normals[corner] = {vertex.normal[0] / NORMALISED_BYTE_MAX, vertex.normal[1] / NORMALISED_BYTE_MAX, vertex.normal[2] / NORMALISED_BYTE_MAX};
				layer = std::max(layer, LayerOf(vertex.normal[3]));
			}
			if (!this->OnMap(face.Centroid())) continue;
			for (size_t corner = 0; corner < 3; corner++) face.points[corner] = this->PointOf(face.at[corner]);
			Vec3 cross = Cross(face.at[1] - face.at[0], face.at[2] - face.at[0]);
			face.area = Length(cross) * 0.5;
			face.normal = Normalised(cross);
			face.layer = layer;
			this->faces.push_back(face);
		}
	}

	/* Points of the water's sheets inside the outline the water shaders draw. */
	void AddWater(const WaterMesh &mesh)
	{
		for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
			std::array<WaterVertex, 3> corners;
			for (size_t corner = 0; corner < 3; corner++) corners[corner] = mesh.vertices[mesh.indices[i + corner]];
			WaterVertex centre = {(corners[0].x + corners[1].x + corners[2].x) / 3.0f, (corners[0].y + corners[1].y + corners[2].y) / 3.0f, (corners[0].level + corners[1].level + corners[2].level) / 3.0f};
			Vec3 at = this->Render(centre.x, centre.y, centre.level);
			if (this->OnMap(at) && SheetWater(centre.x, centre.y, centre.level) >= WATERLINE) this->water_points.push_back(at);
		}
	}

	/* The highest earth over a map point, and what kind it is. */
	std::optional<std::pair<double, Layer>> EarthTop(double x, double y) const
	{
		std::optional<std::pair<double, Layer>> top;
		this->grid->Near(x, y, 0.0, [&](uint32_t, const Face &face) {
			if (!IsEarth(face.layer) || face.Steep()) return;
			std::optional<double> h = face.HeightAt(x, y);
			if (h.has_value() && (!top.has_value() || *h > top->first)) top = {*h, face.layer};
		});
		return top;
	}

	bool OnMapBorder(const Vec3 &a, const Vec3 &b) const
	{
		auto border = [](double first, double second, double line) { return std::abs(first - line) < POINT_QUANTUM && std::abs(second - line) < POINT_QUANTUM; };
		return border(a.x, b.x, 0.0) || border(a.y, b.y, 0.0) || border(a.x, b.x, this->width) || border(a.y, b.y, this->height);
	}

	/* An edge of earth no other face shares: harmless where the earth beside it meets it at the same height, or a wall or skirt closes the step. */
	void CheckOpenEdge(uint32_t index, uint8_t corner)
	{
		const Face &face = this->faces[index];
		if (!IsEarth(face.layer) || face.Steep()) return;
		const Vec3 &a = face.at[(corner + 1) % 3], &b = face.at[(corner + 2) % 3], &c = face.at[corner];
		if (this->OnMapBorder(a, b)) return;
		Vec3 mid = (a + b) * 0.5;
		MapVector along = {b.x - a.x, b.y - a.y};
		double length = std::hypot(along.x, along.y);
		if (length < POINT_QUANTUM) return;
		MapVector out = {along.y / length, -along.x / length};
		if (out.x * (c.x - mid.x) + out.y * (c.y - mid.y) > 0.0) out = out * -1.0;
		double px = mid.x + out.x * GAP_PROBE, py = mid.y + out.y * GAP_PROBE;
		double ix = mid.x - out.x * GAP_PROBE, iy = mid.y - out.y * GAP_PROBE;
		double tolerance = GAP_TOLERANCE_LEVELS * this->rise;

		std::optional<double> highest;
		bool hidden = false;
		bool closed = false;
		this->grid->Near(mid.x, mid.y, CLOSING_REACH, [&](uint32_t other, const Face &beside) {
			if (other == index) return;
			if (beside.Steep() || !IsEarth(beside.layer)) {
				if (DistanceTo(beside, mid) < CLOSING_REACH) closed = true;
				return;
			}
			std::optional<double> over = beside.HeightAt(ix, iy);
			if (over.has_value() && *over > mid.z + tolerance) hidden = true;
			std::optional<double> h = beside.HeightAt(px, py);
			if (h.has_value() && (!highest.has_value() || *h > *highest)) highest = *h;
		});
		if (hidden || closed || this->UnderWay(mid) || (highest.has_value() && *highest >= mid.z - tolerance)) return;
		if (!highest.has_value()) {
			this->findings.Note("hole", mid.x, mid.y, length, LayerName(face.layer));
			return;
		}
		this->findings.Note("gap", mid.x, mid.y, (mid.z - *highest) / this->rise, LayerName(face.layer));
	}

	/* Two faces of earth on one edge: how sharply the surface folds there, and whether their shading breaks where the surface runs on smoothly. */
	void CheckJoin(uint32_t first_index, uint8_t first_corner, uint32_t second_index, uint8_t second_corner)
	{
		const Face &first = this->faces[first_index], &second = this->faces[second_index];
		if (!IsEarth(first.layer) || !IsEarth(second.layer) || first.Steep() || second.Steep()) return;
		uint8_t a = (first_corner + 1) % 3, b = (first_corner + 2) % 3;
		const Vec3 &pa = first.at[a], &pb = first.at[b];
		const Vec3 &pc = first.at[first_corner], &pd = second.at[second_corner];
		double fold = Degrees(Dot(Normalised(Cross(pb - pa, pc - pa)), Normalised(Cross(pa - pb, pd - pb))));
		Vec3 mid = (pa + pb) * 0.5;
		std::string layers = Pair(first.layer, second.layer);
		if (fold >= FIN_DEGREES) {
			this->findings.Note("fin", mid.x, mid.y, fold, layers);
			return;
		}
		double incline = std::atan2(std::abs(pb.z - pa.z), std::hypot(pb.x - pa.x, pb.y - pa.y)) * RADIANS_TO_DEGREES;
		bool level = incline < CREASE_INCLINE_DEGREES;
		if (fold >= (level ? CREASE_DEGREES : FOLD_DEGREES)) {
			this->findings.Note(level ? "crease" : "fold", mid.x, mid.y, fold, layers);
			return;
		}
		double seam = 0.0;
		for (uint8_t corner : {a, b}) {
			auto match = std::ranges::find(second.points, first.points[corner]);
			const Vec3 &other = second.normals[match - second.points.begin()];
			seam = std::max(seam, Degrees(Dot(Normalised(first.normals[corner]), Normalised(other))));
		}
		if (seam >= SHADE_SEAM_DEGREES) this->findings.Note("shade_seam", mid.x, mid.y, seam, layers);
	}

	void CheckOverlap(uint32_t i, uint32_t j, int column, int row)
	{
		const Face &first = this->faces[i], &second = this->faces[j];
		if (first.area < DEGENERATE_AREA || second.area < DEGENERATE_AREA) return;
		if (IsSkirt(first) && IsSkirt(second)) return;
		if (std::abs(Dot(first.normal, second.normal)) < COPLANAR_DOT) return;
		double offset = Dot(first.normal, first.at[0]);
		for (const Vec3 &at : second.at) {
			if (std::abs(Dot(first.normal, at) - offset) > COPLANAR_DISTANCE) return;
		}
		auto [fx0, fy0, fx1, fy1] = this->grid->CellsOf(first);
		auto [sx0, sy0, sx1, sy1] = this->grid->CellsOf(second);
		if (std::max(fx0, sx0) != column || std::max(fy0, sy0) != row) return;
		double area = OverlapArea(first, second);
		if (area < OVERLAP_LEAST_AREA || (first.layer == second.layer && !ShadedApart(first, second))) return;
		Vec3 centre = first.Centroid();
		this->findings.Note("overlap", centre.x, centre.y, area, Pair(first.layer, second.layer));
	}

	static bool ShadedApart(const Face &first, const Face &second)
	{
		auto mean = [](const Face &face) { return Normalised(face.normals[0] + face.normals[1] + face.normals[2]); };
		return Degrees(Dot(mean(first), mean(second))) > OVERLAP_SHADE_DEGREES;
	}

	/* The area two coplanar faces share, both laid flat in the first one's plane. */
	static double OverlapArea(const Face &first, const Face &second)
	{
		Vec3 u = Normalised(first.at[1] - first.at[0]);
		Vec3 v = Cross(first.normal, u);
		using Flat = std::pair<double, double>;
		auto flat = [&](const Vec3 &at) { Vec3 d = at - first.at[0]; return Flat{Dot(d, u), Dot(d, v)}; };
		auto cross = [](const Flat &o, const Flat &a, const Flat &b) { return (a.first - o.first) * (b.second - o.second) - (a.second - o.second) * (b.first - o.first); };
		std::vector<Flat> clip = {flat(first.at[0]), flat(first.at[1]), flat(first.at[2])};
		if (cross(clip[0], clip[1], clip[2]) < 0.0) std::swap(clip[1], clip[2]);
		std::vector<Flat> shape = {flat(second.at[0]), flat(second.at[1]), flat(second.at[2])};
		for (size_t e = 0; e < clip.size() && !shape.empty(); e++) {
			const Flat &p = clip[e], &q = clip[(e + 1) % clip.size()];
			std::vector<Flat> kept;
			for (size_t k = 0; k < shape.size(); k++) {
				const Flat &s = shape[k], &t = shape[(k + 1) % shape.size()];
				double ds = cross(p, q, s), dt = cross(p, q, t);
				if (ds >= 0.0) kept.push_back(s);
				if ((ds >= 0.0) != (dt >= 0.0)) {
					double share = ds / (ds - dt);
					kept.push_back({s.first + (t.first - s.first) * share, s.second + (t.second - s.second) * share});
				}
			}
			shape = std::move(kept);
		}
		double area = 0.0;
		for (size_t k = 0; k < shape.size(); k++) area += shape[k].first * shape[(k + 1) % shape.size()].second - shape[(k + 1) % shape.size()].first * shape[k].second;
		return std::abs(area) * 0.5;
	}

	/* Which piece of stone each stone face belongs to, the faces of a piece meeting at shared points. */
	std::vector<uint32_t> StonePieces() const
	{
		std::vector<uint32_t> parent(this->faces.size());
		std::iota(parent.begin(), parent.end(), 0);
		auto root = [&](uint32_t i) {
			while (parent[i] != i) i = parent[i] = parent[parent[i]];
			return i;
		};
		std::unordered_map<uint32_t, uint32_t> owner;
		for (uint32_t index = 0; index < this->faces.size(); index++) {
			if (!IsStone(this->faces[index].layer)) continue;
			for (uint32_t point : this->faces[index].points) {
				auto [found, fresh] = owner.try_emplace(point, index);
				if (!fresh) parent[root(index)] = root(found->second);
			}
		}
		for (uint32_t index = 0; index < this->faces.size(); index++) parent[index] = root(index);
		return parent;
	}

	void LayWays()
	{
		for (int ty = std::max(this->area.y0 - AREA_MARGIN, 0); ty <= std::min(this->area.y1 + AREA_MARGIN, this->height - 1); ty++) {
			for (int tx = std::max(this->area.x0 - AREA_MARGIN, 0); tx <= std::min(this->area.x1 + AREA_MARGIN, this->width - 1); tx++) {
				for (Track track : SetTrackBitIterator(static_cast<TrackBits>(_world_tiles.NetworkAt(TileXY(tx, ty)).track))) {
					const DrawnTrack &drawn = DrawnTrack::Of(tx, ty, track);
					this->TraceWay("rail", [&](double share) { return drawn.Centred(drawn.from + (drawn.to - drawn.from) * share); });
				}
				if (std::optional<WayCourse> course = WayCourse::OfRoad(tx, ty); course.has_value()) {
					this->TraceWay("road", [&](double share) { MapVector at = course->Centre(share); return WorldPoint{at.x, at.y, course->Level(share)}; });
				}
			}
		}
	}

	template <class PointAt>
	void TraceWay(const std::string &kind, const PointAt &point_at)
	{
		WayTrace way{kind, {}};
		for (int k = 0; k <= WAY_SAMPLES; k++) {
			WorldPoint at = point_at(static_cast<double>(k) / WAY_SAMPLES);
			way.line[k] = this->Render(at.x, at.y, at.level);
		}
		uint32_t index = static_cast<uint32_t>(this->ways.size());
		std::vector<int64_t> keys;
		for (const Vec3 &at : way.line) keys.push_back(TileKey(at.x, at.y));
		std::ranges::sort(keys);
		keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
		for (int64_t key : keys) this->way_tiles[key].push_back(index);
		this->ways.push_back(std::move(way));
	}

	/* Earth under a way's bed or deck, which the way's own model covers. */
	bool UnderWay(const Vec3 &at) const
	{
		for (int dy = -1; dy <= 1; dy++) {
			for (int dx = -1; dx <= 1; dx++) {
				auto found = this->way_tiles.find(TileKey(at.x + dx, at.y + dy));
				if (found == this->way_tiles.end()) continue;
				for (uint32_t index : found->second) {
					const WayTrace &way = this->ways[index];
					for (int k = 0; k < WAY_SAMPLES; k++) {
						const Vec3 &p = way.line[k], &q = way.line[k + 1];
						double dx2 = q.x - p.x, dy2 = q.y - p.y, length = dx2 * dx2 + dy2 * dy2;
						double share = length > 0.0 ? std::clamp(((at.x - p.x) * dx2 + (at.y - p.y) * dy2) / length, 0.0, 1.0) : 0.0;
						double across = std::hypot(at.x - (p.x + dx2 * share), at.y - (p.y + dy2 * share));
						double below = p.z + (q.z - p.z) * share - at.z;
						if (across <= DECK_HALF + WAY_COVER_MARGIN && below >= -WAY_COVER_ABOVE_LEVELS * this->rise && below <= WAY_COVER_DEPTH_LEVELS * this->rise) return true;
					}
				}
			}
		}
		return false;
	}

	void CheckWay(const WayTrace &way, std::vector<WayEnd> &ends)
	{
		const auto &line = way.line;
		const std::string &kind = way.kind;
		auto heading = [&](int k) { return MapVector{line[k + 1].x - line[k].x, line[k + 1].y - line[k].y}; };
		auto run = [&](int k) { return std::hypot(heading(k).x, heading(k).y); };
		auto grade = [&](int k) { return std::atan2(line[k + 1].z - line[k].z, run(k)); };
		for (int k = 0; k + 1 < WAY_SAMPLES; k++) {
			double span = (run(k) + run(k + 1)) * 0.5;
			if (span < POINT_QUANTUM) continue;
			MapVector h0 = heading(k), h1 = heading(k + 1);
			double turn = std::abs(std::atan2(h0.x * h1.y - h0.y * h1.x, h0.x * h1.x + h0.y * h1.y)) * RADIANS_TO_DEGREES / span;
			double crest = std::abs(grade(k + 1) - grade(k)) * RADIANS_TO_DEGREES / span;
			if (turn > WAY_BEND_DEGREES_PER_TILE) this->findings.Note(kind + "_bend", line[k + 1].x, line[k + 1].y, turn);
			if (crest > WAY_CREST_DEGREES_PER_TILE) this->findings.Note(kind + "_crest", line[k + 1].x, line[k + 1].y, crest);
		}
		auto end = [&](const Vec3 &at, const Vec3 &inner) {
			MapVector out = {at.x - inner.x, at.y - inner.y};
			double length = std::hypot(out.x, out.y);
			if (length < POINT_QUANTUM) return;
			ends.push_back({kind, at, out * (1.0 / length), std::atan2(at.z - inner.z, length)});
		};
		end(line.front(), line[1]);
		end(line.back(), line[WAY_SAMPLES - 1]);
	}

	/* Each end of a piece runs on into the piece beyond that carries on its way best; a sideways step, a turn or a change of grade there is a kink. */
	void CheckJoints(const std::vector<WayEnd> &ends)
	{
		std::unordered_map<int64_t, std::vector<uint32_t>> by_tile;
		for (uint32_t index = 0; index < ends.size(); index++) by_tile[TileKey(ends[index].at.x, ends[index].at.y)].push_back(index);
		for (uint32_t index = 0; index < ends.size(); index++) {
			const WayEnd &own = ends[index];
			double best_turn = HALF_TURN, best_grade = 0.0, step = std::numeric_limits<double>::max();
			bool joined = false;
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					auto found = by_tile.find(TileKey(own.at.x + dx, own.at.y + dy));
					if (found == by_tile.end()) continue;
					for (uint32_t other_index : found->second) {
						const WayEnd &other = ends[other_index];
						if (other_index == index || other.kind != own.kind) continue;
						double distance = Length(other.at - own.at);
						if (distance > WAY_JOINT_REACH) {
							if (distance < WAY_STEP_REACH) step = std::min(step, distance);
							continue;
						}
						joined = true;
						double turn = std::acos(std::clamp(-(own.outward.x * other.outward.x + own.outward.y * other.outward.y), -1.0, 1.0));
						if (turn < best_turn) {
							best_turn = turn;
							best_grade = std::abs(own.grade + other.grade);
						}
					}
				}
			}
			if (!joined || best_turn > QUARTER_TURN) {
				if (step < WAY_STEP_REACH) this->findings.Note(own.kind + "_step", own.at.x, own.at.y, step);
				continue;
			}
			double turn = best_turn * RADIANS_TO_DEGREES;
			if (turn > WAY_KINK_DEGREES) this->findings.Note(own.kind + "_kink", own.at.x, own.at.y, turn);
			if (best_grade * RADIANS_TO_DEGREES > WAY_GRADE_KINK_DEGREES) this->findings.Note(own.kind + "_grade_kink", own.at.x, own.at.y, best_grade * RADIANS_TO_DEGREES);
		}
	}

	Area area;
	Findings &findings;
	double rise;
	int width;
	int height;
	std::vector<Face> faces;
	std::vector<Vec3> water_points;
	std::unordered_map<PointKey, uint32_t, PointKeyHash> points;
	std::optional<FaceGrid> grid;
	std::vector<WayTrace> ways;
	std::unordered_map<int64_t, std::vector<uint32_t>> way_tiles;
};

static Area AskedArea()
{
	Area whole = {0, 0, static_cast<int>(Map::MaxX()), static_cast<int>(Map::MaxY())};
	std::optional<std::string_view> asked = GetEnv(AREA_VARIABLE);
	if (!asked.has_value()) return whole;
	std::array<int, 4> bounds;
	StringConsumer consumer(*asked);
	for (int &bound : bounds) {
		std::optional<int> value = ParseInteger<int>(consumer.ReadUntilChar(',', StringConsumer::SKIP_ONE_SEPARATOR));
		if (!value.has_value()) return whole;
		bound = *value;
	}
	return {bounds[0], bounds[1], bounds[2], bounds[3]};
}

static void Write(const std::string &path, const Findings &findings)
{
	std::ofstream file(path);
	file << "kind,x,y,value,count,detail\n";
	std::map<std::string, int> counts;
	for (const auto &[key, finding] : findings.All()) {
		const auto &[kind, x, y] = key;
		file << fmt::format("{},{},{},{:.4f},{},{}\n", kind, x, y, finding.value, finding.count, finding.detail);
		counts[kind]++;
	}
	for (const auto &[kind, tiles] : counts) fmt::print("meshcheck {}: {} tiles\n", kind, tiles);
}

void RunAskedMeshCheck()
{
	std::optional<std::string_view> output = GetEnv(OUTPUT_VARIABLE);
	if (!output.has_value() || _game_mode != GM_NORMAL || !Map::IsInitialized()) return;

	auto started = std::chrono::steady_clock::now();
	_tuning.Load();
	_world_tiles.Reset();
	_world_tiles.Sync();
	Area area = AskedArea();
	Findings findings(area);
	MeshSurvey survey(area, findings);
	survey.Build();
	survey.CheckEdges();
	survey.CheckFaces();
	survey.CheckOverlaps();
	survey.CheckWater();
	survey.CheckSlabs();
	survey.CheckWays();
	Write(std::string(*output), findings);
	double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
	fmt::print("meshcheck area {},{},{},{}: {} faces in {:.1f}s\n", area.x0, area.y0, area.x1, area.y1, survey.FaceCount(), seconds);
	std::fflush(stdout);
	std::exit(EXIT_SUCCESS);
}
