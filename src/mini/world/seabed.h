/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file seabed.h Where water lies over the map and how far the ground under it sinks, so water has depth to show. */

#ifndef MINI_WORLD_SEABED_H
#define MINI_WORLD_SEABED_H

#include <vector>

#include "../core/camera.h"

inline constexpr double DEEPEST_SINK = 4.0;
inline constexpr int SHELF_TILES = 6;

/** How a tile holds water, which decides where its surface lies and whether the ground under it may sink. */
enum class WaterForm : uint8_t {
	Dry,
	Open,
	Shore,
	Incline,
};

/* Past the map's edge and over its border lies open sea. */
WaterForm WaterFormOf(int tx, int ty);
double SurfaceLevelOf(int tx, int ty);
double CornerSink(int cx, int cy);
double CentreSink(int tx, int ty);
bool SharesBasin(int tx, int ty, int nx, int ny);
/* A tile of level river water, whose bed may follow the water's curved outline. */
bool IsChannel(int tx, int ty);
/* A dry tile with nothing on it beside a river at its own lowest level, onto whose low ground the river's curved outline may reach. */
bool IsRiverBank(int tx, int ty);
/* A river or one of its bare banks, whose ground follows the river's outline. */
bool IsRiverside(int tx, int ty);

/* The ground under a point of a tile, down to where the bed sinks under water at the tile's corners. */
double BedLevel(int tx, int ty, double x, double y);

/* The bed under a block of tiles and a ring around it, on the half tile lattice: corners at even coordinates, tile middles at odd ones
 * and edge middles at one of each. Each point is worked out once, when the block's meshes first ask for it. */
class Seabed {
public:
	explicit Seabed(const TileSpan &tiles);

	double Level(int hx, int hy) const;
	double Sink(int cx, int cy) const;
	Vec3 Normal(int hx, int hy, int reach) const;

private:
	int hx0;
	int hy0;
	int columns;
	int rows;
	mutable std::vector<double> levels;
};

#endif /* MINI_WORLD_SEABED_H */
