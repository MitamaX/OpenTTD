/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file shore_relief.h The ground of bare coast tiles curving smoothly through the corners around them, so shores run on from tile to tile instead of in saw teeth. */

#ifndef MINI_WORLD_SHORE_RELIEF_H
#define MINI_WORLD_SHORE_RELIEF_H

#include <array>

#include "../core/camera.h"
#include "seabed.h"

/* A coast tile with nothing standing on it, whose ground may curve without lifting anything off it. */
bool IsBareCoast(int tx, int ty);

/* The curved ground of one bare coast tile: a spline through the bed's corners, straightened toward every tile whose ground keeps its facets,
 * so the two meet edge to edge and corner to corner. */
class ShoreRelief {
public:
	ShoreRelief(const Seabed &bed, int tx, int ty, const std::array<double, 4> &corners);

	double Level(double x, double y) const;
	Vec3 Normal(double x, double y) const;

private:
	double Curved(double x, double y) const;
	double Faceted(double x, double y) const;
	double Freedom(double x, double y) const;

	const Seabed &bed;
	int tx;
	int ty;
	std::array<double, 4> corners;
};

#endif /* MINI_WORLD_SHORE_RELIEF_H */
