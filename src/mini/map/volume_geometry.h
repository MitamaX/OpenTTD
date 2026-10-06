/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file volume_geometry.h The faces of a building's solids, the order the solids stand in, and the cuts that fit faces to tiles and texture cells. */

#ifndef MINI_MAP_VOLUME_GEOMETRY_H
#define MINI_MAP_VOLUME_GEOMETRY_H

#include <algorithm>
#include <array>
#include <cmath>
#include <span>

#include "../../direction_type.h"
#include "../core/camera.h"
#include "building_form.h"

inline constexpr double RISE_SCALE = 2.2360679774997896;
inline constexpr double MODEL_TILE_LEVELS = RISE_SCALE / LEVEL_TILES;
inline constexpr size_t MAX_POLYGON_CORNERS = 32;
inline constexpr size_t RECT_CORNERS = 4;
inline constexpr double VERTICAL_NZ = 0.2;
inline constexpr double PLACE_EPS = 1e-9;

struct Vec3 {
	double x;
	double y;
	double z;
};

constexpr double Dot(const Vec3 &a, const Vec3 &b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 Normalised(const Vec3 &v);
Vec3 RenderNormal(const Vec3 &model_normal);

inline bool IsUpright(const Vec3 &unit_normal)
{
	return std::abs(unit_normal.z) < VERTICAL_NZ;
}

DiagDirection FacingSide(const Vec3 &normal);

constexpr double LevelOf(double floor, double height)
{
	return floor + height * MODEL_TILE_LEVELS;
}

constexpr double HeightOf(double floor, double level)
{
	return (level - floor) / MODEL_TILE_LEVELS;
}

struct PlanPoint {
	double x;
	double y;
};

struct PlanRect {
	double x0;
	double y0;
	double x1;
	double y1;
};

std::array<PlanPoint, RECT_CORNERS> CornersOf(const PlanRect &rect);

/* Heights are model tiles above the form's floor; a grounded point lies on the ground wherever it moves to. */
struct FacePoint {
	double x;
	double y;
	double z;
	Vec3 normal;
	double shade;
	bool grounded;
};

constexpr Vec3 PositionOf(const FacePoint &point)
{
	return {point.x, point.y, point.z};
}

inline bool SamePlace(const PlanPoint &a, const PlanPoint &b)
{
	return std::abs(a.x - b.x) < PLACE_EPS && std::abs(a.y - b.y) < PLACE_EPS;
}

inline bool SamePlace(const FacePoint &a, const FacePoint &b)
{
	return SamePlace(PlanPoint{a.x, a.y}, PlanPoint{b.x, b.y}) && std::abs(a.z - b.z) < PLACE_EPS;
}

/* A convex outline of fixed capacity that skips a corner landing where the last one did, and copies only the corners in use. */
template <typename Point>
class Outline {
public:
	Outline() = default;
	Outline(const Outline &other) { *this = other; }

	Outline &operator=(const Outline &other)
	{
		std::copy_n(other.points.begin(), other.count, this->points.begin());
		this->count = other.count;
		return *this;
	}

	void Add(const Point &point)
	{
		if (this->count > 0 && SamePlace(this->points[this->count - 1], point)) return;
		assert(this->count < this->points.size());
		this->points[this->count++] = point;
	}

	std::span<const Point> Points() const { return {this->points.data(), this->count}; }
	bool IsSurface() const { return this->count >= TRIANGLE_CORNERS; }

private:
	std::array<Point, MAX_POLYGON_CORNERS> points;
	uint8_t count = 0;
};

using FacePolygon = Outline<FacePoint>;

/* U and V run in texture cells, one cell to each whole number; a V that does not repeat stays inside a single cell. */
struct UvMap {
	Vec3 u_axis;
	double u_origin;
	Vec3 v_axis;
	double v_origin;
	bool repeats_v;

	double U(const FacePoint &point) const { return Dot(this->u_axis, PositionOf(point)) + this->u_origin; }
	double V(const FacePoint &point) const { return Dot(this->v_axis, PositionOf(point)) + this->v_origin; }
};

UvMap WallUv(const Vec3 &normal);

enum class Cladding : uint8_t { Wall, Facade, Roof, Skylight, Decal };

/* The normal is the model space unit normal pointing out of the solid; a smooth face lights each corner by its own normal, and a footed face stands in the ground. */
struct Face {
	FacePolygon polygon;
	Vec3 normal;
	UvMap uv;
	Cladding cladding;
	bool smooth;
	bool footed;
};

class FaceSink {
public:
	virtual ~FaceSink() = default;
	virtual void Take(const Face &face) = 0;
};

struct ShapeDetail {
	bool massing;
	int segments;
};

struct SolidPlacement {
	PlanRect footprint;
	double floor;
	MapVector toward;
	ShapeDetail detail;
};

/* The ground under a point of a form's plan, in model tiles above its floor, read from the form's own tile even on its outer edge. */
double GroundHeight(const PlanPoint &plan, const SolidPlacement &placement);

void BuildFaces(const Solid &solid, const SolidPlacement &placement, FaceSink &sink);
std::array<uint8_t, MAX_SOLIDS> PaintOrder(std::span<const Solid> solids, MapVector toward);

bool OwnedByNeighbour(const Face &face, const PlanRect &cell);
void ClipToPlan(FacePolygon &polygon, const PlanRect &rect);
void ClipToHeights(FacePolygon &polygon, double low, double high);
/* The hillside hides whatever lies below the ground, and the cut along it darkens like the foot of a wall. */
void ClipToGround(FacePolygon &polygon, const SolidPlacement &placement);

struct UvCell {
	int u;
	int v;
};

struct UvCells {
	int u0;
	int u1;
	int v0;
	int v1;
};

UvCells CellsOf(const FacePolygon &polygon, const UvMap &map);
void ClipToUvCell(FacePolygon &polygon, const UvMap &map, UvCell cell);

#endif /* MINI_MAP_VOLUME_GEOMETRY_H */
