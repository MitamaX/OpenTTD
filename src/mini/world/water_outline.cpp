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
#include "../map/tile_shapes.h"
#include "../map/world_tiles.h"

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
	float cell_x = std::floor(x);
	float cell_y = std::floor(y);
	float fx = x - cell_x;
	float fy = y - cell_y;
	float sx = fx * fx * (3.0f - 2.0f * fx);
	float sy = fy * fy * (3.0f - 2.0f * fy);
	int cx = static_cast<int>(cell_x);
	int cy = static_cast<int>(cell_y);
	float north = std::lerp(LatticeHash(cx, cy), LatticeHash(cx + 1, cy), sx);
	float south = std::lerp(LatticeHash(cx, cy + 1), LatticeHash(cx + 1, cy + 1), sx);
	return std::lerp(north, south, sy);
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

/* A tile's water and the levels it spans: a level tile's water lies at its lowest corner, a stream's runs from its lowest corner to its highest. */
struct SheetSpan {
	WaterTexel water;
	double low;
	double high;
	double top;
};

static SheetSpan SheetSpanOf(int tx, int ty)
{
	TileIndex tile = TileXY(std::clamp<int>(tx, 0, Map::MaxX()), std::clamp<int>(ty, 0, Map::MaxY()));
	WaterTexel water = _world_tiles.WaterAt(tile);
	SurfaceTexel surface = _world_tiles.SurfaceAt(tile);
	double low = std::min({surface.north, surface.west, surface.east, surface.south});
	double top = std::max({surface.north, surface.west, surface.east, surface.south});
	return {water, low, water.level == FULL_WATER ? top : low, top};
}

static double JoiningOf(const SheetSpan &span, double level)
{
	double apart = std::max({span.low - level, level - span.high, 0.0});
	return 1.0 - SmoothStep(SHEET_HOLD, SHEET_REACH, apart);
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
	double brim;

	void Add(const SheetShares &other, double weight)
	{
		this->level += other.level * weight;
		this->pooled += other.pooled * weight;
		this->water += other.water * weight;
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
	return {water.level * joining, pooled, (water.sea + water.canal + water.river) * joining, brim};
}

double SheetWater(double x, double y, double level)
{
	double tx = x + ShoreWarp(x, y, SHORE_WARP_SEEDS[0]) - HALF_TILE;
	double ty = y + ShoreWarp(x, y, SHORE_WARP_SEEDS[1]) - HALF_TILE;
	double cell_x = std::floor(tx);
	double cell_y = std::floor(ty);
	std::array<double, 4> wx = SplineWeights(tx - cell_x);
	std::array<double, 4> wy = SplineWeights(ty - cell_y);
	SheetShares field{};
	for (int j = 0; j < 4; j++) {
		for (int i = 0; i < 4; i++) field.Add(SheetTexel(static_cast<int>(cell_x) + i - 1, static_cast<int>(cell_y) + j - 1, level), wx[i] * wy[j]);
	}
	return field.level - BRIM_PULL * field.brim * field.pooled / std::max(field.water, LEAST_WATER);
}
