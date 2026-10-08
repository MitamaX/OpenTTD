/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file forest_pass.h The map's trees, each a copy of a low poly model swaying in the wind, lit by the sun and casting its shadow. */

#ifndef MINI_WORLD_FOREST_PASS_H
#define MINI_WORLD_FOREST_PASS_H

#include <vector>

#include "../gpu/instanced_meshes.h"
#include "forest_field.h"
#include "shader_program.h"
#include "world_pass.h"
#include "world_textures.h"

class ForestPass final : public WorldPass {
public:
	explicit ForestPass(const WorldTextures &textures);

	std::string_view Name() const override { return "forest"; }
	std::vector<ShaderProgram *> Programs() override;
	void Load() override;
	void Sync(const WorldChanges &changes) override;
	void Prepare(const SceneView &view) override;
	void Cast(const ShadowView &view) override;
	void Draw(const SceneView &view) override;
	void Release() override;

private:
	template <class ChooseMesh>
	void DrawTrees(const ShaderProgram &program, const SceneView &camera, const Frustum &frustum, TreeDetail coarsest, ChooseMesh choose_mesh);

	const WorldTextures &textures;
	ForestField forest;
	InstancedMeshes models;
	TreeBatch batch;
	ShaderProgram program;
	ShaderProgram caster;
};

#endif /* MINI_WORLD_FOREST_PASS_H */
