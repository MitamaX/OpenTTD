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

#include "../gpu/gl_api.h"
#include "laid_depth.h"
#include "shadow_map.h"

#include "../../safeguards.h"

static constexpr std::array<const char *, 2> VERTEX_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/structure.vert",
};
static constexpr std::array<const char *, 15> FRAGMENT_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/noise.glsl",
	"mini_ui/shaders/common.glsl",
	"mini_ui/shaders/sky.glsl",
	"mini_ui/shaders/cloud_field.glsl",
	"mini_ui/shaders/clouds.glsl",
	"mini_ui/shaders/shadow.glsl",
	"mini_ui/shaders/lighting.glsl",
	"mini_ui/shaders/overlay.glsl",
	"mini_ui/shaders/cover.glsl",
	"mini_ui/shaders/structure.glsl",
	"mini_ui/shaders/pane.glsl",
	"mini_ui/shaders/glazing.glsl",
	"mini_ui/shaders/structure_lit.glsl",
	"mini_ui/shaders/structure.frag",
};
static constexpr std::array<const char *, 18> FACADE_FRAGMENT_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/noise.glsl",
	"mini_ui/shaders/common.glsl",
	"mini_ui/shaders/sky.glsl",
	"mini_ui/shaders/cloud_field.glsl",
	"mini_ui/shaders/clouds.glsl",
	"mini_ui/shaders/shadow.glsl",
	"mini_ui/shaders/lighting.glsl",
	"mini_ui/shaders/overlay.glsl",
	"mini_ui/shaders/cover.glsl",
	"mini_ui/shaders/structure.glsl",
	"mini_ui/shaders/pane.glsl",
	"mini_ui/shaders/glazing.glsl",
	"mini_ui/shaders/bays.glsl",
	"mini_ui/shaders/rooms.glsl",
	"mini_ui/shaders/facade.glsl",
	"mini_ui/shaders/structure_lit.glsl",
	"mini_ui/shaders/structure_facade.frag",
};
static constexpr std::array<const char *, 5> DEPTH_FRAGMENT_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/noise.glsl",
	"mini_ui/shaders/common.glsl",
	"mini_ui/shaders/cover.glsl",
	"mini_ui/shaders/structure_depth.frag",
};
static constexpr std::array<const char *, 6> OPEN_CASTER_FRAGMENT_SOURCES = {
	"mini_ui/shaders/caster.glsl",
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/noise.glsl",
	"mini_ui/shaders/common.glsl",
	"mini_ui/shaders/cover.glsl",
	"mini_ui/shaders/structure_caster.frag",
};

StructurePass::StructurePass(const WorldTextures &textures) :
	textures(textures), program(VERTEX_SOURCES, FRAGMENT_SOURCES), facades(VERTEX_SOURCES, FACADE_FRAGMENT_SOURCES), depth(VERTEX_SOURCES, DEPTH_FRAGMENT_SOURCES), caster(CasterProgram(VERTEX_SOURCES)), open_caster(CasterProgram(VERTEX_SOURCES, OPEN_CASTER_FRAGMENT_SOURCES))
{
}

std::vector<ShaderProgram *> StructurePass::Programs()
{
	return {&this->program, &this->facades, &this->depth, &this->caster, &this->open_caster};
}

void StructurePass::Prepare(const SceneView &view)
{
	this->field.Prepare(view);
}

void StructurePass::Sync(const WorldChanges &changes)
{
	this->field.Sync(changes);
}

/* Solid claddings cast with a program that only lays depth, so the GPU may lay it as fast as it can; open ones cut their gaps out of their shadows. */
void StructurePass::Cast(const ShadowView &view)
{
	if (!this->caster.Ready() || !this->open_caster.Ready()) return;
	this->field.Gather(view.camera, view.frustum, this->shown);
	this->caster.Use();
	this->DrawGroups(CladdingGroup::Facade, CladdingGroup::Solid);
	this->open_caster.Use();
	this->DrawGroups(CladdingGroup::Open, CladdingGroup::Open);
}

void StructurePass::Lay(const SceneView &view)
{
	if (!this->Shadeable()) return;
	this->field.Gather(view, view.frustum, this->shown);
	this->UseDepth();
	this->DrawGroups(CladdingGroup::Facade, CladdingGroup::Solid);
}

void StructurePass::Draw([[maybe_unused]] const SceneView &view)
{
	if (!this->Shadeable()) return;
	this->DrawLaid(this->facades, CladdingGroup::Facade);
	this->DrawLaid(this->program, CladdingGroup::Solid);
}

void StructurePass::DrawOpen([[maybe_unused]] const SceneView &view)
{
	if (!this->Shadeable()) return;
	this->UseLit(this->program);
	this->DrawGroups(CladdingGroup::Open, CladdingGroup::Open);
}

void StructurePass::WarmCast()
{
	for (ShaderProgram *caster : {&this->caster, &this->open_caster}) {
		if (!caster->Ready()) continue;
		caster->Use();
		DrawBlankTriangle(STRUCTURE_LAYOUT, sizeof(StructureVertex));
	}
}

void StructurePass::Warm()
{
	if (!this->Shadeable()) return;
	this->UseDepth();
	DrawBlankTriangle(STRUCTURE_LAYOUT, sizeof(StructureVertex));
	for (const ShaderProgram *lit : {&this->program, &this->facades}) {
		this->UseLit(*lit);
		DrawBlankTriangle(STRUCTURE_LAYOUT, sizeof(StructureVertex));
	}
}

bool StructurePass::Shadeable()
{
	return this->program.Ready() && this->facades.Ready() && this->depth.Ready();
}

static void UploadFade(const ShaderProgram &program)
{
	glUniform2f(program.Uniform("u_fade"), static_cast<float>(STRUCTURE_FADE_START), static_cast<float>(STRUCTURE_FADE_END));
}

void StructurePass::UseDepth() const
{
	this->depth.Use();
	UploadFade(this->depth);
}

/* Panes read the ground of the street before them from the world's textures. */
void StructurePass::UseLit(const ShaderProgram &lit) const
{
	WorldTextures::BindSamplers(lit);
	this->textures.Bind();
	UploadOverlay(lit);
	UploadFade(lit);
}

void StructurePass::DrawLaid(const ShaderProgram &lit, CladdingGroup group) const
{
	this->UseLit(lit);
	LaidDepthScope laid(lit);
	this->DrawGroups(group, group);
}

void StructurePass::DrawGroups(CladdingGroup first, CladdingGroup last) const
{
	for (const StructureChunk *chunk : this->shown) {
		int start = chunk->group_starts[to_underlying(first)];
		int end = chunk->group_starts[to_underlying(last) + 1];
		chunk->mesh.Draw(start, end - start);
	}
}

void StructurePass::Release()
{
	this->field.Release();
	this->program.Release();
	this->facades.Release();
	this->depth.Release();
	this->caster.Release();
	this->open_caster.Release();
}
