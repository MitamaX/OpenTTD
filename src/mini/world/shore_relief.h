/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file shore_relief.h The ground along shores curving smoothly from tile to tile instead of in saw teeth: bare coasts through the corners around them, river beds along the water's outline. */

#ifndef MINI_WORLD_SHORE_RELIEF_H
#define MINI_WORLD_SHORE_RELIEF_H

#include <array>

#include <optional>

#include "../core/camera.h"
#include "../map/tile_shapes.h"
#include "seabed.h"

/* A coast tile with nothing standing on it or spanning it, whose ground may curve without lifting anything off it. */
bool IsBareCoast(int tx, int ty);

/* The curved ground of one tile, straightened toward every tile whose ground keeps its facets, so the two meet edge to edge and corner to corner. */
class TileRelief {
public:
	TileRelief(int tx, int ty, const std::array<double, 4> &corners, double straightening_reach);
	virtual ~TileRelief() = default;

	double Level(double x, double y) const;
	Vec3 Normal(double x, double y) const;
	/* Whether the ground at a point is held to the tile's facets by a neighbour that keeps them. */
	bool Pinned(double x, double y) const { return this->Freedom(x, y) <= 0.0; }

protected:
	virtual double Curved(double x, double y) const = 0;
	virtual bool Curves(int nx, int ny) const = 0;
	/* The level the tile's facets are held at about a corner where no neighbour keeps its facets. */
	virtual double Held(double x, double y, double corner) const = 0;

	int tx;
	int ty;

private:
	/* The tiles about this one whose ground curves are looked up this far either side of it. */
	static constexpr int CURVING_REACH = 2;
	static constexpr int CURVING_SIDE = 2 * CURVING_REACH + 1;

	double Faceted(double x, double y) const;
	double Freedom(double x, double y) const;
	bool Curving(int nx, int ny) const;

	std::array<double, 4> corners;
	double straightening_reach;
	mutable std::optional<std::array<double, 4>> held;
	mutable std::optional<std::array<bool, CURVING_SIDE * CURVING_SIDE>> curving;
};

/* A bare coast's ground: a spline through the bed's corners. */
class ShoreRelief : public TileRelief {
public:
	ShoreRelief(const Seabed &bed, int tx, int ty, const std::array<double, 4> &corners);

private:
	double Curved(double x, double y) const override;
	bool Curves(int nx, int ny) const override { return IsBareCoast(nx, ny); }
	double Held(double, double, double corner) const override { return corner; }

	const Seabed &bed;
};

/* The ground of a river and its bare banks: its own facets out to the water's curved outline, and falling away inside it to the depth the water has there. */
class ChannelRelief : public TileRelief {
public:
	ChannelRelief(const Seabed &bed, int tx, int ty, const std::array<double, 4> &corners);

private:
	double Curved(double x, double y) const override;
	bool Curves(int nx, int ny) const override;
	double Held(double x, double y, double) const override { return this->ground.Level(x, y); }
	double Depth(double x, double y) const;
	double CentreDepth(int cx, int cy) const;

	const Seabed &bed;
	TileGround ground;
	double surface;
};

#endif /* MINI_WORLD_SHORE_RELIEF_H */
