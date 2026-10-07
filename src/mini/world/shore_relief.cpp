/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file shore_relief.cpp The ground of bare coast tiles curving smoothly through the corners around them, so shores run on from tile to tile instead of in saw teeth. */

#include "../../stdafx.h"
#include "shore_relief.h"

#include <algorithm>
#include <cmath>

#include "../../map_func.h"
#include "../map/tile_shapes.h"
#include "../map/world_tiles.h"

#include "../../safeguards.h"

static constexpr double STRAIGHTENING_REACH = 0.5;
static constexpr double SLOPE_PROBE = 0.1;
static constexpr int SPLINE_REACH = 1;

bool IsBareCoast(int tx, int ty)
{
	if (!OnMap(tx, ty) || WaterFormOf(tx, ty) != WaterForm::Shore) return false;
	TileIndex tile = TileXY(tx, ty);
	GroundTexel ground = _world_tiles.GroundAt(tile);
	NetworkTexel network = _world_tiles.NetworkAt(tile);
	return ground.material == GroundMaterial::Shore && ground.flora == 0 && network.track == 0 && network.road == 0 && network.tram == 0;
}

/* The cubic through p1 and p2 whose slopes there point from the points either side. */
static double CatmullRom(double p0, double p1, double p2, double p3, double t)
{
	return p1 + 0.5 * t * (p2 - p0 + t * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3 + t * (3.0 * (p1 - p2) + p3 - p0)));
}

/* How far a point lies from a tile's square. */
static double DistanceToTile(double x, double y, int tx, int ty)
{
	double dx = std::max({tx - x, 0.0, x - (tx + 1)});
	double dy = std::max({ty - y, 0.0, y - (ty + 1)});
	return std::hypot(dx, dy);
}

ShoreRelief::ShoreRelief(const Seabed &bed, int tx, int ty, const std::array<double, 4> &corners) : bed(bed), tx(tx), ty(ty), corners(corners)
{
}

double ShoreRelief::Level(double x, double y) const
{
	return std::lerp(this->Faceted(x, y), this->Curved(x, y), this->Freedom(x, y));
}

Vec3 ShoreRelief::Normal(double x, double y) const
{
	double rise = LevelRise();
	double slope_x = (this->Level(x + SLOPE_PROBE, y) - this->Level(x - SLOPE_PROBE, y)) * rise / (2.0 * SLOPE_PROBE);
	double slope_y = (this->Level(x, y + SLOPE_PROBE) - this->Level(x, y - SLOPE_PROBE)) * rise / (2.0 * SLOPE_PROBE);
	return Normalised({-slope_x, -slope_y, 1.0});
}

/* The spline runs through every corner of the bed, so it agrees with the tile's own corners wherever it is free to show. */
double ShoreRelief::Curved(double x, double y) const
{
	int cx = static_cast<int>(std::floor(x));
	int cy = static_cast<int>(std::floor(y));
	std::array<double, 4> rows;
	for (int j = 0; j < 4; j++) {
		auto at = [&](int i) { return this->bed.Level(2 * (cx + i - 1), 2 * (cy + j - 1)); };
		rows[j] = CatmullRom(at(0), at(1), at(2), at(3), x - cx);
	}
	return CatmullRom(rows[0], rows[1], rows[2], rows[3], y - cy);
}

/* The tile's corners spread over it, straight along each side as every neighbour's edge runs. Corners go north, west, east, south. */
double ShoreRelief::Faceted(double x, double y) const
{
	double fx = x - this->tx;
	double fy = y - this->ty;
	double north = std::lerp(this->corners[0], this->corners[1], fx);
	double south = std::lerp(this->corners[2], this->corners[3], fx);
	return std::lerp(north, south, fy);
}

/* How far the ground may curve at a point: not at all on the edge of a tile that keeps its facets, and freely half a tile away from every one. */
double ShoreRelief::Freedom(double x, double y) const
{
	int cx = static_cast<int>(std::floor(x));
	int cy = static_cast<int>(std::floor(y));
	double freedom = 1.0;
	for (int ny = cy - SPLINE_REACH; ny <= cy + SPLINE_REACH; ny++) {
		for (int nx = cx - SPLINE_REACH; nx <= cx + SPLINE_REACH; nx++) {
			if (IsBareCoast(nx, ny)) continue;
			freedom = std::min(freedom, SmoothStep(0.0, STRAIGHTENING_REACH, DistanceToTile(x, y, nx, ny)));
		}
	}
	return freedom;
}
