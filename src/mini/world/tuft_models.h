/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tuft_models.h Tufts of grass, thin blades fanning out from a foot, and low scrub bushes, painted like a vehicle's paintwork so each takes its own green. */

#ifndef MINI_WORLD_TUFT_MODELS_H
#define MINI_WORLD_TUFT_MODELS_H

#include <cstddef>
#include <vector>

#include "../model/model_mesh.h"

inline constexpr size_t BLADE_TUFTS = 3;
inline constexpr size_t SHRUB_TUFTS = 2;
inline constexpr size_t TUFT_SHAPES = BLADE_TUFTS + SHRUB_TUFTS;

/* A tuft stands at the origin, in tiles; its blades or leaves take the primary paintwork, darker at their feet than their tips. The shrubs come after the blades. */
std::vector<ModelMesh> BuildTuftModels();

#endif /* MINI_WORLD_TUFT_MODELS_H */
