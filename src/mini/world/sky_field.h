/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sky_field.h How high the bodies around every tile rise against the sky, eight ways round, one texel per tile, for the light that reaches streets and yards. */

#ifndef MINI_WORLD_SKY_FIELD_H
#define MINI_WORLD_SKY_FIELD_H

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "../../core/geometry_type.hpp"
#include "massing_field.h"

inline constexpr int HORIZON_REACH_TILES = 6;
inline constexpr int SKY_SECTORS = 8;
inline constexpr float SKY_HEIGHT_STEP = 1.0f / 64.0f;
inline constexpr float SKY_REACH_STEP = 1.0f / 32.0f;
inline constexpr float NEAREST_HORIZON = 0.05f;
inline constexpr float SKY_SECTOR_TURN = 0.70710678f;
inline constexpr uint SKY_SECTOR_BITS = 16;
inline constexpr uint SKY_REACH_SHIFT = 8;
inline constexpr uint SKY_SECTORS_PER_CHANNEL = 2;

using SkyTexel = std::array<uint32_t, SKY_SECTORS / SKY_SECTORS_PER_CHANNEL>;

class Surroundings;

class SkyField {
public:
	void Clear(Dimension map);
	std::vector<Rect> Resurvey(std::span<const Rect> changed, const MassingField &massing);
	const void *Texels() const { return this->texels.data(); }

private:
	void Survey(const Rect &area, const Surroundings &around, const MassingField &massing);

	Dimension map{};
	uint32_t generation = 0;
	std::vector<SkyTexel> texels;
	std::vector<uint32_t> surveyed;
};

#endif /* MINI_WORLD_SKY_FIELD_H */
