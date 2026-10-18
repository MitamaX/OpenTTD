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
#include <cmath>
#include <optional>

#include "../../map_func.h"
#include "../map/tile_shapes.h"
#include "../map/world_tiles.h"

#include "../../safeguards.h"

static constexpr double SHALLOWEST_SINK = 0.3;
static constexpr double SHORE_GRADIENT = 1.2;
static constexpr int CORNER_MARGIN = 1;
static constexpr int SHALLOW_MARGIN = 5;
static constexpr int SHALLOWS_REACH = 2;
static constexpr double SHORE_BLEND_SHARPNESS = 4.0;
static constexpr double SHORE_BLEND_REACH = 1.0;

static bool IsFlat(const SurfaceTexel &surface)
{
	return surface.north == surface.west && surface.north == surface.east && surface.north == surface.south;
}

WaterForm WaterFormOf(int tx, int ty)
{
	if (!OnMap(tx, ty)) return WaterForm::Open;
	TileIndex tile = TileXY(tx, ty);
	WaterTexel water = _world_tiles.WaterAt(tile);
	if (water.level == 0) return WaterForm::Dry;
	if (IsFlat(_world_tiles.SurfaceAt(tile))) return WaterForm::Open;
	return water.level < FULL_WATER ? WaterForm::Shore : WaterForm::Incline;
}

double SurfaceLevelOf(int tx, int ty)
{
	return OnMap(tx, ty) ? _world_tiles.SurfaceAt(TileXY(tx, ty)).Lowest() : 0.0;
}

/* How far a point lies from the tiles that are not open water, searched out to the edge of the shelf: the nearest one's distance, drawn in by each other tile close behind it, so it runs smooth where two tiles are as near. */
static double ShoreDistance(double x, double y)
{
	int cx = static_cast<int>(std::floor(x));
	int cy = static_cast<int>(std::floor(y));
	double reach = SHELF_TILES;
	double weight = 0.0;
	auto weigh = [&](int tx, int ty) {
		double squared = SquaredDistanceToTile(x, y, tx, ty);
		if (squared >= reach * reach || WaterFormOf(tx, ty) == WaterForm::Open) return;
		double distance = std::sqrt(squared);
		weight += std::exp(-SHORE_BLEND_SHARPNESS * distance);
		reach = std::min(reach, distance + SHORE_BLEND_REACH);
	};
	for (int ring = 0; ring <= SHELF_TILES && ring - 1 < reach; ring++) {
		for (int ty = cy - ring; ty <= cy + ring; ty++) {
			int step = ty == cy - ring || ty == cy + ring ? 1 : 2 * ring;
			for (int tx = cx - ring; tx <= cx + ring; tx += step) weigh(tx, ty);
		}
	}
	return weight > 0.0 ? std::max(-std::log(weight) / SHORE_BLEND_SHARPNESS, 0.0) : SHELF_TILES;
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
	for (const CornerTile &meeting : TilesAtCorner(cx, cy)) {
		WaterForm form = WaterFormOf(meeting.tx, meeting.ty);
		if (form != WaterForm::Open && form != WaterForm::Shore) return 0.0;
		if (OnMap(meeting.tx, meeting.ty) && meeting.Level() != SurfaceLevelOf(meeting.tx, meeting.ty)) return 0.0;
	}
	return SinkAt(ShoreDistance(cx, cy));
}

double LowestCornerAt(int cx, int cy)
{
	std::optional<double> lowest;
	for (const CornerTile &meeting : TilesAtCorner(cx, cy)) {
		if (!IsMapTile(meeting.tx, meeting.ty)) continue;
		double level = meeting.Level();
		lowest = std::min(lowest.value_or(level), level);
	}
	return lowest.value_or(GroundLevel(cx, cy));
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

static bool RunsAsRiver(int tx, int ty, WaterForm form)
{
	if (!OnMap(tx, ty) || WaterFormOf(tx, ty) != form) return false;
	WaterTexel water = _world_tiles.WaterAt(TileXY(tx, ty));
	return water.river > 0 && water.canal == 0;
}

bool IsChannel(int tx, int ty)
{
	return RunsAsRiver(tx, ty, WaterForm::Open);
}

bool IsRapids(int tx, int ty)
{
	return RunsAsRiver(tx, ty, WaterForm::Incline);
}

MapVector Downhill(int tx, int ty)
{
	SurfaceTexel surface = _world_tiles.SurfaceAt(TileXY(tx, ty));
	double rise_x = (surface.west - surface.north) + (surface.south - surface.east);
	double rise_y = (surface.east - surface.north) + (surface.south - surface.west);
	return Unit({-rise_x, -rise_y});
}

TileIndexDiffC FallStep(int tx, int ty)
{
	MapVector down = Downhill(tx, ty);
	return {static_cast<int16_t>(std::lround(down.x)), static_cast<int16_t>(std::lround(down.y))};
}

static bool FallsOnto(int nx, int ny, int tx, int ty)
{
	if (!IsRapids(nx, ny) && !FallBesideCoast(nx, ny).has_value()) return false;
	TileIndexDiffC fall = FallStep(nx, ny);
	return nx + fall.x == tx && ny + fall.y == ty;
}

bool IsRiverMouth(int tx, int ty)
{
	if (!OnMap(tx, ty) || WaterFormOf(tx, ty) != WaterForm::Open || _world_tiles.WaterAt(TileXY(tx, ty)).sea == 0) return false;
	return AnySideTile(tx, ty, [&](int nx, int ny) { return FallsOnto(nx, ny, tx, ty); });
}

static std::optional<TileIndex> RiverAlong(int tx, int ty)
{
	SurfaceTexel surface = _world_tiles.SurfaceAt(TileXY(tx, ty));
	for (int ny = ty - 1; ny <= ty + 1; ny++) {
		for (int nx = tx - 1; nx <= tx + 1; nx++) {
			if ((IsChannel(nx, ny) || IsRapids(nx, ny)) && _world_tiles.SurfaceAt(TileXY(nx, ny)) == surface) return TileXY(nx, ny);
		}
	}
	return std::nullopt;
}

bool IsBuiltOver(int tx, int ty)
{
	TileIndex tile = TileXY(tx, ty);
	GroundTexel ground = _world_tiles.GroundAt(tile);
	NetworkTexel network = _world_tiles.NetworkAt(tile);
	return (network.style & NETWORK_BRIDGE_BIT) != 0 || GroundworkOf(ground, network) != Groundwork::Open;
}

bool IsWooded(int tx, int ty)
{
	return TreeCount(_world_tiles.GroundAt(TileXY(tx, ty)).flora) != 0;
}

static bool IsBareGround(int tx, int ty)
{
	return !IsBuiltOver(tx, ty) && !IsWooded(tx, ty);
}

std::optional<TileIndex> RiverOfBank(int tx, int ty)
{
	if (!OnMap(tx, ty) || WaterFormOf(tx, ty) != WaterForm::Dry || !IsBareGround(tx, ty)) return std::nullopt;
	return RiverAlong(tx, ty);
}

bool IsRiverBank(int tx, int ty)
{
	return RiverOfBank(tx, ty).has_value();
}

std::optional<TileIndex> FallBesideCoast(int tx, int ty)
{
	if (!OnMap(tx, ty) || WaterFormOf(tx, ty) != WaterForm::Shore) return std::nullopt;
	SurfaceTexel surface = _world_tiles.SurfaceAt(TileXY(tx, ty));
	std::optional<TileIndex> fall;
	AnySideTile(tx, ty, [&](int nx, int ny) {
		if (IsRapids(nx, ny) && _world_tiles.SurfaceAt(TileXY(nx, ny)) == surface) fall = TileXY(nx, ny);
		return fall.has_value();
	});
	return fall;
}

bool IsRiverside(int tx, int ty)
{
	return IsChannel(tx, ty) || IsRapids(tx, ty) || IsRiverMouth(tx, ty) || IsRiverBank(tx, ty) || FallBesideCoast(tx, ty).has_value();
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

static bool NearLand(int tx, int ty)
{
	for (int dy = -SHALLOWS_REACH; dy <= SHALLOWS_REACH; dy++) {
		for (int dx = -SHALLOWS_REACH; dx <= SHALLOWS_REACH; dx++) {
			if (WaterFormOf(tx + dx, ty + dy) != WaterForm::Open) return true;
		}
	}
	return false;
}

static bool IsShallowSea(int tx, int ty)
{
	if (!OnMap(tx, ty) || WaterFormOf(tx, ty) != WaterForm::Open || IsRiverMouth(tx, ty)) return false;
	TileIndex tile = TileXY(tx, ty);
	if (_world_tiles.WaterAt(tile).sea == 0 || (_world_tiles.NetworkAt(tile).style & NETWORK_BRIDGE_BIT) != 0) return false;
	return NearLand(tx, ty);
}

Seabed::Seabed(const TileSpan &tiles) :
	levels(2 * (tiles.tx0 - CORNER_MARGIN), 2 * (tiles.ty0 - CORNER_MARGIN), 2 * (tiles.tx1 - tiles.tx0 + 1 + 2 * CORNER_MARGIN) + 1, 2 * (tiles.ty1 - tiles.ty0 + 1 + 2 * CORNER_MARGIN) + 1),
	shallow(tiles.tx0 - SHALLOW_MARGIN, tiles.ty0 - SHALLOW_MARGIN, tiles.tx1 - tiles.tx0 + 1 + 2 * SHALLOW_MARGIN, tiles.ty1 - tiles.ty0 + 1 + 2 * SHALLOW_MARGIN)
{
}

double Seabed::Level(int hx, int hy) const
{
	return this->levels.At(hx, hy, [this](int x, int y) { return this->LevelAt(x, y); });
}

double Seabed::Sink(int cx, int cy) const
{
	return LowestCornerAt(cx, cy) - this->CornerLevel(cx, cy);
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

bool Seabed::Shallow(int tx, int ty) const
{
	return this->shallow.At(tx, ty, IsShallowSea);
}

std::array<double, 4> Seabed::SettledCorners(int tx, int ty) const
{
	SurfaceTexel surface = _world_tiles.SurfaceAt(TileXY(tx, ty));
	std::array<double, 4> corners;
	for (size_t i = 0; i < corners.size(); i++) {
		int dx = static_cast<int>(i % 2);
		int dy = static_cast<int>(i / 2);
		corners[i] = surface.CornerAt(dx, dy) - this->Sink(tx + dx, ty + dy);
	}
	return corners;
}

/* The bed at a point of the half tile lattice. An edge's middle sinks where open water runs on across it, and otherwise lies on the line between the edge's ends. */
double Seabed::LevelAt(int hx, int hy) const
{
	int x = hx >> 1;
	int y = hy >> 1;
	bool across_half = (hx & 1) != 0;
	bool down_half = (hy & 1) != 0;
	if (!across_half && !down_half) return LowestCornerAt(x, y) - CornerSink(x, y);
	if (across_half && down_half) return WaterFormOf(x, y) == WaterForm::Open ? SurfaceLevelOf(x, y) - CentreSink(x, y) : this->CornerMean(x, y);

	int nx = across_half ? x : x - 1;
	int ny = across_half ? y - 1 : y;
	if (SharesBasin(x, y, nx, ny)) return SurfaceLevelOf(x, y) - SinkAt(ShoreDistance(hx * HALF_TILE, hy * HALF_TILE));
	int ex = across_half ? x + 1 : x;
	int ey = across_half ? y : y + 1;
	return (this->CornerLevel(x, y) + this->CornerLevel(ex, ey)) * 0.5;
}

double Seabed::CornerLevel(int cx, int cy) const
{
	return this->Level(2 * cx, 2 * cy);
}

double Seabed::CornerMean(int tx, int ty) const
{
	return (this->CornerLevel(tx, ty) + this->CornerLevel(tx + 1, ty) + this->CornerLevel(tx, ty + 1) + this->CornerLevel(tx + 1, ty + 1)) * 0.25;
}
