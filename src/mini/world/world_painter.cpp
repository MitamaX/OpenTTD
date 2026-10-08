/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file world_painter.cpp The map's 3D world: its passes drawn into a target of its own, then laid into RmlUi's layer under the map element. */

#include "../../stdafx.h"
#include "world_painter.h"

#include <algorithm>
#include <iterator>
#include <limits>

#include "../../settings_type.h"
#include "../core/camera.h"
#include "../core/canvas.h"
#include "../core/ground_trace.h"
#include "../gpu/frame_profile.h"
#include "../gpu/gl_api.h"
#include "../map/way_bends.h"
#include "../map/way_course.h"
#include "build_slice.h"
#include "farm_models.h"
#include "farmsteads.h"
#include "figure_models.h"
#include "forest_pass.h"
#include "ground_cover.h"
#include "network_pass.h"
#include "scatter_pass.h"
#include "smoke_pass.h"
#include "street_walkers.h"
#include "structure_pass.h"
#include "terrain_pass.h"
#include "tuft_models.h"
#include "vehicle_pass.h"
#include "water_pass.h"

#include "../../safeguards.h"

static constexpr std::array<const char *, to_underlying(ChangeKind::End)> CHANGE_COUNTERS = {"shape_areas", "cover_areas", "flora_areas", "water_areas", "ways_areas", "relief_areas"};
static constexpr GLint REQUIRED_MAJOR = 3;
static constexpr GLint REQUIRED_MINOR = 3;

/* People show where a tile spans enough pixels to make them out and cast shadows only close up; tufts show only closer still, too small to shadow;
 * the countryside's farmsteads and bales show from further off, its fences and walls only once they stand more than a hairline tall. */
static constexpr ScatterLook WALKER_LOOK = {"walkers", BuildFigureModels, FIGURE_POSES, 26.0, 34.0, 48.0};
static constexpr ScatterLook TUFT_LOOK = {"tufts", BuildTuftModels, TUFT_SHAPES, 40.0, 56.0, std::numeric_limits<double>::infinity()};
static constexpr ScatterLook FARM_LOOK = {"farms", BuildFarmModels, FARM_PIECES, 10.0, 16.0, 22.0};
static constexpr ScatterLook BOUNDARY_LOOK = {"boundaries", BuildBoundaryModels, BOUNDARY_PIECES, 28.0, 40.0, std::numeric_limits<double>::infinity()};

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
	this->passes.push_back(std::make_unique<NetworkPass>());
	auto structures = std::make_unique<StructurePass>();
	this->structures = structures.get();
	this->passes.push_back(std::move(structures));
	auto vehicles = std::make_unique<VehiclePass>();
	this->vehicles = vehicles.get();
	this->passes.push_back(std::move(vehicles));
	this->passes.push_back(std::make_unique<ScatterPass>(std::make_unique<StreetWalkers>(), WALKER_LOOK));
	this->passes.push_back(std::make_unique<ScatterPass>(std::make_unique<GroundCover>(), TUFT_LOOK));
	this->passes.push_back(std::make_unique<ScatterPass>(std::make_unique<Farmsteads>(), FARM_LOOK));
	this->passes.push_back(std::make_unique<ScatterPass>(std::make_unique<FieldBoundaries>(), BOUNDARY_LOOK));
	this->passes.push_back(std::make_unique<ForestPass>(this->textures));
	this->passes.push_back(std::make_unique<TerrainPass>(this->textures, this->field));
	this->passes.push_back(std::make_unique<WaterPass>(this->textures, this->field, *this->vehicles));
	this->passes.push_back(std::make_unique<SmokePass>(*this->structures, *this->vehicles));
	for (const auto &pass : this->passes) std::ranges::copy(pass->Programs(), std::back_inserter(this->programs));
}

void WorldPainter::Reload()
{
	this->post.Reload();
	this->overlay.Reload();
	for (ShaderProgram *program : this->programs) program->Reload();
	this->warmed.reset();
}

/* Called from the mini UI's frame while the game's state holds still, so passes may read the game's map. */
void WorldPainter::Prepare()
{
	if (_world_tiles.Size().width == 0) return;
	_build_frame.Open(std::chrono::microseconds(std::chrono::seconds(1)) / std::max<int>(_settings_client.gui.refresh_rate, 1));
	ProfileScope profile("prepare", ProfileClock::Cpu);
	{
		ProfileScope bends_profile("prepare", "bends", ProfileClock::Cpu);
		_way_bends.Refresh();
	}
	SceneView view = SceneView::Of(_camera);
	for (const auto &pass : this->passes) {
		ProfileScope pass_profile("prepare", pass->Name(), ProfileClock::Cpu);
		pass->Prepare(view);
	}
}

/* RmlUi's layer is put back as it was before the world and the shapes on its ground are laid into it, so the map element's clipping still holds. */
void WorldPainter::Paint(const ShaderArea &area, ShaderLayer &layer)
{
	if (_world_tiles.Size().width == 0 || !this->Ready()) return;

	ProfileScope profile("world");
	SceneView view = SceneView::Of(_camera);
	this->Render(view);
	this->overlay.Upload(_ground_draw);
	layer.Restore();
	this->post.Present(area, layer.Size(), this->target);
	{
		ProfileScope overlay_profile("overlay");
		this->overlay.Draw(layer.Size(), this->target);
	}
	if (this->warmed != _settings_game.game_creation.landscape) this->Warm(view, layer.Size());
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
	this->overlay.Release();
	for (const auto &pass : this->passes) pass->Release();
	this->supported.reset();
	this->warmed.reset();
}

/* Every pass's program is built with the painter's own, though a pass whose program fails only goes undrawn. The context's version is asked once, as it holds while the context lasts. */
bool WorldPainter::Ready()
{
	if (!this->supported.has_value()) this->supported = GlSupportsWorld();
	if (!*this->supported) return false;
	for (ShaderProgram *program : this->programs) program->Ready();
	for (const auto &pass : this->passes) pass->Load();
	return this->post.Ready() && this->overlay.Ready();
}

static void BeginWorldState()
{
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glClearDepth(1.0);
}

/* Shadows are cast before the solid passes draw, surface passes draw over a snapshot of the solid world, and the finishing steps work on the whole. */
void WorldPainter::Render(const SceneView &view)
{
	this->SyncChanges();
	this->field.Refresh(view);
	for (const auto &pass : this->passes) {
		ProfileScope profile("refresh", pass->Name(), ProfileClock::Cpu);
		pass->Refresh(view);
	}

	BeginWorldState();
	this->scene.Upload(this->post.Jitter(view));
	this->shadows.Render(view, this->passes);
	if (!this->target.Bind(view.viewport)) return;

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	this->shadows.Bind();
	this->LayDepth(view);
	this->DrawStage(WorldStage::Solid, view);
	this->target.Snapshot();
	this->DrawStage(WorldStage::Surface, view);
	this->post.Finish(this->target, view);
}

/* After the first frame every program draws once more into a single pixel of what it draws into, with the scene, shadows and snapshot that frame left bound;
 * the next frame clears or overwrites that pixel before anything reads it. */
void WorldPainter::Warm(const SceneView &view, Dimension layer)
{
	ProfileScope profile("warm");
	this->overlay.Warm(layer, this->target);
	BeginWorldState();
	glEnable(GL_SCISSOR_TEST);
	glScissor(0, 0, 1, 1);
	this->shadows.Warm(this->passes);
	if (this->target.Bind(view.viewport)) {
		for (const auto &pass : this->passes) pass->Warm();
		this->post.Warm(this->target);
	}
	glDisable(GL_SCISSOR_TEST);
	this->warmed = _settings_game.game_creation.landscape;
}

void WorldPainter::SyncChanges()
{
	ProfileScope profile("sync");
	WorldChanges changes = _world_tiles.TakeChanges();
	for (size_t kind = 0; kind < CHANGE_COUNTERS.size(); kind++) _frame_profile.Count(CHANGE_COUNTERS[kind], changes.spans[kind].size());
	{
		ProfileScope ways_profile("sync", "ways", ProfileClock::Cpu);
		ForgetCourses(changes);
		_way_bends.Sync(changes);
	}
	{
		ProfileScope textures_profile("sync", "textures", ProfileClock::Cpu);
		this->textures.Sync(changes);
	}
	{
		ProfileScope field_profile("sync", "field", ProfileClock::Cpu);
		this->field.Sync(changes);
	}
	for (const auto &pass : this->passes) {
		ProfileScope pass_profile("sync", pass->Name(), ProfileClock::Cpu);
		pass->Sync(changes);
	}
}

void WorldPainter::LayDepth(const SceneView &view)
{
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	for (const auto &pass : this->passes) {
		ProfileScope profile("lay", pass->Name());
		pass->Lay(view);
	}
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void WorldPainter::DrawStage(WorldStage stage, const SceneView &view)
{
	for (const auto &pass : this->passes) {
		if (pass->Stage() != stage) continue;
		ProfileScope profile("draw", pass->Name());
		pass->Draw(view);
	}
}
