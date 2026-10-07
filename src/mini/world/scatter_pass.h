/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file scatter_pass.h Small things strewn near the eye, such as people walking town streets or tufts of grass, drawn like vehicles and fading out further off. */

#ifndef MINI_WORLD_SCATTER_PASS_H
#define MINI_WORLD_SCATTER_PASS_H

#include <memory>
#include <vector>

#include "scatter.h"
#include "shader_program.h"
#include "world_pass.h"

/* How a scatter shows: the models its copies are drawn with, and where a tile spans enough pixels for them to show, to stand solid and to cast shadows. */
struct ScatterLook {
	std::vector<ModelMesh> (*build)();
	size_t models;
	double shown_pixels;
	double solid_pixels;
	double cast_pixels;
};

class ScatterPass final : public WorldPass {
public:
	ScatterPass(std::unique_ptr<Scatter> scatter, const ScatterLook &look);

	void Reload() override;
	void Sync(const WorldChanges &changes) override;
	void Cast(const ShadowView &view) override;
	void Draw(const SceneView &view) override;
	void Release() override;

private:
	void DrawBatch(const ShaderProgram &program);

	std::unique_ptr<Scatter> scatter;
	ScatterLook look;
	std::vector<ModelMesh> meshes;
	InstancedMeshes models;
	VehicleBatch batch;
	ShaderProgram program;
	ShaderProgram caster;
};

#endif /* MINI_WORLD_SCATTER_PASS_H */
