/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file forest_pass.cpp The map's trees, each a copy of a low poly model swaying in the wind, lit by the sun and casting its shadow. */

#include "../../stdafx.h"
#include "forest_pass.h"

#include <array>

#include "../gpu/gl_api.h"
#include "shadow_map.h"

#include "../../safeguards.h"

static constexpr std::array<const char *, 6> VERTEX_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/noise.glsl",
	"mini_ui/shaders/common.glsl",
	"mini_ui/shaders/climate.glsl",
	"mini_ui/shaders/flora.glsl",
	"mini_ui/shaders/tree.vert",
};
static constexpr std::array<const char *, 8> FRAGMENT_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/noise.glsl",
	"mini_ui/shaders/sky.glsl",
	"mini_ui/shaders/cloud_field.glsl",
	"mini_ui/shaders/shadow.glsl",
	"mini_ui/shaders/lighting.glsl",
	"mini_ui/shaders/flora.glsl",
	"mini_ui/shaders/tree.frag",
};
static constexpr double TREE_CASTER_WIDTH = 0.2;
/* Near trees cast with simpler meshes and distant ones cast none: their shadows would only speckle the ground, which the forest tint already shades. */
static constexpr TreeDetail COARSEST_CAST = TreeDetail::Simple;
static constexpr TreeDetail COARSEST_SHOWN = TreeDetail::Crude;

static size_t DrawnMesh(size_t mesh)
{
	return mesh;
}

ForestPass::ForestPass(const WorldTextures &textures) : textures(textures), program(VERTEX_SOURCES, FRAGMENT_SOURCES), caster(CasterProgram(VERTEX_SOURCES))
{
}

std::vector<ShaderProgram *> ForestPass::Programs()
{
	return {&this->program, &this->caster};
}

void ForestPass::Load()
{
	if (this->models.Ready()) return;
	std::vector<ModelMesh> meshes = BuildTreeModels();
	this->models.Upload<ModelMesh>(meshes, MODEL_LAYOUT, TREE_INSTANCE_LAYOUT, sizeof(TreeInstance));
}

void ForestPass::Sync(const WorldChanges &changes)
{
	this->forest.Sync(changes);
}

void ForestPass::Prepare(const SceneView &view)
{
	this->forest.Refresh(view, COARSEST_SHOWN, COARSEST_CAST);
}

void ForestPass::Cast(const ShadowView &view)
{
	if (!this->caster.Ready() || !view.Resolves(TREE_CASTER_WIDTH)) return;
	this->DrawTrees(this->caster, view.camera, view.frustum, COARSEST_CAST, TreeCasterMesh);
}

void ForestPass::Draw(const SceneView &view)
{
	if (!this->program.Ready()) return;
	WorldTextures::BindSamplers(this->program);
	this->textures.Bind();
	this->DrawTrees(this->program, view, view.frustum, COARSEST_SHOWN, DrawnMesh);
}

void ForestPass::WarmCast()
{
	if (!this->caster.Ready()) return;
	this->batch.Clear(TREE_DRAWN_MESHES);
	this->batch.Add(0, TreeInstance{});
	this->DrawBatch(this->caster, TreeCasterMesh);
}

void ForestPass::Warm()
{
	if (!this->program.Ready()) return;
	WorldTextures::BindSamplers(this->program);
	this->textures.Bind();
	this->batch.Clear(TREE_DRAWN_MESHES);
	this->batch.Add(0, TreeInstance{});
	this->DrawBatch(this->program, DrawnMesh);
}

template <class ChooseMesh>
void ForestPass::DrawTrees(const ShaderProgram &program, const SceneView &camera, const Frustum &frustum, TreeDetail coarsest, ChooseMesh choose_mesh)
{
	this->batch.Clear(TREE_DRAWN_MESHES);
	this->forest.Gather(camera, frustum, coarsest, this->batch);
	this->DrawBatch(program, choose_mesh);
}

/* Each mesh's copies learn which detail they were gathered for. */
template <class ChooseMesh>
void ForestPass::DrawBatch(const ShaderProgram &program, ChooseMesh choose_mesh)
{
	program.Use();
	int detail_uniform = program.Uniform("u_detail");
	this->batch.Draw(this->models, [&](size_t mesh) {
		glUniform1i(detail_uniform, static_cast<GLint>(TreeDetailOf(mesh)));
		return choose_mesh(mesh);
	});
}

void ForestPass::Release()
{
	this->forest.Release();
	this->models.Release();
	this->program.Release();
	this->caster.Release();
}
