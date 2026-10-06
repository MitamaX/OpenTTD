/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file world_painter.cpp The map's 3D world: its passes drawn into a target of its own, then laid into RmlUi's layer under the map element. */

#include "../../stdafx.h"
#include "world_painter.h"

#include <array>

#include "../core/camera.h"
#include "../gpu/gl_api.h"
#include "../gpu/gl_state.h"
#include "forest_pass.h"
#include "terrain_pass.h"
#include "water_pass.h"

#include "../../safeguards.h"

static constexpr std::array<const char *, 1> SCREEN_SOURCES = {
	"mini_ui/shaders/screen.vert",
};
static constexpr std::array<const char *, 3> COMPOSITE_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/sky.glsl",
	"mini_ui/shaders/composite.frag",
};
static constexpr int QUAD_CORNERS = 4;
static constexpr GLint REQUIRED_MAJOR = 3;
static constexpr GLint REQUIRED_MINOR = 3;

/** The world target's textures, as the composite samples them. */
enum CompositeUnit : uint8_t {
	COLOUR_UNIT,
	DEPTH_UNIT,
};

WorldPainter _world_painter;

/* Every world shader is written against GL 3.3, which some contexts the game accepts fall short of. */
static bool GlSupportsWorld()
{
	GLint major = 0;
	GLint minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	return major > REQUIRED_MAJOR || (major == REQUIRED_MAJOR && minor >= REQUIRED_MINOR);
}

static void BindTexture(uint unit, uint32_t texture)
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, texture);
}

WorldPainter::WorldPainter() : composite(SCREEN_SOURCES, COMPOSITE_SOURCES)
{
	this->passes.push_back(std::make_unique<TerrainPass>(this->textures, this->field));
	this->passes.push_back(std::make_unique<ForestPass>(this->textures));
	this->passes.push_back(std::make_unique<WaterPass>(this->textures, this->field));
}

void WorldPainter::Reload()
{
	this->composite.Reload();
	for (const auto &pass : this->passes) pass->Reload();
}

/* RmlUi's layer is put back as it was before the world is laid into it, so the map element's clipping still holds. */
void WorldPainter::Paint(const ShaderArea &area)
{
	if (_world_tiles.Size().width == 0 || !this->Ready()) return;

	GlStateScope borrowed;
	this->Render(SceneView::Of(_camera));
	borrowed.Restore();
	this->Composite(area);
}

void WorldPainter::Release()
{
	this->target.Release();
	this->textures.Release();
	this->field.Release();
	this->shadows.Release();
	this->scene.Release();
	this->composite.Release();
	for (const auto &pass : this->passes) pass->Release();
	if (this->quad != 0) glDeleteVertexArrays(1, &this->quad);
	this->quad = 0;
}

bool WorldPainter::Ready()
{
	if (!GlSupportsWorld() || !this->composite.Ready()) return false;
	if (this->quad == 0) glGenVertexArrays(1, &this->quad);
	return true;
}

/* Shadows are cast before the solid passes draw, and surface passes draw over a snapshot of the solid world. */
void WorldPainter::Render(const SceneView &view)
{
	WorldChanges changes = _world_tiles.TakeChanges();
	this->textures.Sync(changes);
	this->field.Sync(changes);
	for (const auto &pass : this->passes) pass->Sync(changes);

	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glClearDepth(1.0);
	this->scene.Upload(view);
	this->shadows.Render(view, this->passes);
	if (!this->target.Bind(view.viewport)) return;

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	this->shadows.Bind();
	this->DrawStage(WorldStage::Solid, view);
	this->target.Snapshot();
	this->DrawStage(WorldStage::Surface, view);
}

void WorldPainter::DrawStage(WorldStage stage, const SceneView &view)
{
	for (const auto &pass : this->passes) {
		if (pass->Stage() == stage) pass->Draw(view);
	}
}

/* The quad covers the element in RmlUi's pixels; the viewport is whichever layer RmlUi is drawing into. */
void WorldPainter::Composite(const ShaderArea &area)
{
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	this->composite.Use();
	glUniform4f(this->composite.Uniform("u_area"), area.left, area.top, area.right, area.bottom);
	glUniform2f(this->composite.Uniform("u_viewport"), static_cast<float>(viewport[2]), static_cast<float>(viewport[3]));
	this->composite.BindSampler("u_colour", COLOUR_UNIT);
	this->composite.BindSampler("u_depth", DEPTH_UNIT);
	BindTexture(COLOUR_UNIT, this->target.Colour());
	BindTexture(DEPTH_UNIT, this->target.Depth());
	glBindVertexArray(this->quad);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, QUAD_CORNERS);
}
