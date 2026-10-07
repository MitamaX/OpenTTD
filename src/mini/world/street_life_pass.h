/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file street_life_pass.h The people walking town streets, drawn like vehicles where they are near enough to see and fading out further off. */

#ifndef MINI_WORLD_STREET_LIFE_PASS_H
#define MINI_WORLD_STREET_LIFE_PASS_H

#include <vector>

#include "shader_program.h"
#include "street_walkers.h"
#include "world_pass.h"

class StreetLifePass final : public WorldPass {
public:
	StreetLifePass();

	void Reload() override;
	void Sync(const WorldChanges &changes) override;
	void Cast(const ShadowView &view) override;
	void Draw(const SceneView &view) override;
	void Release() override;

private:
	void DrawBatch(const ShaderProgram &program);

	StreetWalkers walkers;
	std::vector<ModelMesh> meshes;
	InstancedMeshes models;
	VehicleBatch batch;
	ShaderProgram program;
	ShaderProgram caster;
};

#endif /* MINI_WORLD_STREET_LIFE_PASS_H */
