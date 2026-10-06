/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file pattern_noise.h Stateless hashed noise that repeats exactly once across the unit square, so generated textures tile without seams. */

#ifndef MINI_ART_PATTERN_NOISE_H
#define MINI_ART_PATTERN_NOISE_H

#include <cstdint>

inline constexpr float GLASS_FRAME_LUMA = 0.95f;

int Wrapped(int value, int period);
float Centred(float share);

float Hash01(uint32_t seed, int x, int y);
float TileNoise(uint32_t seed, float x, float y, int cycles_x, int cycles_y);
float TileFbm(uint32_t seed, float x, float y, int cycles, int octaves);
float TileCells(uint32_t seed, float x, float y, int cycles);
float GlassSheen(int x, int y, float phase);

#endif /* MINI_ART_PATTERN_NOISE_H */
