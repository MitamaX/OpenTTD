/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file model_shapes.h The plain solids models are put together from, each built welded about its own origin. */

#ifndef MINI_MODEL_MODEL_SHAPES_H
#define MINI_MODEL_MODEL_SHAPES_H

#include <span>
#include <vector>

#include "../core/camera.h"
#include "model_mesh.h"

/* One ring of a turned solid: its radius and its height on the upright axis. */
struct LatheRing {
	double radius;
	double z;
};

/* One point of a path a cross section is swept along, and how much the section is scaled there. */
struct SweepPoint {
	Vec3 at;
	double scale;
};

/* Rings from the bottom up, turned about the upright axis; an end ring of no radius closes in a point, any other end is capped flat. */
ModelMesh Lathe(std::span<const LatheRing> profile, int sides);
ModelMesh Column(int sides, double low_radius, double high_radius, double height);
ModelMesh Prism(int sides, double radius, double height);
ModelMesh Cone(int sides, double radius, double height);

/* A sphere of unit radius, from an icosahedron whose faces are split into four this many times over. */
ModelMesh Icosphere(int subdivisions);
ModelMesh Box(const Vec3 &low, const Vec3 &high);

/* A convex outline on the ground, counterclockwise from above, raised to a height and capped. */
ModelMesh Extrusion(std::span<const MapVector> outline, double height);

/* A regular polygon's corners about the origin, counterclockwise from the positive x axis, to sweep as a round section. */
std::vector<MapVector> Circle(double radius, int sides);

/* A cross section swept along a path: across runs square to the path toward the side vector, and up runs square to both.
 * A closed section wraps around; an open one leaves a strip, which with a section of two points is a flat ribbon. */
ModelMesh Sweep(std::span<const MapVector> section, bool closed, std::span<const SweepPoint> path, const Vec3 &side);

#endif /* MINI_MODEL_MODEL_SHAPES_H */
