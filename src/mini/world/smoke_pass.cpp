/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file smoke_pass.cpp Smoke and steam rising from industry stacks and steam engines in soft puffs that swell, drift off on the wind and thin away. */

#include "../../stdafx.h"
#include "smoke_pass.h"

#include <algorithm>

#include "../core/seed.h"
#include "../gpu/gl_api.h"
#include "../model/model_mesh.h"
#include "structure_pass.h"
#include "vehicle_pass.h"

#include "../../safeguards.h"

static constexpr std::array<const char *, 4> VERTEX_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/noise.glsl",
	"mini_ui/shaders/cloud_field.glsl",
	"mini_ui/shaders/smoke.vert",
};
static constexpr std::array<const char *, 7> FRAGMENT_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/noise.glsl",
	"mini_ui/shaders/sky.glsl",
	"mini_ui/shaders/cloud_field.glsl",
	"mini_ui/shaders/shadow.glsl",
	"mini_ui/shaders/lighting.glsl",
	"mini_ui/shaders/smoke.frag",
};
static constexpr int PUFFS_PER_PLUME = 14;
static constexpr double PLUME_PIXELS = 6.0;

/* A square card facing the eye, its corners a unit from its middle. */
static ModelMesh PuffCard()
{
	ModelMesh card;
	uint32_t a = card.Point({-1.0, -1.0, 0.0});
	uint32_t b = card.Point({1.0, -1.0, 0.0});
	uint32_t c = card.Point({1.0, 1.0, 0.0});
	uint32_t d = card.Point({-1.0, 1.0, 0.0});
	card.Quad(a, b, c, d);
	return card;
}

SmokePass::SmokePass(const StructurePass &structures, const VehiclePass &vehicles) :
	structures(structures), vehicles(vehicles), program(VERTEX_SOURCES, FRAGMENT_SOURCES)
{
}

std::vector<ShaderProgram *> SmokePass::Programs()
{
	return {&this->program};
}

/* Each plume's puffs start evenly through their rise, the whole plume shifted by its vent's seed. */
void SmokePass::Add(const SceneView &view, std::span<const SmokeVent> vents)
{
	for (const SmokeVent &vent : vents) {
		if (view.TilePixelsAt(Length(view.eye - vent.at)) < PLUME_PIXELS) continue;
		float offset = SeedShare(vent.seed, 0, SeedDice::SHARE_BITS);
		for (int puff = 0; puff < PUFFS_PER_PLUME; puff++) {
			this->puffs.push_back({
				static_cast<float>(vent.at.x), static_cast<float>(vent.at.y), static_cast<float>(vent.at.z), static_cast<float>(vent.radius),
				(puff + offset) / PUFFS_PER_PLUME, static_cast<float>(vent.motion.x), static_cast<float>(vent.motion.y),
				SeedShare(SubSeed(vent.seed, puff), 0, SeedDice::SHARE_BITS),
			});
		}
	}
}

/* Puffs neither hide one another nor what lies behind them from the depth test; they only darken and light what they cover, in their own alpha. */
void SmokePass::Draw(const SceneView &view)
{
	if (!this->program.Ready()) return;
	this->puffs.clear();
	for (const StructureChunk *chunk : this->structures.Shown()) this->Add(view, chunk->vents);
	this->Add(view, this->vehicles.Funnels());
	if (this->puffs.empty()) return;

	auto distance = [&](const PuffInstance &puff) { return Length(view.eye - Vec3{puff.x, puff.y, puff.z}); };
	std::ranges::sort(this->puffs, std::greater{}, distance);
	this->batch.Clear(1);
	this->batch.Add(0, this->puffs);
	if (!this->card.Ready()) {
		std::vector<ModelMesh> meshes = {PuffCard()};
		this->card.Upload<ModelMesh>(meshes, MODEL_LAYOUT, PUFF_INSTANCE_LAYOUT, sizeof(PuffInstance));
	}

	this->program.Use();
	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
	glDepthMask(GL_FALSE);
	this->batch.Draw(this->card, [](size_t mesh) { return mesh; });
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}

void SmokePass::Release()
{
	this->card.Release();
	this->program.Release();
}
