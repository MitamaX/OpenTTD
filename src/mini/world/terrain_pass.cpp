/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file terrain_pass.cpp The map's ground, shaded from the world's texels and casting the sun's shadows. */

#include "../../stdafx.h"
#include "terrain_pass.h"

#include <array>

#include "../../settings_type.h"
#include "../core/tones.h"
#include "../core/tuning.h"
#include "../gpu/frame_capture.h"
#include "../gpu/gl_api.h"
#include "shadow_map.h"

#include "../../safeguards.h"

static constexpr std::array<const char *, 2> VERTEX_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/terrain.vert",
};
static constexpr std::array<const char *, 12> FRAGMENT_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/noise.glsl",
	"mini_ui/shaders/common.glsl",
	"mini_ui/shaders/detail.glsl",
	"mini_ui/shaders/sky.glsl",
	"mini_ui/shaders/cloud_field.glsl",
	"mini_ui/shaders/shadow.glsl",
	"mini_ui/shaders/lighting.glsl",
	"mini_ui/shaders/overlay.glsl",
	"mini_ui/shaders/network.glsl",
	"mini_ui/shaders/flora.glsl",
	"mini_ui/shaders/terrain.frag",
};

TerrainPass::TerrainPass(const WorldTextures &textures, TerrainField &field) :
	textures(textures), field(field), program(VERTEX_SOURCES, FRAGMENT_SOURCES), caster(CasterProgram(VERTEX_SOURCES))
{
}

void TerrainPass::Reload()
{
	this->program.Reload();
	this->caster.Reload();
}

void TerrainPass::Cast(const ShadowView &view)
{
	if (!this->caster.Ready()) return;
	this->caster.Use();
	this->field.DrawGround(view.camera, view.frustum);
}

void TerrainPass::Draw(const SceneView &view)
{
	if (!this->program.Ready()) return;
	this->Configure();
	this->textures.Bind();
	this->detail.Bind();
	this->field.DrawGround(view, view.frustum);
}

void TerrainPass::Configure() const
{
	this->program.Use();
	WorldTextures::BindSamplers(this->program);
	GroundDetail::BindSamplers(this->program);
	glUniform1i(this->program.Uniform("u_landscape"), to_underlying(_settings_game.game_creation.landscape));
	bool guides = !_frame_capture.HidesGuides();
	glUniform1f(this->program.Uniform("u_contour"), guides ? ChannelShare(_tuning.contour_alpha) : 0.0f);
	glUniform1f(this->program.Uniform("u_grid"), guides ? ChannelShare(_tuning.grid_alpha) : 0.0f);
	UploadOverlay(this->program);
}

void TerrainPass::Release()
{
	this->program.Release();
	this->caster.Release();
	this->detail.Release();
}
