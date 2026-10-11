/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tile_relief.cpp The ground of a tile curving smoothly between its corners, and the cubic weighing of the corners about it that curves it. */

#include "../../stdafx.h"
#include "tile_relief.h"

#include <algorithm>

#include "../core/space.h"

#include "../../safeguards.h"

static constexpr double SLOPE_PROBE = 0.1;
static constexpr double LEAST_UPRIGHT = 0.05;
static constexpr int SPLINE_REACH = 1;

LatticeCell CellOf(double x, double y)
{
	int cx = static_cast<int>(std::floor(x));
	int cy = static_cast<int>(std::floor(y));
	return {cx, cy, x - cx, y - cy};
}

CubicWeights CatmullRomWeights(double t)
{
	double t2 = t * t;
	double t3 = t2 * t;
	return {0.5 * (2.0 * t2 - t - t3), 1.0 + 0.5 * (3.0 * t3 - 5.0 * t2), 0.5 * (t + 4.0 * t2 - 3.0 * t3), 0.5 * (t3 - t2)};
}

CubicWeights BsplineWeights(double t)
{
	double s = 1.0 - t;
	double t2 = t * t;
	double t3 = t2 * t;
	return {s * s * s / 6.0, (4.0 - 6.0 * t2 + 3.0 * t3) / 6.0, (1.0 + 3.0 * t + 3.0 * t2 - 3.0 * t3) / 6.0, t3 / 6.0};
}

CubicWeights BsplineCurvatures(double t)
{
	return {1.0 - t, 3.0 * t - 2.0, 1.0 - 3.0 * t, t};
}

double Weighed(const CornerPatch &patch, const CubicWeights &across, const CubicWeights &down)
{
	double sum = 0.0;
	for (int j = 0; j < 4; j++) {
		double row = 0.0;
		for (int i = 0; i < 4; i++) row += across[i] * patch[j][i];
		sum += down[j] * row;
	}
	return sum;
}

Vec3 ClimbNormal(const MapVector &climb)
{
	double rise = LevelRise();
	return Normalised(Vec3{-climb.x * rise, -climb.y * rise, 1.0});
}

/* How far a point lies from a tile's square. */
static double DistanceToTile(double x, double y, int tx, int ty)
{
	double dx = std::max({tx - x, 0.0, x - (tx + 1)});
	double dy = std::max({ty - y, 0.0, y - (ty + 1)});
	return std::hypot(dx, dy);
}

TileRelief::TileRelief(int tx, int ty, const std::array<double, 4> &corners, double straightening_reach) : tx(tx), ty(ty), corners(corners), straightening_reach(straightening_reach)
{
}

double TileRelief::Level(double x, double y) const
{
	return std::lerp(this->Faceted(x, y), this->Curved(x, y), this->Freedom(x, y));
}

Vec3 TileRelief::Normal(double x, double y, const Vec3 &faceted) const
{
	MapVector bend = ClimbOf([this](double px, double py) { return this->Level(px, py) - this->Faceted(px, py); }, x, y, SLOPE_PROBE) * LevelRise();
	double upright = std::max(faceted.z, LEAST_UPRIGHT);
	return Normalised({faceted.x / upright - bend.x, faceted.y / upright - bend.y, 1.0});
}

/* The tile's corners spread over it, straight along each side as every neighbour's edge runs. Corners go north, west, east, south. */
double TileRelief::Faceted(double x, double y) const
{
	if (!this->held.has_value()) {
		std::array<double, 4> held;
		for (size_t i = 0; i < held.size(); i++) {
			double cx = this->tx + static_cast<double>(i % 2);
			double cy = this->ty + static_cast<double>(i / 2);
			held[i] = this->Pinned(cx, cy) ? this->corners[i] : this->Held(cx, cy, this->corners[i]);
		}
		this->held = held;
	}
	const std::array<double, 4> &corner = *this->held;
	double fx = x - this->tx;
	double fy = y - this->ty;
	double north = std::lerp(corner[0], corner[1], fx);
	double south = std::lerp(corner[2], corner[3], fx);
	return std::lerp(north, south, fy);
}

/* How far the ground may curve at a point: not at all on the edge of a tile that keeps its facets, and freely from the straightening reach away from every one. */
double TileRelief::Freedom(double x, double y) const
{
	int cx = static_cast<int>(std::floor(x));
	int cy = static_cast<int>(std::floor(y));
	double freedom = 1.0;
	for (int ny = cy - SPLINE_REACH; ny <= cy + SPLINE_REACH; ny++) {
		for (int nx = cx - SPLINE_REACH; nx <= cx + SPLINE_REACH; nx++) {
			if (this->Curving(nx, ny)) continue;
			freedom = std::min(freedom, SmoothStep(0.0, this->straightening_reach, DistanceToTile(x, y, nx, ny)));
		}
	}
	return freedom;
}

/* Whether a tile near this one curves, asked of each tile once. */
bool TileRelief::Curving(int nx, int ny) const
{
	if (!this->curving.has_value()) {
		std::array<bool, CURVING_SIDE * CURVING_SIDE> curving;
		for (int j = 0; j < CURVING_SIDE; j++) {
			for (int i = 0; i < CURVING_SIDE; i++) curving[j * CURVING_SIDE + i] = this->Curves(this->tx + i - CURVING_REACH, this->ty + j - CURVING_REACH);
		}
		this->curving = curving;
	}
	return (*this->curving)[(ny - this->ty + CURVING_REACH) * CURVING_SIDE + nx - this->tx + CURVING_REACH];
}
