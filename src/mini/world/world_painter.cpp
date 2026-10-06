/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file world_painter.cpp The map's 3D world: its passes drawn into a target of its own, then laid into RmlUi's layer under the map element. */

#include "../../stdafx.h"
#include "world_painter.h"

#include "../core/camera.h"
#include "../core/ground_trace.h"
#include "../gpu/gl_api.h"
#include "../gpu/gl_state.h"
#include "forest_pass.h"
#include "network_pass.h"
#include "structure_pass.h"
#include "terrain_pass.h"
#include "vehicle_pass.h"
#include "water_pass.h"

#include "../../safeguards.h"

static constexpr GLint REQUIRED_MAJOR = 3;
static constexpr GLint REQUIRED_MINOR = 3;

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

WorldPainter::WorldPainter()
{
	this->passes.push_back(std::make_unique<TerrainPass>(this->textures, this->field));
	this->passes.push_back(std::make_unique<NetworkPass>());
	auto structures = std::make_unique<StructurePass>();
	this->structures = structures.get();
	this->passes.push_back(std::move(structures));
	auto vehicles = std::make_unique<VehiclePass>();
	this->vehicles = vehicles.get();
	this->passes.push_back(std::move(vehicles));
	this->passes.push_back(std::make_unique<ForestPass>(this->textures));
	this->passes.push_back(std::make_unique<WaterPass>(this->textures, this->field));
}

void WorldPainter::Reload()
{
	this->post.Reload();
	for (const auto &pass : this->passes) pass->Reload();
}

/* Called from the mini UI's frame while the game's state holds still, so passes may read the game's map. */
void WorldPainter::Prepare()
{
	if (_world_tiles.Size().width == 0) return;
	SceneView view = SceneView::Of(_camera);
	for (const auto &pass : this->passes) pass->Prepare(view);
}

/* RmlUi's layer is put back as it was before the world is laid into it, so the map element's clipping still holds. */
void WorldPainter::Paint(const ShaderArea &area)
{
	if (_world_tiles.Size().width == 0 || !this->Ready()) return;

	GlStateScope borrowed;
	this->Render(SceneView::Of(_camera));
	borrowed.Restore();
	this->post.Present(area, this->target);
}

std::optional<TileIndex> WorldPainter::BuildingAt(const Vec3 &origin, const Vec3 &direction) const
{
	std::optional<StructureHit> hit = this->structures->Pick(origin, direction);
	if (!hit.has_value()) return std::nullopt;
	if (TraceGround({origin, direction, LevelRise()}, hit->distance).has_value()) return std::nullopt;
	return hit->tile;
}

std::optional<VehicleID> WorldPainter::VehicleAt(const Vec3 &origin, const Vec3 &direction) const
{
	std::optional<VehicleHit> hit = this->vehicles->Pick(origin, direction);
	if (!hit.has_value()) return std::nullopt;
	if (TraceGround({origin, direction, LevelRise()}, hit->distance).has_value()) return std::nullopt;
	return hit->vehicle;
}

void WorldPainter::Release()
{
	this->target.Release();
	this->textures.Release();
	this->field.Release();
	this->shadows.Release();
	this->scene.Release();
	this->post.Release();
	for (const auto &pass : this->passes) pass->Release();
}

bool WorldPainter::Ready()
{
	return GlSupportsWorld() && this->post.Ready();
}

/* Shadows are cast before the solid passes draw, surface passes draw over a snapshot of the solid world, and the finishing steps work on the whole. */
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
	this->scene.Upload(this->post.Jitter(view));
	this->shadows.Render(view, this->passes);
	if (!this->target.Bind(view.viewport)) return;

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	this->shadows.Bind();
	this->DrawStage(WorldStage::Solid, view);
	this->target.Snapshot();
	this->DrawStage(WorldStage::Surface, view);
	this->post.Finish(this->target, view);
}

void WorldPainter::DrawStage(WorldStage stage, const SceneView &view)
{
	for (const auto &pass : this->passes) {
		if (pass->Stage() == stage) pass->Draw(view);
	}
}
