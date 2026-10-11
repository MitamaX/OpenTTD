/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tile_relief.h The ground of a tile curving smoothly between its corners, and the cubic weighing of the corners about it that curves it. */

#ifndef MINI_WORLD_TILE_RELIEF_H
#define MINI_WORLD_TILE_RELIEF_H

#include <array>
#include <cmath>
#include <optional>

#include "../core/camera.h"

inline constexpr double CLIMB_PROBE = 0.05;
inline constexpr double SLOPE_PROBE = 0.1;

using CubicWeights = std::array<double, 4>;
using CornerPatch = std::array<std::array<double, 4>, 4>;

struct LatticeCell {
	int cx;
	int cy;
	double u;
	double v;
};

LatticeCell CellOf(double x, double y);
CubicWeights CatmullRomWeights(double t);
CubicWeights BsplineWeights(double t);
CubicWeights BsplineCurvatures(double t);
double Weighed(const CornerPatch &patch, const CubicWeights &across, const CubicWeights &down);

template <class Corner>
CornerPatch PatchAbout(const Corner &corner, const LatticeCell &cell)
{
	CornerPatch patch;
	for (int j = 0; j < 4; j++) {
		for (int i = 0; i < 4; i++) patch[j][i] = corner(cell.cx + i - 1, cell.cy + j - 1);
	}
	return patch;
}

template <class Corner>
double Bicubic(const Corner &corner, const LatticeCell &cell, const CubicWeights &across, const CubicWeights &down)
{
	return Weighed(PatchAbout(corner, cell), across, down);
}

template <class Level>
MapVector ClimbOf(const Level &level, double x, double y, double probe = CLIMB_PROBE)
{
	double east = level(x + probe, y) - level(x - probe, y);
	double south = level(x, y + probe) - level(x, y - probe);
	return MapVector{east, south} * (0.5 / probe);
}

Vec3 ClimbNormal(const MapVector &climb);

/* The curved ground of one tile, straightened toward every tile whose ground keeps its facets, so the two meet edge to edge and corner to corner. */
class TileRelief {
public:
	TileRelief(int tx, int ty, const std::array<double, 4> &corners, double straightening_reach);
	virtual ~TileRelief() = default;

	double Level(double x, double y) const;
	virtual Vec3 Normal(double x, double y, const Vec3 &faceted) const;
	virtual bool HoldsWater() const { return true; }
	virtual bool OnBankFace(double, double) const { return false; }
	/* Whether the ground at a point is held to the tile's facets by a neighbour that keeps them. */
	bool Pinned(double x, double y) const { return this->Freedom(x, y) <= 0.0 && this->Lift(x, y, this->Faceted(x, y)) <= 0.0; }

protected:
	virtual double Curved(double x, double y) const = 0;
	virtual bool Curves(int nx, int ny) const = 0;
	/* The level the tile's facets are held at about a corner where no neighbour keeps its facets. */
	virtual double Held(double x, double y, double corner) const = 0;
	virtual double Lift(double, double, double) const { return 0.0; }
	double Ground(double x, double y) const;
	double Freedom(double x, double y) const;
	MapVector LevelClimb(double x, double y) const;
	Vec3 BentNormal(const MapVector &level_climb, double x, double y, const Vec3 &faceted) const;

	int tx;
	int ty;

private:
	/* The tiles about this one whose ground curves are looked up this far either side of it. */
	static constexpr int CURVING_REACH = 2;
	static constexpr int CURVING_SIDE = 2 * CURVING_REACH + 1;

	const std::array<double, 4> &HeldCorners() const;
	double Faceted(double x, double y) const;
	MapVector FacetedClimb(double x, double y) const;
	bool Curving(int nx, int ny) const;

	std::array<double, 4> corners;
	double straightening_reach;
	mutable std::optional<std::array<double, 4>> held;
	mutable std::optional<std::array<bool, CURVING_SIDE * CURVING_SIDE>> curving;
};

#endif /* MINI_WORLD_TILE_RELIEF_H */
