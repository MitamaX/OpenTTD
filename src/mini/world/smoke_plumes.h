/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file smoke_plumes.h Smoke rising from the stacks of industries in low poly puffs that swell and thin as the wind carries them off. */

#ifndef MINI_WORLD_SMOKE_PLUMES_H
#define MINI_WORLD_SMOKE_PLUMES_H

#include <array>
#include <cstddef>
#include <span>

#include "../core/space.h"
#include "../gpu/instanced_meshes.h"
#include "../model/model_mesh.h"
#include "shader_program.h"
#include "scene_view.h"

/* The mouth of a stack in render space and how wide it is. */
struct SmokeVent {
	Vec3 at;
	double radius;
};

/* One puff of a plume: the vent it rises from, the vent's radius, and how far through its rise it starts. */
struct PuffInstance {
	float x;
	float y;
	float z;
	float radius;
	float phase;
};

inline constexpr std::array<VertexAttribute, 3> PUFF_INSTANCE_LAYOUT = {{
	{3, 3, AttributeType::Float, offsetof(PuffInstance, x)},
	{4, 1, AttributeType::Float, offsetof(PuffInstance, radius)},
	{5, 1, AttributeType::Float, offsetof(PuffInstance, phase)},
}};

/* Plumes show only where a stack spans enough pixels for its smoke to read. */
class SmokePlumes {
public:
	SmokePlumes();

	void Begin();
	void Add(const SceneView &view, std::span<const SmokeVent> vents);
	void Draw();
	void Reload() { this->program.Reload(); }
	void Release();

private:
	InstancedMeshes puff;
	InstanceBatch<PuffInstance> batch;
	ShaderProgram program;
};

#endif /* MINI_WORLD_SMOKE_PLUMES_H */
