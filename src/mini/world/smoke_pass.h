/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file smoke_pass.h Smoke and steam rising from industry stacks and steam engines in soft puffs that swell, drift off on the wind and thin away. */

#ifndef MINI_WORLD_SMOKE_PASS_H
#define MINI_WORLD_SMOKE_PASS_H

#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "../gpu/instanced_meshes.h"
#include "shader_program.h"
#include "smoke_vent.h"
#include "world_pass.h"

class StructurePass;
class VehiclePass;

/* One puff: the vent it rises from and the vent's radius, how far through its rise it starts, how the vent moves along and its own seed. */
struct PuffInstance {
	float x;
	float y;
	float z;
	float radius;
	float phase;
	float motion_x;
	float motion_y;
	float seed;
};

inline constexpr std::array<VertexAttribute, 5> PUFF_INSTANCE_LAYOUT = {{
	{3, 3, AttributeType::Float, offsetof(PuffInstance, x)},
	{4, 1, AttributeType::Float, offsetof(PuffInstance, radius)},
	{5, 1, AttributeType::Float, offsetof(PuffInstance, phase)},
	{6, 2, AttributeType::Float, offsetof(PuffInstance, motion_x)},
	{7, 1, AttributeType::Float, offsetof(PuffInstance, seed)},
}};

/* Puffs are drawn over the whole solid world, blended in from the farthest to the nearest, and show only where a vent spans enough pixels for its smoke to read. */
class SmokePass final : public WorldPass {
public:
	SmokePass(const StructurePass &structures, const VehiclePass &vehicles);

	std::string_view Name() const override { return "smoke"; }
	void Reload() override;
	void Draw(const SceneView &view) override;
	void Release() override;
	WorldStage Stage() const override { return WorldStage::Surface; }

private:
	void Add(const SceneView &view, std::span<const SmokeVent> vents);

	const StructurePass &structures;
	const VehiclePass &vehicles;
	std::vector<PuffInstance> puffs;
	InstancedMeshes card;
	InstanceBatch<PuffInstance> batch;
	ShaderProgram program;
};

#endif /* MINI_WORLD_SMOKE_PASS_H */
