/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file seabed.cpp Where water lies over the map and how far the ground under it sinks, so water has depth to show. */

#include "../../stdafx.h"
#include "seabed.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "../../map_func.h"
#include "../map/tile_shapes.h"
#include "../map/world_tiles.h"

#include "../../safeguards.h"

static constexpr double SHALLOWEST_SINK = 0.3;
static constexpr double SHORE_GRADIENT = 1.2;
static constexpr int CORNER_MARGIN = 1;
static constexpr double UNKNOWN = std::numeric_limits<double>::quiet_NaN();

static bool IsFlat(const SurfaceTexel &surface)
{
	return surface.north == surface.west && surface.north == surface.east && surface.north == surface.south;
}

static uint8_t LowestCorner(const SurfaceTexel &surface)
{
	return std::min({surface.north, surface.west, surface.east, surface.south});
}

/* The level of one corner point of a tile, the corner named by its offset from the tile's north corner. */
static uint8_t CornerLevelOf(const SurfaceTexel &surface, int dx, int dy)
{
	static constexpr std::array<std::array<uint8_t SurfaceTexel::*, 2>, 2> CORNERS = {{
		{&SurfaceTexel::north, &SurfaceTexel::east},
		{&SurfaceTexel::west, &SurfaceTexel::south},
	}};
	return surface.*CORNERS[dx][dy];
}

WaterForm WaterFormOf(int tx, int ty)
{
	if (!OnMap(tx, ty)) return WaterForm::Open;
	TileIndex tile = TileXY(tx, ty);
	if (_world_tiles.GroundAt(tile).material == GroundMaterial::Void) return WaterForm::Open;
	WaterTexel water = _world_tiles.WaterAt(tile);
	if (water.level == 0) return WaterForm::Dry;
	if (IsFlat(_world_tiles.SurfaceAt(tile))) return WaterForm::Open;
	return water.level < FULL_WATER ? WaterForm::Shore : WaterForm::Incline;
}

double SurfaceLevelOf(int tx, int ty)
{
	return OnMap(tx, ty) ? LowestCorner(_world_tiles.SurfaceAt(TileXY(tx, ty))) : 0.0;
}

/* How far a point lies from the nearest tile that is not open water, searched out to the edge of the shelf. */
static double ShoreDistance(double x, double y)
{
	int cx = static_cast<int>(std::floor(x));
	int cy = static_cast<int>(std::floor(y));
	double nearest = SHELF_TILES;
	for (int ty = cy - SHELF_TILES; ty <= cy + SHELF_TILES; ty++) {
		for (int tx = cx - SHELF_TILES; tx <= cx + SHELF_TILES; tx++) {
			double dx = std::max({tx - x, 0.0, x - (tx + 1)});
			double dy = std::max({ty - y, 0.0, y - (ty + 1)});
			double distance = std::hypot(dx, dy);
			if (distance < nearest && WaterFormOf(tx, ty) != WaterForm::Open) nearest = distance;
		}
	}
	return nearest;
}

/* The ground slopes gently away from the shore and levels out toward the shelf's edge. */
static double SinkAt(double shore_distance)
{
	double share = std::min(shore_distance / SHELF_TILES, 1.0);
	double shelf = SHALLOWEST_SINK + (DEEPEST_SINK - SHALLOWEST_SINK) * (1.0 - (1.0 - share) * (1.0 - share));
	return std::min(shelf, SHALLOWEST_SINK + SHORE_GRADIENT * shore_distance);
}

/* A corner sinks only where every tile meeting there holds level water at the corner's own height, so the ground never breaks open beside land or a sloping stream. */
double CornerSink(int cx, int cy)
{
	for (int dy = 0; dy <= 1; dy++) {
		for (int dx = 0; dx <= 1; dx++) {
			int tx = cx - 1 + dx;
			int ty = cy - 1 + dy;
			WaterForm form = WaterFormOf(tx, ty);
			if (form != WaterForm::Open && form != WaterForm::Shore) return 0.0;
			if (!OnMap(tx, ty)) continue;
			SurfaceTexel surface = _world_tiles.SurfaceAt(TileXY(tx, ty));
			if (CornerLevelOf(surface, 1 - dx, 1 - dy) != LowestCorner(surface)) return 0.0;
		}
	}
	return SinkAt(ShoreDistance(cx, cy));
}

double CentreSink(int tx, int ty)
{
	if (WaterFormOf(tx, ty) != WaterForm::Open) return 0.0;
	return SinkAt(ShoreDistance(tx + HALF_TILE, ty + HALF_TILE));
}

bool SharesBasin(int tx, int ty, int nx, int ny)
{
	return WaterFormOf(tx, ty) == WaterForm::Open && WaterFormOf(nx, ny) == WaterForm::Open && SurfaceLevelOf(tx, ty) == SurfaceLevelOf(nx, ny);
}

bool IsChannel(int tx, int ty)
{
	if (!OnMap(tx, ty) || WaterFormOf(tx, ty) != WaterForm::Open) return false;
	WaterTexel water = _world_tiles.WaterAt(TileXY(tx, ty));
	return water.river > 0 && water.canal == 0;
}

bool IsRiverBank(int tx, int ty)
{
	if (!OnMap(tx, ty) || WaterFormOf(tx, ty) != WaterForm::Dry) return false;
	TileIndex tile = TileXY(tx, ty);
	GroundTexel ground = _world_tiles.GroundAt(tile);
	NetworkTexel network = _world_tiles.NetworkAt(tile);
	if (ground.flora != 0 || (network.style & NETWORK_BRIDGE_BIT) != 0 || GroundworkOf(ground, network) != Groundwork::Open) return false;
	double level = SurfaceLevelOf(tx, ty);
	for (int ny = ty - 1; ny <= ty + 1; ny++) {
		for (int nx = tx - 1; nx <= tx + 1; nx++) {
			if (IsChannel(nx, ny) && SurfaceLevelOf(nx, ny) == level) return true;
		}
	}
	return false;
}

bool IsRiverside(int tx, int ty)
{
	return IsChannel(tx, ty) || IsRiverBank(tx, ty);
}

static double CornerBedLevel(int cx, int cy)
{
	return GroundLevel(cx, cy) - CornerSink(cx, cy);
}

/* The bed at a point of the half tile lattice. An edge's middle sinks where open water runs on across it, and otherwise lies on the line between the edge's ends. */
static double BedLevelAt(int hx, int hy)
{
	int x = hx >> 1;
	int y = hy >> 1;
	bool across_half = (hx & 1) != 0;
	bool down_half = (hy & 1) != 0;
	if (!across_half && !down_half) return CornerBedLevel(x, y);
	if (across_half && down_half) return WaterFormOf(x, y) == WaterForm::Open ? SurfaceLevelOf(x, y) - CentreSink(x, y) : GroundLevel(x + HALF_TILE, y + HALF_TILE);

	int nx = across_half ? x : x - 1;
	int ny = across_half ? y - 1 : y;
	if (SharesBasin(x, y, nx, ny)) return SurfaceLevelOf(x, y) - SinkAt(ShoreDistance(hx * HALF_TILE, hy * HALF_TILE));
	int ex = across_half ? x + 1 : x;
	int ey = across_half ? y : y + 1;
	return (CornerBedLevel(x, y) + CornerBedLevel(ex, ey)) * 0.5;
}

double BedLevel(int tx, int ty, double x, double y)
{
	double level = TileGround(tx, ty).Level(x, y);
	if (WaterFormOf(tx, ty) == WaterForm::Dry) return level;
	double fx = x - tx;
	double fy = y - ty;
	double north = std::lerp(CornerSink(tx, ty), CornerSink(tx + 1, ty), fx);
	double south = std::lerp(CornerSink(tx, ty + 1), CornerSink(tx + 1, ty + 1), fx);
	return level - std::lerp(north, south, fy);
}

Seabed::Seabed(const TileSpan &tiles) :
	hx0(2 * (tiles.tx0 - CORNER_MARGIN)),
	hy0(2 * (tiles.ty0 - CORNER_MARGIN)),
	columns(2 * (tiles.tx1 - tiles.tx0 + 1 + 2 * CORNER_MARGIN) + 1),
	rows(2 * (tiles.ty1 - tiles.ty0 + 1 + 2 * CORNER_MARGIN) + 1),
	levels(static_cast<size_t>(columns) * rows, UNKNOWN)
{
}

double Seabed::Level(int hx, int hy) const
{
	int i = hx - this->hx0;
	int j = hy - this->hy0;
	if (i < 0 || j < 0 || i >= this->columns || j >= this->rows) return BedLevelAt(hx, hy);
	double &level = this->levels[static_cast<size_t>(j) * this->columns + i];
	if (std::isnan(level)) level = BedLevelAt(hx, hy);
	return level;
}

double Seabed::Sink(int cx, int cy) const
{
	return GroundLevel(cx, cy) - this->Level(2 * cx, 2 * cy);
}

/* The bed's slope across a reach of half tiles either side. */
Vec3 Seabed::Normal(int hx, int hy, int reach) const
{
	double rise = LevelRise();
	double span = reach * 2.0 * HALF_TILE;
	double slope_x = (this->Level(hx + reach, hy) - this->Level(hx - reach, hy)) * rise / span;
	double slope_y = (this->Level(hx, hy + reach) - this->Level(hx, hy - reach)) * rise / span;
	return Normalised({-slope_x, -slope_y, 1.0});
}
