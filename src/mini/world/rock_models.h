/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rock_models.h Stones lying on open land: boulders, slabs, blocks, wedges, split boulders and heaps of rubble, hewn into facets and painted like a vehicle's paintwork. */

#ifndef MINI_WORLD_ROCK_MODELS_H
#define MINI_WORLD_ROCK_MODELS_H

#include <algorithm>
#include <cstddef>
#include <vector>

#include "../../core/enum_type.hpp"
#include "../core/camera.h"
#include "../core/seed.h"
#include "../model/model_mesh.h"

enum class RockModel : uint8_t {
	Boulder,
	Slab,
	Block,
	Wedge,
	Split,
	Rubble,
	Ledge,
	Buttress,
	End,
};

inline constexpr size_t ROCK_MODELS = to_underlying(RockModel::End);

struct RockFit {
	MapVector reach;
	double follow;
	SeedRange radius;

	double Farthest() const { return std::max(this->reach.x, this->reach.y); }
};

RockFit FitOf(RockModel model);

std::vector<ModelMesh> BuildRockModels();

#endif /* MINI_WORLD_ROCK_MODELS_H */
