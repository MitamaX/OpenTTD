/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file signal_models.h Railway signals as low poly models: colour light signals and semaphores, each showing stop or clear. */

#ifndef MINI_WORLD_SIGNAL_MODELS_H
#define MINI_WORLD_SIGNAL_MODELS_H

#include <array>
#include <cstddef>
#include <vector>

#include "../../signal_type.h"
#include "../gpu/instanced_meshes.h"
#include "../model/model_mesh.h"

/* Where a signal stands in render space and the bearing it faces, toward the trains it speaks to. */
struct SignalInstance {
	float x;
	float y;
	float z;
	float facing;
};

inline constexpr std::array<VertexAttribute, 2> SIGNAL_INSTANCE_LAYOUT = {{
	{3, 3, AttributeType::Float, offsetof(SignalInstance, x)},
	{4, 1, AttributeType::Float, offsetof(SignalInstance, facing)},
}};

using SignalBatch = InstanceBatch<SignalInstance>;

inline constexpr size_t SIGNAL_STATES = 2;
inline constexpr size_t SIGNAL_MODELS = 2 * SIGNAL_STATES;

constexpr size_t SignalModelIndex(SignalVariant variant, SignalState state)
{
	return static_cast<size_t>(variant) * SIGNAL_STATES + static_cast<size_t>(state);
}

/* Models stand on their foot at the origin facing along x, in tiles; listed by variant, then by state. */
std::vector<ModelMesh> BuildSignalModels();

#endif /* MINI_WORLD_SIGNAL_MODELS_H */
