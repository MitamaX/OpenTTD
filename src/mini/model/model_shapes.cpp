/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file model_shapes.cpp The plain solids models are put together from, each built welded about its own origin. */

#include "../../stdafx.h"
#include "model_shapes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numbers>
#include <utility>
#include <vector>

#include "../../safeguards.h"

static constexpr Vec3 DOWN = {0.0, 0.0, -1.0};
static constexpr Vec3 UP = {0.0, 0.0, 1.0};

static Vec3 RingPoint(double radius, double z, int side, int sides)
{
	double angle = 2.0 * std::numbers::pi * side / sides;
	return {radius * std::cos(angle), radius * std::sin(angle), z};
}

/* A ring's corners, or its one point when it has no radius. */
static std::vector<uint32_t> AddRing(ModelMesh &mesh, const LatheRing &ring, int sides)
{
	if (ring.radius <= 0.0) return {mesh.Point({0.0, 0.0, ring.z})};
	std::vector<uint32_t> corners;
	for (int side = 0; side < sides; side++) corners.push_back(mesh.Point(RingPoint(ring.radius, ring.z, side, sides)));
	return corners;
}

static void JoinRings(ModelMesh &mesh, const std::vector<uint32_t> &low, const std::vector<uint32_t> &high, int sides)
{
	for (int side = 0; side < sides; side++) {
		int next = (side + 1) % sides;
		if (low.size() == 1) {
			mesh.Triangle(low[0], high[next], high[side]);
		} else if (high.size() == 1) {
			mesh.Triangle(low[side], low[next], high[0]);
		} else {
			mesh.Quad(low[side], low[next], high[next], high[side]);
		}
	}
}

/* A flat cap of corners of its own, so the rim stays sharp however the mesh is shaded. */
static void Cap(ModelMesh &mesh, const LatheRing &ring, int sides, const Vec3 &facing)
{
	uint32_t centre = mesh.Point({0.0, 0.0, ring.z}, facing);
	std::vector<uint32_t> rim;
	for (int side = 0; side < sides; side++) rim.push_back(mesh.Point(RingPoint(ring.radius, ring.z, side, sides), facing));
	for (int side = 0; side < sides; side++) {
		int next = (side + 1) % sides;
		if (facing.z > 0.0) {
			mesh.Triangle(centre, rim[side], rim[next]);
		} else {
			mesh.Triangle(centre, rim[next], rim[side]);
		}
	}
}

ModelMesh Lathe(std::span<const LatheRing> profile, int sides)
{
	ModelMesh mesh;
	std::vector<uint32_t> below = AddRing(mesh, profile.front(), sides);
	for (size_t ring = 1; ring < profile.size(); ring++) {
		std::vector<uint32_t> above = AddRing(mesh, profile[ring], sides);
		JoinRings(mesh, below, above, sides);
		below = std::move(above);
	}
	mesh.Smooth();
	if (profile.front().radius > 0.0) Cap(mesh, profile.front(), sides, DOWN);
	if (profile.back().radius > 0.0) Cap(mesh, profile.back(), sides, UP);
	return mesh;
}

ModelMesh Column(int sides, double low_radius, double high_radius, double height)
{
	std::array<LatheRing, 2> profile = {{{low_radius, 0.0}, {high_radius, height}}};
	return Lathe(profile, sides);
}

ModelMesh Prism(int sides, double radius, double height)
{
	return Column(sides, radius, radius, height);
}

ModelMesh Cone(int sides, double radius, double height)
{
	return Column(sides, radius, 0.0, height);
}

/* The midpoint of an edge, made once however many faces share the edge. */
class EdgeSplitter {
public:
	explicit EdgeSplitter(ModelMesh &mesh) : mesh(mesh) {}

	uint32_t Middle(uint32_t a, uint32_t b)
	{
		std::pair<uint32_t, uint32_t> edge = std::minmax(a, b);
		auto found = this->middles.find(edge);
		if (found != this->middles.end()) return found->second;
		Vec3 middle = Normalised(this->mesh.vertices[a].Position() + this->mesh.vertices[b].Position());
		uint32_t index = this->mesh.Point(middle, middle);
		this->middles.emplace(edge, index);
		return index;
	}

private:
	ModelMesh &mesh;
	std::map<std::pair<uint32_t, uint32_t>, uint32_t> middles;
};

ModelMesh Icosphere(int subdivisions)
{
	const double t = std::numbers::phi;
	const std::array<Vec3, 12> corners = {{
		{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0},
		{0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t},
		{t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1},
	}};
	static constexpr std::array<std::array<uint32_t, 3>, 20> FACES = {{
		{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
		{1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
		{3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
		{4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1},
	}};

	ModelMesh mesh;
	for (const Vec3 &corner : corners) {
		Vec3 unit = Normalised(corner);
		mesh.Point(unit, unit);
	}
	for (const auto &face : FACES) mesh.Triangle(face[0], face[1], face[2]);

	for (int level = 0; level < subdivisions; level++) {
		EdgeSplitter splitter(mesh);
		std::vector<uint32_t> faces = std::move(mesh.indices);
		mesh.indices.clear();
		for (size_t first = 0; first + 2 < faces.size(); first += 3) {
			uint32_t a = faces[first];
			uint32_t b = faces[first + 1];
			uint32_t c = faces[first + 2];
			uint32_t ab = splitter.Middle(a, b);
			uint32_t bc = splitter.Middle(b, c);
			uint32_t ca = splitter.Middle(c, a);
			mesh.Triangle(a, ab, ca);
			mesh.Triangle(b, bc, ab);
			mesh.Triangle(c, ca, bc);
			mesh.Triangle(ab, bc, ca);
		}
	}
	return mesh;
}

ModelMesh Box(const Vec3 &low, const Vec3 &high)
{
	std::array<MapVector, 4> outline = {{{low.x, low.y}, {high.x, low.y}, {high.x, high.y}, {low.x, high.y}}};
	return Extrusion(outline, high.z - low.z).Transform(Mat4::Translation({0.0, 0.0, low.z}));
}

ModelMesh Extrusion(std::span<const MapVector> outline, double height)
{
	ModelMesh mesh;
	size_t count = outline.size();
	for (size_t corner = 0; corner < count; corner++) {
		const MapVector &from = outline[corner];
		const MapVector &to = outline[(corner + 1) % count];
		Vec3 outward = Normalised({to.y - from.y, from.x - to.x, 0.0});
		uint32_t a = mesh.Point({from.x, from.y, 0.0}, outward);
		uint32_t b = mesh.Point({to.x, to.y, 0.0}, outward);
		uint32_t c = mesh.Point({to.x, to.y, height}, outward);
		uint32_t d = mesh.Point({from.x, from.y, height}, outward);
		mesh.Quad(a, b, c, d);
	}
	for (double z : {0.0, height}) {
		const Vec3 &facing = z > 0.0 ? UP : DOWN;
		uint32_t first = static_cast<uint32_t>(mesh.vertices.size());
		for (const MapVector &corner : outline) mesh.Point({corner.x, corner.y, z}, facing);
		for (uint32_t corner = 1; corner + 1 < count; corner++) {
			if (z > 0.0) {
				mesh.Triangle(first, first + corner, first + corner + 1);
			} else {
				mesh.Triangle(first, first + corner + 1, first + corner);
			}
		}
	}
	return mesh;
}

std::vector<MapVector> Circle(double radius, int sides)
{
	std::vector<MapVector> corners;
	for (int side = 0; side < sides; side++) {
		Vec3 at = RingPoint(radius, 0.0, side, sides);
		corners.push_back({at.x, at.y});
	}
	return corners;
}

/* The path's direction at a point, from its neighbours on either side. */
static Vec3 PathTangent(std::span<const SweepPoint> path, size_t point)
{
	size_t before = point == 0 ? 0 : point - 1;
	size_t after = std::min(point + 1, path.size() - 1);
	return Normalised(path[after].at - path[before].at);
}

ModelMesh Sweep(std::span<const MapVector> section, bool closed, std::span<const SweepPoint> path, const Vec3 &side)
{
	ModelMesh mesh;
	uint32_t width = static_cast<uint32_t>(section.size());
	for (size_t point = 0; point < path.size(); point++) {
		Vec3 tangent = PathTangent(path, point);
		Vec3 across = Normalised(side - tangent * Dot(side, tangent));
		Vec3 up = Cross(tangent, across);
		for (const MapVector &corner : section) mesh.Point(path[point].at + (across * corner.x + up * corner.y) * path[point].scale);
	}
	uint32_t spans = closed ? width : width - 1;
	for (uint32_t ring = 0; ring + 1 < path.size(); ring++) {
		uint32_t here = ring * width;
		uint32_t next = here + width;
		for (uint32_t corner = 0; corner < spans; corner++) {
			uint32_t following = (corner + 1) % width;
			mesh.Quad(here + corner, here + following, next + following, next + corner);
		}
	}
	return mesh.Smooth();
}
