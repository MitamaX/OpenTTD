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
#include <cmath>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

#include "../../core/enum_type.hpp"
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

enum class WayBed : uint8_t {
	Plain,
	Ballast,
	Shoulder,
	Asphalt,
	Marking,
	Paving,
	End,
};

inline constexpr std::array<std::string_view, to_underlying(WayBed::End)> WAY_BED_NAMES = {"PLAIN", "BALLAST", "SHOULDER", "ASPHALT", "MARKING", "PAVING"};

inline ModelMesh &Bedded(ModelMesh &mesh, WayBed bed)
{
	return mesh.Coded(to_underlying(bed));
}

/* A point of a cross section: how far right of the run's centre line it lies and how high, with the paint, gloss and bed of the face from it to the next point.
 * Points go over the top from left to right, so every face looks outward. A face laid as a bed is matte. */
struct SectionPoint {
	double lateral;
	double height;
	uint32_t tone = MODEL_WHITE;
	double gloss = 0.0;
	WayBed bed = WayBed::Plain;
};

using Section = std::span<const SectionPoint>;

template <size_t N>
constexpr std::array<SectionPoint, N> Shifted(const std::array<SectionPoint, N> &section, double lateral)
{
	std::array<SectionPoint, N> shifted = section;
	for (SectionPoint &point : shifted) point.lateral += lateral;
	return shifted;
}

/* How far below its footing a way's sides reach, where the ground or an eased way's bank meets them. */
inline constexpr double WAY_FOOT = -0.012;

/* A box section from one lateral to another and from one height to another, closed underneath; its top takes one paint, its sides and underside another. */
std::array<SectionPoint, 5> BoxSection(double from, double to, double low, double high, uint32_t top, uint32_t sides, double gloss = 0.0);

/* A straight stretch of a way, its ends cut square. */
struct Stretch final : WayLine {
	MapVector from{};
	MapVector to{};

	Stretch() = default;
	Stretch(const MapVector &from, const MapVector &to) : from(from), to(to) {}

	MapVector Along() const { return Unit(this->to - this->from); }
	MapVector Along(double) const override { return this->Along(); }
	MapVector Right() const { return RightOf(this->Along()); }
	double Span() const override { return Length(this->to - this->from); }
	MapVector At(double share, double lateral) const override;
	double ShareOf(const MapVector &point) const override;
};

struct Portion final : WayLine {
	const WayLine &line;
	double from;
	double to;

	Portion(const WayLine &line, double from, double to) : line(line), from(from), to(to) {}

	MapVector At(double share, double lateral) const override { return this->line.At(this->Inner(share), lateral); }
	MapVector Along(double share) const override { return this->line.Along(this->Inner(share)); }
	double Span() const override { return this->line.Span() * (this->to - this->from); }
	double ShareOf(const MapVector &point) const override { return (this->line.ShareOf(point) - this->from) / (this->to - this->from); }

private:
	double Inner(double share) const { return std::lerp(this->from, this->to, share); }
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
