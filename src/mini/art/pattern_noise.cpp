/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file pattern_noise.cpp Stateless hashed noise that repeats exactly once across the unit square, so generated textures tile without seams. */

#include "../../stdafx.h"
#include "pattern_noise.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../core/camera.h"
#include "../core/seed.h"

#include "../../safeguards.h"

static constexpr int HASH_SHARE_BITS = 24;
static constexpr int HASH_DROPPED_BITS = std::numeric_limits<uint32_t>::digits - HASH_SHARE_BITS;
static constexpr float HASH_SHARE_UNIT = 1.0f / (1U << HASH_SHARE_BITS);
static constexpr float FBM_GAIN = 0.5f;
static constexpr int NEIGHBOUR_REACH = 1;
static constexpr uint32_t FEATURE_DEPTH_SALT = 0x9E3779B9U;
static constexpr float SHEEN_PERIOD = 32.0f;
static constexpr float SHEEN_SLANT = 0.5f;
static constexpr float SHEEN_SHARE = 0.25f;
static constexpr float SHEEN_LUMA = 0.10f;

int Wrapped(int value, int period)
{
	int rest = value % period;
	return rest < 0 ? rest + period : rest;
}

float Centred(float share)
{
	return share - 0.5f;
}

float Hash01(uint32_t seed, int x, int y)
{
	uint32_t hash = SubSeed(SubSeed(seed, static_cast<uint32_t>(x)), static_cast<uint32_t>(y));
	return (hash >> HASH_DROPPED_BITS) * HASH_SHARE_UNIT;
}

static float Eased(float share)
{
	return static_cast<float>(SmoothStep(0.0, 1.0, share));
}

float TileNoise(uint32_t seed, float x, float y, int cycles_x, int cycles_y)
{
	float lattice_x = x * cycles_x;
	float lattice_y = y * cycles_y;
	int x0 = static_cast<int>(std::floor(lattice_x));
	int y0 = static_cast<int>(std::floor(lattice_y));
	float ease_x = Eased(lattice_x - x0);
	float ease_y = Eased(lattice_y - y0);
	auto corner = [&](int dx, int dy) { return Hash01(seed, Wrapped(x0 + dx, cycles_x), Wrapped(y0 + dy, cycles_y)); };
	float north = std::lerp(corner(0, 0), corner(1, 0), ease_x);
	float south = std::lerp(corner(0, 1), corner(1, 1), ease_x);
	return std::lerp(north, south, ease_y);
}

float TileFbm(uint32_t seed, float x, float y, int cycles, int octaves)
{
	float sum = 0.0f;
	float weight = 0.0f;
	float amplitude = 1.0f;
	for (int octave = 0; octave < octaves; octave++) {
		int octave_cycles = cycles << octave;
		sum += amplitude * TileNoise(SubSeed(seed, static_cast<uint32_t>(octave)), x, y, octave_cycles, octave_cycles);
		weight += amplitude;
		amplitude *= FBM_GAIN;
	}
	return sum / weight;
}

/* Distance to the nearest of one scattered point per lattice cell, in cells. */
float TileCells(uint32_t seed, float x, float y, int cycles)
{
	float lattice_x = x * cycles;
	float lattice_y = y * cycles;
	int home_x = static_cast<int>(std::floor(lattice_x));
	int home_y = static_cast<int>(std::floor(lattice_y));
	float nearest = std::numeric_limits<float>::max();
	for (int dy = -NEIGHBOUR_REACH; dy <= NEIGHBOUR_REACH; dy++) {
		for (int dx = -NEIGHBOUR_REACH; dx <= NEIGHBOUR_REACH; dx++) {
			int cell_x = home_x + dx;
			int cell_y = home_y + dy;
			int wrapped_x = Wrapped(cell_x, cycles);
			int wrapped_y = Wrapped(cell_y, cycles);
			float feature_x = cell_x + Hash01(seed, wrapped_x, wrapped_y);
			float feature_y = cell_y + Hash01(SubSeed(seed, FEATURE_DEPTH_SALT), wrapped_x, wrapped_y);
			nearest = std::min(nearest, std::hypot(feature_x - lattice_x, feature_y - lattice_y));
		}
	}
	return nearest;
}

/* Slanted bright bands whose period divides the texture period both ways, so glass keeps them across seams. */
float GlassSheen(int x, int y, float phase)
{
	float stripe = (x + SHEEN_SLANT * y) / SHEEN_PERIOD + phase;
	return stripe - std::floor(stripe) < SHEEN_SHARE ? SHEEN_LUMA : 0.0f;
}
