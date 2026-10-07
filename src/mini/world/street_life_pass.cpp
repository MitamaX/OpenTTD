/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file street_life_pass.cpp The people walking town streets, drawn like vehicles where they are near enough to see and fading out further off. */

#include "../../stdafx.h"
#include "street_life_pass.h"

#include "../gpu/gl_api.h"
#include "../map/map_overlay.h"
#include "figure_models.h"
#include "shadow_map.h"

#include "../../safeguards.h"

/* People show where a tile spans this many pixels, fading in over the next few, and cast shadows only close up. */
static constexpr double SHOWN_PIXELS = 26.0;
static constexpr double SOLID_PIXELS = 34.0;
static constexpr double CAST_PIXELS = 48.0;
static constexpr float NO_GROWTH = 1.0f;

StreetLifePass::StreetLifePass() : program(VEHICLE_VERTEX_SOURCES, VEHICLE_FRAGMENT_SOURCES), caster(CasterProgram(VEHICLE_VERTEX_SOURCES))
{
}

void StreetLifePass::Reload()
{
	this->program.Reload();
	this->caster.Reload();
}

void StreetLifePass::Sync(const WorldChanges &changes)
{
	this->walkers.Sync(changes);
}

void StreetLifePass::Cast(const ShadowView &view)
{
	if (!this->caster.Ready()) return;
	this->batch.Clear(FIGURE_POSES);
	this->walkers.Gather(view.camera, view.frustum, CAST_PIXELS, this->batch);
	this->caster.Use();
	this->DrawBatch(this->caster);
}

void StreetLifePass::Draw(const SceneView &view)
{
	if (!this->program.Ready()) return;
	this->batch.Clear(FIGURE_POSES);
	this->walkers.Gather(view, view.frustum, SHOWN_PIXELS, this->batch);
	this->program.Use();
	UploadOverlay(this->program);
	glUniform2f(this->program.Uniform("u_fade"), static_cast<float>(SHOWN_PIXELS), static_cast<float>(SOLID_PIXELS));
	glUniform1i(this->program.Uniform("u_own_colours"), GL_TRUE);
	glUniform1i(this->program.Uniform("u_way"), to_underlying(MiniLayer::None));
	this->DrawBatch(this->program);
}

/* People keep their true size however far off, as they fade out before they would need to grow. */
void StreetLifePass::DrawBatch(const ShaderProgram &program)
{
	if (this->meshes.empty()) this->meshes = BuildFigureModels();
	if (!this->models.Ready()) this->models.Upload<ModelMesh>(this->meshes, MODEL_LAYOUT, VEHICLE_INSTANCE_LAYOUT, sizeof(VehicleInstance));
	glUniform2f(program.Uniform("u_most_growth"), NO_GROWTH, NO_GROWTH);
	this->batch.Draw(this->models, [](size_t mesh) { return mesh; });
}

void StreetLifePass::Release()
{
	this->walkers.Release();
	this->models.Release();
	this->program.Release();
	this->caster.Release();
}
