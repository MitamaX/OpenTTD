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

/* A tile's water where its surface comes within reach of the sheet: a level tile's water lies at its lowest corner, a stream's runs from its lowest corner to its highest. */
static double SheetTexel(int tx, int ty, double level)
{
	TileIndex tile = TileXY(std::clamp<int>(tx, 0, Map::MaxX()), std::clamp<int>(ty, 0, Map::MaxY()));
	WaterTexel water = _world_tiles.WaterAt(tile);
	SurfaceTexel surface = _world_tiles.SurfaceAt(tile);
	double low = std::min({surface.north, surface.west, surface.east, surface.south});
	double high = water.level == FULL_WATER ? std::max({surface.north, surface.west, surface.east, surface.south}) : low;
	return level >= low - SHEET_TOLERANCE && level <= high + SHEET_TOLERANCE ? water.level / TEXEL_SCALE : 0.0;
}

double SheetWater(double x, double y, double level)
{
	double tx = x + ShoreWarp(x, y, SHORE_WARP_SEEDS[0]) - HALF_TILE;
	double ty = y + ShoreWarp(x, y, SHORE_WARP_SEEDS[1]) - HALF_TILE;
	double cell_x = std::floor(tx);
	double cell_y = std::floor(ty);
	std::array<double, 4> wx = SplineWeights(tx - cell_x);
	std::array<double, 4> wy = SplineWeights(ty - cell_y);
	double field = 0.0;
	for (int j = 0; j < 4; j++) {
		for (int i = 0; i < 4; i++) field += wx[i] * wy[j] * SheetTexel(static_cast<int>(cell_x) + i - 1, static_cast<int>(cell_y) + j - 1, level);
	}
	return field;
}
