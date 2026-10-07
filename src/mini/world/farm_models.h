/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file farm_models.h The pieces of the countryside scattered over open land: farmhouses, barns, silos, hay bales, fences and dry stone walls. */

#ifndef MINI_WORLD_FARM_MODELS_H
#define MINI_WORLD_FARM_MODELS_H

#include <cstddef>
#include <vector>

#include "../../core/enum_type.hpp"
#include "../model/model_mesh.h"

enum class FarmPiece : uint8_t {
	House,
	Barn,
	Silo,
	RoundBale,
	BaleStack,
	Fence,
	StoneWall,
	End,
};

inline constexpr size_t FARM_PIECES = to_underlying(FarmPiece::End);

/* Each piece stands on the ground at the origin, its long side along x, in tiles; walls, bales and silos take the primary paintwork and roofs the secondary.
 * A fence or a wall runs one tile's side, centred on the origin. */
std::vector<ModelMesh> BuildFarmModels();

#endif /* MINI_WORLD_FARM_MODELS_H */
