/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file way_shapes.cpp The shapes rails, roads and bridges are built from: cross sections run over the map, flat outlines and ribbons, all laid on a footing. */

#include "../../stdafx.h"
#include "way_shapes.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../map/tile_shapes.h"
#include "../model/model_shapes.h"

#include "../../safeguards.h"

static constexpr Vec3 UP = {0.0, 0.0, 1.0};

static Vec3 Flat(const MapVector &v, double z = 0.0)
{
	return {v.x, v.y, z};
}

Footing GroundFooting(int tx, int ty)
{
	return [ground = TileGround(tx, ty)](double x, double y) { return ground.Level(x, y); };
}

Footing LevelFooting(double level)
{
	return [level](double, double) { return level; };
}

std::array<SectionPoint, 5> BoxSection(double from, double to, double low, double high, uint32_t top, uint32_t sides, double gloss)
{
	double left = std::min(from, to);
	double right = std::max(from, to);
	return {{{left, low, sides, gloss}, {left, high, top, gloss}, {right, high, sides, gloss}, {right, low, sides, gloss}, {left, low, sides, gloss}}};
}

MapVector Stretch::At(double share, double lateral) const
{
	return this->from + (this->to - this->from) * share + this->Right() * lateral;
}

double Stretch::ShareOf(const MapVector &point) const
{
	MapVector run = this->to - this->from;
	return Dot(point - this->from, run) / Dot(run, run);
}

ModelMesh EndCap(const WayLine &line, Section section, bool last)
{
	double share = last ? 1.0 : 0.0;
	Vec3 facing = Flat(line.Along(share) * (last ? 1.0 : -1.0));
	ModelMesh cap;
	for (const SectionPoint &point : section) cap.Point(Flat(line.At(share, point.lateral), point.height), facing);
	for (uint32_t corner = 1; corner + 1 < section.size(); corner++) cap.Triangle(0, corner, corner + 1);
	return cap.Paint(section.front().tone).Gloss(section.front().gloss);
}

ModelMesh Laid(const WayLine &line, Section section, int rows)
{
	ModelMesh mesh;
	for (size_t point = 0; point + 1 < section.size(); point++) {
		const SectionPoint &low = section[point];
		const SectionPoint &high = section[point + 1];
		ModelMesh face;
		for (int row = 0; row <= rows; row++) {
			double share = static_cast<double>(row) / rows;
			Vec3 normal = Flat(RightOf(line.Along(share)) * (low.height - high.height), high.lateral - low.lateral);
			face.Point(Flat(line.At(share, low.lateral), low.height), normal);
			face.Point(Flat(line.At(share, high.lateral), high.height), normal);
			if (row > 0) face.Quad(2 * row - 2, 2 * row - 1, 2 * row + 1, 2 * row);
		}
		mesh.Append(face.Paint(low.tone).Gloss(low.gloss));
	}
	return mesh;
}

ModelMesh Plate(std::span<const MapVector> outline, const MapVector &centre, double height)
{
	ModelMesh mesh;
	uint32_t middle = mesh.Point(Flat(centre, height), UP);
	for (const MapVector &corner : outline) mesh.Point(Flat(corner, height), UP);
	uint32_t count = static_cast<uint32_t>(outline.size());
	for (uint32_t corner = 0; corner < count; corner++) mesh.Triangle(middle, middle + 1 + corner, middle + 1 + (corner + 1) % count);
	return mesh;
}

ModelMesh Wall(std::span<const MapVector> line, double low, double high)
{
	ModelMesh mesh;
	for (size_t point = 0; point + 1 < line.size(); point++) {
		Vec3 facing = Flat(RightOf(Unit(line[point + 1] - line[point])));
		uint32_t a = mesh.Point(Flat(line[point], low), facing);
		uint32_t b = mesh.Point(Flat(line[point + 1], low), facing);
		uint32_t c = mesh.Point(Flat(line[point + 1], high), facing);
		uint32_t d = mesh.Point(Flat(line[point], high), facing);
		mesh.Quad(a, b, c, d);
	}
	return mesh;
}

std::vector<MapVector> Offset(std::span<const MapVector> line, double distance)
{
	std::vector<MapVector> moved;
	for (size_t point = 0; point < line.size(); point++) {
		MapVector out = Unit(line[std::min(point + 1, line.size() - 1)] - line[point]);
		MapVector into = point == 0 ? out : Unit(line[point] - line[point - 1]);
		if (point + 1 == line.size()) out = into;
		MapVector right = RightOf(into);
		MapVector cut = RightOf(Unit(into + out));
		moved.push_back(line[point] + cut * (distance / Dot(cut, right)));
	}
	return moved;
}

ModelMesh Ribbon(std::span<const MapVector> line, double half_width, double height)
{
	std::vector<MapVector> right = Offset(line, half_width);
	std::vector<MapVector> left = Offset(line, -half_width);
	ModelMesh mesh;
	for (size_t point = 0; point < line.size(); point++) {
		mesh.Point(Flat(left[point], height), UP);
		mesh.Point(Flat(right[point], height), UP);
		uint32_t row = static_cast<uint32_t>(point);
		if (row > 0) mesh.Quad(2 * row - 2, 2 * row - 1, 2 * row + 1, 2 * row);
	}
	return mesh;
}

ModelMesh Block(const MapVector &at, const MapVector &facing, const Vec3 &low, const Vec3 &high)
{
	return Box(low, high).Transform(Mat4::Translation(Flat(at)) * Mat4::Turn(UP, std::atan2(facing.y, facing.x)));
}

ModelMesh Strut(const Vec3 &from, const Vec3 &to, double half)
{
	Vec3 axis = Normalised(to - from);
	Vec3 side = Normalised(Cross(std::abs(axis.z) < 0.9 ? UP : Vec3{1.0, 0.0, 0.0}, axis));
	Vec3 lift = Cross(axis, side);
	Mat4 frame = Mat4::Identity();
	auto column = [&frame](int index, const Vec3 &v) {
		frame.At(0, index) = v.x;
		frame.At(1, index) = v.y;
		frame.At(2, index) = v.z;
	};
	column(0, side);
	column(1, lift);
	column(2, axis);
	column(3, from);
	return Box({-half, -half, 0.0}, {half, half, Length(to - from)}).Transform(frame);
}

std::vector<MapVector> Arc(const MapVector &centre, double radius, const MapVector &from, const MapVector &to, int steps)
{
	double start = std::atan2(from.y, from.x);
	double turn = std::remainder(std::atan2(to.y, to.x) - start, 2.0 * std::numbers::pi);
	std::vector<MapVector> points;
	for (int step = 0; step <= steps; step++) {
		double angle = start + turn * step / steps;
		points.push_back(centre + MapVector{std::cos(angle), std::sin(angle)} * radius);
	}
	return points;
}

ModelMesh &Drape(ModelMesh &mesh, const Footing &footing)
{
	static constexpr double PROBE = 1.0e-3;
	double rise = LevelRise();
	for (ModelVertex &vertex : mesh.vertices) {
		Vec3 at = vertex.Position();
		Vec3 normal = vertex.Normal();
		double east = (footing(at.x + PROBE, at.y) - footing(at.x - PROBE, at.y)) * rise / (2.0 * PROBE);
		double south = (footing(at.x, at.y + PROBE) - footing(at.x, at.y - PROBE)) * rise / (2.0 * PROBE);
		vertex.Place({at.x, at.y, at.z + footing(at.x, at.y) * rise});
		vertex.Face({normal.x - east * normal.z, normal.y - south * normal.z, normal.z});
	}
	return mesh;
}
