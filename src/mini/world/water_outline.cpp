/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file water_outline.cpp The curved outline the water shaders draw, worked out on the CPU as well so the ground under the water can follow it. */

#include "../../stdafx.h"
#include "water_outline.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "../../map_func.h"
#include "../core/lattice_noise.h"
#include "../map/tile_shapes.h"
#include "../map/world_tiles.h"
#include "seabed.h"

#include "../../safeguards.h"

static constexpr double TEXEL_SCALE = 255.0;
static constexpr double LEAST_WATER = 1e-4;

/* The shaders' lattice hash, bit for bit. */
static float LatticeHash(int x, int y)
{
	uint32_t h = static_cast<uint32_t>(x) * 0x8DA6B343U ^ static_cast<uint32_t>(y) * 0xD8163841U;
	h = (h ^ (h >> 15U)) * 0x2C1B3C6DU;
	h ^= h >> 12U;
	return static_cast<float>(h) * (1.0f / 4294967296.0f);
}

/* The shaders' value noise, in the same single precision. */
static float Noise(float x, float y)
{
	return LatticeBlend(x, y, LatticeHash);
}

static double ShoreWarp(double x, double y, double seed)
{
	float frequency = static_cast<float>(SHORE_WARP_FREQUENCY);
	float offset = static_cast<float>(seed);
	float noise = Noise(static_cast<float>(x) * frequency + offset, static_cast<float>(y) * frequency + offset);
	return (noise - 0.5) * 2.0 * SHORE_WARP;
}

/* The weights a cubic B-spline gives the four texels around a point, f of the way from the second to the third. */
static std::array<double, 4> SplineWeights(double f)
{
	double g = 1.0 - f;
	double w0 = g * g * g / 6.0;
	double w1 = (4.0 - 6.0 * f * f + 3.0 * f * f * f) / 6.0;
	double w3 = f * f * f / 6.0;
	return {w0, w1, 1.0 - w0 - w1 - w3, w3};
}

static TileIndex ClampedTile(int tx, int ty)
{
	return TileXY(std::clamp<int>(tx, 0, Map::MaxX()), std::clamp<int>(ty, 0, Map::MaxY()));
}

/* A tile's water and the levels it spans: a level tile's water lies at its lowest corner, a stream's runs from its lowest corner to its highest. */
struct SheetSpan {
	WaterTexel water;
	double low;
	double high;
	double top;
};

static SheetSpan SheetSpanOf(int tx, int ty)
{
	TileIndex tile = ClampedTile(tx, ty);
	WaterTexel water = _world_tiles.WaterAt(tile);
	SurfaceTexel surface = _world_tiles.SurfaceAt(tile);
	double low = surface.Lowest();
	double top = surface.Highest();
	return {water, low, water.level == FULL_WATER ? top : low, top};
}

static double JoiningOf(const SheetSpan &span, double level)
{
	double apart = std::max({span.low - level, level - span.high, 0.0});
	bool river = span.water.river > 0;
	return 1.0 - SmoothStep(river ? RIVER_SHEET_HOLD : SHEET_HOLD, river ? RIVER_SHEET_REACH : SHEET_REACH, apart);
}

double Joining(int tx, int ty, double level)
{
	return JoiningOf(SheetSpanOf(tx, ty), level);
}

static double BrimShare(double level, double low, double high)
{
	return SmoothStep(0.0, BRIM_FADE, level - low) * (1.0 - SmoothStep(0.0, BRIM_FADE, high - level));
}

struct SheetShares {
	double level;
	double pooled;
	double water;
	double river;
	double brim;

	void Add(const SheetShares &other, double weight)
	{
		this->level += other.level * weight;
		this->pooled += other.pooled * weight;
		this->water += other.water * weight;
		this->river += other.river * weight;
		this->brim += other.brim * weight;
	}
};

static SheetShares SheetTexel(int tx, int ty, double level)
{
	SheetSpan span = SheetSpanOf(tx, ty);
	const WaterTexel &water = span.water;
	double joining = JoiningOf(span, level) / TEXEL_SCALE;
	double brim = water.level == 0 ? BrimShare(level, span.low, span.top) : 0.0;
	double pooled = span.high > span.low ? 0.0 : water.river * joining;
	return {water.level * joining, pooled, (water.sea + water.canal + water.river) * joining, water.river * joining, brim};
}

static double DistanceToTile(double x, double y, int tx, int ty)
{
	return std::hypot(x - std::clamp<double>(x, tx, tx + 1), y - std::clamp<double>(y, ty, ty + 1));
}

static double DistanceToTreeCrown(double x, double y, int tx, int ty)
{
	return std::max(std::hypot(x - tx - HALF_TILE, y - ty - HALF_TILE) - TILE_CORNER_REACH, 0.0);
}

static bool IsRiverChannelTexel(const WaterTexel &water, const SurfaceTexel &surface)
{
	return water.level > 0 && water.river > 0 && water.canal == 0 && (surface.Lowest() == surface.Highest() || water.level == FULL_WATER);
}

static bool FlatRiverAcross(const SurfaceTexel &own, int tx, int ty)
{
	TileIndex beside = ClampedTile(tx, ty);
	SurfaceTexel across = _world_tiles.SurfaceAt(beside);
	return across == own && IsRiverChannelTexel(_world_tiles.WaterAt(beside), across);
}

static bool IsSpanned(int tx, int ty)
{
	return (_world_tiles.NetworkAt(ClampedTile(tx, ty)).style & NETWORK_BRIDGE_BIT) != 0;
}

static bool FlatRiverAlong(const SurfaceTexel &own, int flank_x, int flank_y, int axis_x, int axis_y)
{
	return FlatRiverAcross(own, flank_x, flank_y) && FlatRiverAcross(own, flank_x - axis_x, flank_y - axis_y)
		&& FlatRiverAcross(own, flank_x + axis_x, flank_y + axis_y);
}

static bool BridgeHeadBesideRiver(int tx, int ty)
{
	NetworkTexel network = _world_tiles.NetworkAt(ClampedTile(tx, ty));
	bool without_way = (network.track | network.road | network.tram) == 0;
	if ((network.style & NETWORK_BRIDGE_BIT) == 0 || !without_way || IsWooded(tx, ty)) return false;
	bool along_x = IsSpanned(tx - 1, ty) || IsSpanned(tx + 1, ty);
	bool along_y = IsSpanned(tx, ty - 1) || IsSpanned(tx, ty + 1);
	if (along_x == along_y) return false;
	int axis_x = along_x ? 1 : 0;
	int axis_y = along_x ? 0 : 1;
	SurfaceTexel own = _world_tiles.SurfaceAt(ClampedTile(tx, ty));
	return FlatRiverAlong(own, tx + axis_y, ty + axis_x, axis_x, axis_y) || FlatRiverAlong(own, tx - axis_y, ty - axis_x, axis_x, axis_y);
}

static double DistanceToHold(double x, double y, int tx, int ty)
{
	TileIndex tile = ClampedTile(tx, ty);
	int cx = TileX(tile);
	int cy = TileY(tile);
	if (_world_tiles.WaterAt(tile).level != 0 || BridgeHeadBesideRiver(cx, cy)) return NOTHING_HELD;
	if (IsBuiltOver(cx, cy)) return DistanceToTile(x, y, tx, ty);
	return IsWooded(cx, cy) ? DistanceToTreeCrown(x, y, tx, ty) : NOTHING_HELD;
}

static double DryRimmed(double warped_x, double warped_y, double field, double river_share)
{
	if (field <= DRY_RIM_FLOOR || river_share <= 0.0) return field;
	int corner_x = static_cast<int>(std::floor(warped_x + HALF_TILE));
	int corner_y = static_cast<int>(std::floor(warped_y + HALF_TILE));
	double capped = field;
	for (int ty = corner_y - 1; ty <= corner_y; ty++) {
		for (int tx = corner_x - 1; tx <= corner_x; tx++) {
			capped = std::min(capped, DRY_RIM_FLOOR + DRY_RIM_RISE * DistanceToHold(warped_x, warped_y, tx, ty));
		}
	}
	return std::lerp(field, capped, river_share);
}

double SheetWater(double x, double y, double level)
{
	double warped_x = x + ShoreWarp(x, y, SHORE_WARP_SEEDS[0]);
	double warped_y = y + ShoreWarp(x, y, SHORE_WARP_SEEDS[1]);
	double tx = warped_x - HALF_TILE;
	double ty = warped_y - HALF_TILE;
	double cell_x = std::floor(tx);
	double cell_y = std::floor(ty);
	std::array<double, 4> wx = SplineWeights(tx - cell_x);
	std::array<double, 4> wy = SplineWeights(ty - cell_y);
	SheetShares field{};
	for (int j = 0; j < 4; j++) {
		for (int i = 0; i < 4; i++) field.Add(SheetTexel(static_cast<int>(cell_x) + i - 1, static_cast<int>(cell_y) + j - 1, level), wx[i] * wy[j]);
	}
	double watered = std::max(field.water, LEAST_WATER);
	double pulled = field.level - BRIM_PULL * field.brim * field.pooled / watered;
	return DryRimmed(warped_x, warped_y, pulled, field.river / watered);
}
