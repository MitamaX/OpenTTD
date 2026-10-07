/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file way_shapes.h The shapes rails, roads and bridges are built from: cross sections run over the map, flat outlines and ribbons, all laid on a footing. */

#ifndef MINI_WORLD_WAY_SHAPES_H
#define MINI_WORLD_WAY_SHAPES_H

#include <array>
#include <functional>
#include <span>
#include <vector>

#include "../core/camera.h"
#include "../map/way_line.h"
#include "../model/model_mesh.h"

/* Every way shape is built flat: x and y on the map in tiles, z in tiles above whatever it is laid on. */

/** How finely ways are built: in full near the eye, plainly further off where small parts would not show. */
enum class WayDetail : uint8_t {
	Full,
	Simple,
};

/* What a shape is laid on: the level, in height levels, under a point of the map. */
using Footing = std::function<double(double x, double y)>;

/* A tile's own ground, read for every point laid on the tile, and a level surface at one height. */
Footing GroundFooting(int tx, int ty);
Footing LevelFooting(double level);

/* A point of a cross section: how far right of the run's centre line it lies and how high, with the paint of the face from it to the next point.
 * Points go over the top from left to right, so every face looks outward. */
struct SectionPoint {
	double lateral;
	double height;
	uint32_t tone = MODEL_WHITE;
	double gloss = 0.0;
};

using Section = std::span<const SectionPoint>;

/* How far below its footing a way's sides reach, where the ground or an eased way's bank meets them. */
inline constexpr double WAY_FOOT = -0.012;

/* Whether a direction beyond a stretch's end is given, so the end is cut on the bisector toward it. */
inline bool Heads(const MapVector &beyond)
{
	return beyond.x != 0.0 || beyond.y != 0.0;
}

/* A box section from one lateral to another and from one height to another, closed underneath; its top takes one paint, its sides and underside another. */
std::array<SectionPoint, 5> BoxSection(double from, double to, double low, double high, uint32_t top, uint32_t sides, double gloss = 0.0);

/* A straight stretch of a way. An end is cut square unless given the direction the way runs in beyond it, coming into the first end or leaving the last,
 * when it is cut on the bisector so the two stretches meet in a mitre. */
struct Stretch final : WayLine {
	MapVector from{};
	MapVector to{};
	MapVector before{};
	MapVector after{};

	Stretch() = default;
	Stretch(const MapVector &from, const MapVector &to, const MapVector &before = {}, const MapVector &after = {}) : from(from), to(to), before(before), after(after) {}

	MapVector Along() const { return Unit(this->to - this->from); }
	MapVector Along(double) const override { return this->Along(); }
	MapVector Right() const { return RightOf(this->Along()); }
	double Span() const override { return std::hypot(this->to.x - this->from.x, this->to.y - this->from.y); }
	MapVector At(double share, double lateral) const override;
	/* Read along the cuts its ends are made on, so pieces meeting in a mitre agree along the joint. */
	double ShareOf(const MapVector &point) const override;
};

/* The section run along a line, split into rows so the footing can bend it. */
ModelMesh Laid(const WayLine &line, Section section, int rows);

/* A closed section's face at the line's first or last end. */
ModelMesh EndCap(const WayLine &line, Section section, bool last);

/* A flat plate over an outline that every point of can be seen from its centre. */
ModelMesh Plate(std::span<const MapVector> outline, const MapVector &centre, double height);

/* An upright face along a line from one height to another, facing to the right of the line. */
ModelMesh Wall(std::span<const MapVector> line, double low, double high);

/* The line moved sideways, to its right for a positive distance, its bends mitred so it keeps the distance on either side of them. */
std::vector<MapVector> Offset(std::span<const MapVector> line, double distance);

/* A thin flat band centred on a line, mitred at its bends. */
ModelMesh Ribbon(std::span<const MapVector> line, double half_width, double height);

/* A box standing on a point of the map, turned to face along a direction. */
ModelMesh Block(const MapVector &at, const MapVector &facing, const Vec3 &low, const Vec3 &high);

/* A square beam from one point to another. */
ModelMesh Strut(const Vec3 &from, const Vec3 &to, double half);

/* Points of a circular arc about a centre, turning the short way from one direction to another. */
std::vector<MapVector> Arc(const MapVector &centre, double radius, const MapVector &from, const MapVector &to, int steps);

/* Lays a flat built shape on a footing: each point is raised by the footing's level under it and each normal tips with the footing's slope there. */
ModelMesh &Drape(ModelMesh &mesh, const Footing &footing);

#endif /* MINI_WORLD_WAY_SHAPES_H */
