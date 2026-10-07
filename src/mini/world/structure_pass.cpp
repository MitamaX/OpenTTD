/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file structure_pass.cpp The map's buildings in 3D: houses, industries, objects, depots and the buildings of stations, lit and casting shadows. */

#include "../../stdafx.h"
#include "structure_pass.h"

#include <array>
#include <limits>

#include "../../landscape.h"
#include "../../settings_type.h"
#include "../gpu/gl_api.h"
#include "shadow_map.h"

#include "../../safeguards.h"

static constexpr std::array<const char *, 2> VERTEX_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/structure.vert",
};
static constexpr std::array<const char *, 9> FRAGMENT_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/common.glsl",
	"mini_ui/shaders/sky.glsl",
	"mini_ui/shaders/clouds.glsl",
	"mini_ui/shaders/shadow.glsl",
	"mini_ui/shaders/lighting.glsl",
	"mini_ui/shaders/overlay.glsl",
	"mini_ui/shaders/structure.glsl",
	"mini_ui/shaders/structure.frag",
};
static constexpr std::array<const char *, 5> CASTER_FRAGMENT_SOURCES = {
	"mini_ui/shaders/caster.glsl",
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/common.glsl",
	"mini_ui/shaders/structure.glsl",
	"mini_ui/shaders/structure_caster.frag",
};

StructurePass::StructurePass() : program(VERTEX_SOURCES, FRAGMENT_SOURCES), caster(CasterProgram(VERTEX_SOURCES, CASTER_FRAGMENT_SOURCES))
{
}

void StructurePass::Reload()
{
	this->program.Reload();
	this->caster.Reload();
	this->plumes.Reload();
}

/* Roofs above the snow line wear snow, on maps that have one. */
void StructurePass::Prepare(const SceneView &view)
{
	bool snowy = _settings_game.game_creation.landscape == LandscapeType::Arctic;
	this->snow_level = snowy ? GetSnowLine() * LevelRise() : std::numeric_limits<float>::max();
	this->field.Prepare(view);
}

void StructurePass::Sync(const WorldChanges &changes)
{
	this->field.Sync(changes);
}

void StructurePass::Cast(const ShadowView &view)
{
	if (!this->caster.Ready()) return;
	this->field.Gather(view.camera, view.frustum, this->shown);
	this->caster.Use();
	this->DrawChunks();
}

void StructurePass::Draw(const SceneView &view)
{
	if (!this->program.Ready()) return;
	this->field.Gather(view, view.frustum, this->shown);
	this->program.Use();
	UploadOverlay(this->program);
	glUniform2f(this->program.Uniform("u_fade"), static_cast<float>(STRUCTURE_FADE_START), static_cast<float>(STRUCTURE_FADE_END));
	glUniform1f(this->program.Uniform("u_snow_level"), static_cast<float>(this->snow_level));
	this->DrawChunks();

	this->plumes.Begin();
	for (const StructureChunk *chunk : this->shown) this->plumes.Add(view, chunk->vents);
	this->plumes.Draw();
}

void StructurePass::DrawChunks() const
{
	for (const StructureChunk *chunk : this->shown) chunk->mesh.Draw();
}

void StructurePass::Release()
{
	this->field.Release();
	this->plumes.Release();
	this->program.Release();
	this->caster.Release();
}
