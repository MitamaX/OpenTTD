/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file bare_ground.h Where the land wears through to bare stone and how that stone is coloured, read alike by the ground's shader and by the stones standing on the ground. */

#ifndef MINI_WORLD_BARE_GROUND_H
#define MINI_WORLD_BARE_GROUND_H

#include <array>
#include <cstdint>
#include <numbers>

#include "../core/camera.h"

inline constexpr double STANDARD_SLOPE = 1.0;
inline constexpr double STEEPEST_SLOPE = std::numbers::sqrt2;
inline constexpr double ALPINE_ALTITUDE = 0.55;
inline constexpr double STEEP_BARING = 1.1;
inline constexpr double SHED_BARING = 0.9;
inline constexpr double ALPINE_BARING = 0.9;
inline constexpr double SCREE_EXPOSURE = 0.5;
inline constexpr double TOY_BARING = 0.55;
inline constexpr double ROCK_EXPOSURE = 0.8;
inline constexpr double EXPOSURE_BLEND = 0.14;
inline constexpr double MIN_RELIEF_SPAN = 6.0;
inline constexpr double STRATA_PER_LEVEL = 2.5;
inline constexpr double TOY_STRATA_PER_LEVEL = 1.0;
inline constexpr double LAYER_VARIETY = 0.3;
inline constexpr MapVector STRATA_DIP = {0.09, -0.06};

inline constexpr uint32_t ROCK_TONE = 0xFF807D78;
inline constexpr uint32_t ROCK_WARM_TONE = 0xFF948570;
inline constexpr uint32_t SCREE_TONE = 0xFF8C8578;
inline constexpr uint32_t CAP_SNOW_TONE = 0xFFDBE3F0;
inline constexpr uint32_t DRY_SAND_TONE = 0xFFC7B58C;
inline constexpr uint32_t LITTER_MOSS_TONE = 0xFF4A592B;
inline constexpr std::array<uint32_t, 4> TOY_STRATA_TONES = {0xFFF2A8C2, 0xFFC2B0ED, 0xFFFCD199, 0xFF9EDBE6};

#endif /* MINI_WORLD_BARE_GROUND_H */
