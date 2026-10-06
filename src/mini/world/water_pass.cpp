/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file water_pass.cpp The water's surface, drawn over a snapshot of the solid world it reflects the sky over and lets the ground show through. */

#include "../../stdafx.h"
#include "water_pass.h"

#include <array>

#include "../../safeguards.h"

static constexpr std::array<const char *, 2> VERTEX_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/water.vert",
};
static constexpr std::array<const char *, 6> FRAGMENT_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/common.glsl",
	"mini_ui/shaders/sky.glsl",
	"mini_ui/shaders/shadow.glsl",
	"mini_ui/shaders/lighting.glsl",
	"mini_ui/shaders/water.frag",
};

WaterPass::WaterPass(const WorldTextures &textures, TerrainField &field) : textures(textures), field(field), program(VERTEX_SOURCES, FRAGMENT_SOURCES)
{
}

void WaterPass::Reload()
{
	this->program.Reload();
}

void WaterPass::Draw(const SceneView &view)
{
	if (!this->program.Ready()) return;
	WorldTextures::BindSamplers(this->program);
	this->textures.Bind();
	this->field.DrawWater(view);
}

void WaterPass::Release()
{
	this->program.Release();
}
