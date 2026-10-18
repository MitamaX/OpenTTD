/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file seabed.h Where water lies over the map and how far the ground under it sinks, so water has depth to show. */

#ifndef MINI_WORLD_SEABED_H
#define MINI_WORLD_SEABED_H

#include <array>
#include <optional>

#include "../../map_type.h"
#include "../core/camera.h"
#include "lazy_lattice.h"

inline constexpr double DEEPEST_SINK = 4.0;
inline constexpr int SHELF_TILES = 6;

/** How a tile holds water, which decides where its surface lies and whether the ground under it may sink. */
enum class WaterForm : uint8_t {
	Dry,
	Open,
	Shore,
	Incline,
};

/* Past the map's edge lies open sea. */
WaterForm WaterFormOf(int tx, int ty);
double SurfaceLevelOf(int tx, int ty);
double CornerSink(int cx, int cy);
double LowestCornerAt(int cx, int cy);
double CentreSink(int tx, int ty);
bool SharesBasin(int tx, int ty, int nx, int ny);
/* A tile of level river water, whose bed may follow the water's curved outline. */
bool IsChannel(int tx, int ty);
bool IsRapids(int tx, int ty);
MapVector Downhill(int tx, int ty);
TileIndexDiffC FallStep(int tx, int ty);
/* A tile of sea that a fall, or a coast carrying a fall's water, runs down onto, so its bed curves on from theirs. */
bool IsRiverMouth(int tx, int ty);
/* The river a dry tile with nothing on it lies beside, its ground lying just as the tile's own, level or down the same slope, so the river's curved outline may reach onto it. */
std::optional<TileIndex> RiverOfBank(int tx, int ty);
bool IsRiverBank(int tx, int ty);
/* The fall a coast lies beside, sloping just as the coast does, so the fall's water runs on down along the coast instead of ending at the tile edge. */
std::optional<TileIndex> FallBesideCoast(int tx, int ty);
bool IsBuiltOver(int tx, int ty);
bool IsWooded(int tx, int ty);
/* A river, one of its bare banks or a coast beside one of its falls, whose ground follows the river's outline. */
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
	std::array<double, 4> SettledCorners(int tx, int ty) const;
	bool Shallow(int tx, int ty) const;

private:
	double LevelAt(int hx, int hy) const;
	double CornerLevel(int cx, int cy) const;
	double CornerMean(int tx, int ty) const;

	LazyLattice<double> levels;
	LazyLattice<bool> shallow;
};

#endif /* MINI_WORLD_SEABED_H */
