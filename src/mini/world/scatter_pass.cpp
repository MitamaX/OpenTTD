/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file scatter_pass.cpp Small things strewn near the eye, such as people walking town streets or tufts of grass, drawn like vehicles and fading out further off. */

#include "../../stdafx.h"
#include "scatter_pass.h"

#include "../gpu/gl_api.h"
#include "../map/map_overlay.h"
#include "shadow_map.h"

#include "../../safeguards.h"

static constexpr float NO_GROWTH = 1.0f;

ScatterPass::ScatterPass(std::unique_ptr<Scatter> scatter, const ScatterLook &look) :
	scatter(std::move(scatter)), look(look), program(VEHICLE_VERTEX_SOURCES, VEHICLE_FRAGMENT_SOURCES), caster(CasterProgram(VEHICLE_VERTEX_SOURCES))
{
}

std::vector<ShaderProgram *> ScatterPass::Programs()
{
	return {&this->program, &this->caster};
}

void ScatterPass::Sync(const WorldChanges &changes)
{
	this->scatter->Sync(changes);
}

void ScatterPass::Prepare(const SceneView &view)
{
	this->scatter->Prepare(view, this->look.shown_pixels, this->look.cast_pixels);
}

void ScatterPass::Cast(const ShadowView &view)
{
	if (!this->caster.Ready()) return;
	this->batch.Clear(this->look.models);
	this->scatter->Gather(view.camera, view.frustum, this->look.cast_pixels, this->batch);
	this->caster.Use();
	this->DrawBatch(this->caster);
}

void ScatterPass::Draw(const SceneView &view)
{
	if (!this->program.Ready()) return;
	this->batch.Clear(this->look.models);
	this->scatter->Gather(view, view.frustum, this->look.shown_pixels, this->batch);
	this->program.Use();
	UploadOverlay(this->program);
	glUniform2f(this->program.Uniform("u_fade"), static_cast<float>(this->look.shown_pixels), static_cast<float>(this->look.solid_pixels));
	glUniform1i(this->program.Uniform("u_own_colours"), GL_TRUE);
	glUniform1i(this->program.Uniform("u_way"), to_underlying(MiniLayer::None));
	this->DrawBatch(this->program);
}

/* The copies keep their true size however far off, as they fade out before they would need to grow. */
void ScatterPass::DrawBatch(const ShaderProgram &program)
{
	if (this->meshes.empty()) this->meshes = this->look.build();
	if (!this->models.Ready()) this->models.Upload<ModelMesh>(this->meshes, MODEL_LAYOUT, VEHICLE_INSTANCE_LAYOUT, sizeof(VehicleInstance));
	glUniform2f(program.Uniform("u_most_growth"), NO_GROWTH, NO_GROWTH);
	this->batch.Draw(this->models, [](size_t mesh) { return mesh; });
}

void ScatterPass::Release()
{
	this->scatter->Release();
	this->models.Release();
	this->program.Release();
	this->caster.Release();
}
