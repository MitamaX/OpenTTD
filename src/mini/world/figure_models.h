/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file figure_models.h Tiny low poly people in the poses of a walk, their shirts and trousers painted like a vehicle's paintwork. */

#ifndef MINI_WORLD_FIGURE_MODELS_H
#define MINI_WORLD_FIGURE_MODELS_H

#include <array>
#include <cstddef>
#include <vector>

#include "../../core/enum_type.hpp"
#include "../model/model_mesh.h"

/** A stride with the left leg forward, a step with both legs together, and a stride with the right leg forward. */
enum class FigurePose : uint8_t {
	LeftStride,
	Upright,
	RightStride,
	End,
};

inline constexpr size_t FIGURE_POSES = to_underlying(FigurePose::End);

/* One step after another runs through the poses as a walk does, standing upright between the strides. */
inline constexpr std::array<FigurePose, 4> WALK_CYCLE = {FigurePose::LeftStride, FigurePose::Upright, FigurePose::RightStride, FigurePose::Upright};

/* A figure stands at the origin facing along x, in tiles; its shirt takes the primary paintwork and its trousers the secondary. Listed by pose. */
std::vector<ModelMesh> BuildFigureModels();

#endif /* MINI_WORLD_FIGURE_MODELS_H */
