/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file smoke_plumes.cpp Smoke rising from the stacks of industries in low poly puffs that swell and thin as the wind carries them off. */

#include "../../stdafx.h"
#include "smoke_plumes.h"

#include <array>
#include <vector>

#include "../core/seed.h"
#include "../model/model_shapes.h"

#include "../../safeguards.h"

static constexpr std::array<const char *, 2> VERTEX_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/smoke.vert",
};
static constexpr std::array<const char *, 6> FRAGMENT_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/sky.glsl",
	"mini_ui/shaders/shadow.glsl",
	"mini_ui/shaders/lighting.glsl",
	"mini_ui/shaders/overlay.glsl",
	"mini_ui/shaders/smoke.frag",
};
static constexpr int PUFF_SUBDIVISIONS = 1;
static constexpr double PUFF_LUMPINESS = 0.12;
static constexpr uint32_t PUFF_SEED = 0x50FF;
static constexpr uint32_t SMOKE_TONE = 0xE4E2DE;
static constexpr int PUFFS_PER_PLUME = 8;
static constexpr double PLUME_PIXELS = 6.0;

SmokePlumes::SmokePlumes() : program(VERTEX_SOURCES, FRAGMENT_SOURCES)
{
}

void SmokePlumes::Begin()
{
	this->batch.Clear(1);
}

/* Each plume's puffs start evenly through their rise, the whole plume shifted by where its vent stands. */
void SmokePlumes::Add(const SceneView &view, std::span<const SmokeVent> vents)
{
	for (const SmokeVent &vent : vents) {
		if (view.TilePixelsAt(Length(view.eye - vent.at)) < PLUME_PIXELS) continue;
		float offset = SeedShare(Hash32(static_cast<uint32_t>(vent.at.x * 64.0) ^ Hash32(static_cast<uint32_t>(vent.at.y * 64.0))), 0, SeedDice::SHARE_BITS);
		for (int puff = 0; puff < PUFFS_PER_PLUME; puff++) {
			float phase = (puff + offset) / PUFFS_PER_PLUME;
			this->batch.Add(0, PuffInstance{static_cast<float>(vent.at.x), static_cast<float>(vent.at.y), static_cast<float>(vent.at.z), static_cast<float>(vent.radius), phase});
		}
	}
}

void SmokePlumes::Draw()
{
	if (!this->program.Ready()) return;
	if (!this->puff.Ready()) {
		std::vector<ModelMesh> meshes = {Icosphere(PUFF_SUBDIVISIONS).Displace(PUFF_LUMPINESS, PUFF_SEED).Facet().Round({0.0, 0.0, 0.0}, 0.6).Paint(SMOKE_TONE)};
		this->puff.Upload<ModelMesh>(meshes, MODEL_LAYOUT, PUFF_INSTANCE_LAYOUT, sizeof(PuffInstance));
	}
	this->program.Use();
	UploadOverlay(this->program);
	this->batch.Draw(this->puff, [](size_t mesh) { return mesh; });
}

void SmokePlumes::Release()
{
	this->puff.Release();
	this->program.Release();
}
