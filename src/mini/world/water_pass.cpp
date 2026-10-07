/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file water_pass.cpp The water's surface, drawn over a snapshot of the solid world it reflects the sky over and lets the ground show through. */

#include "../../stdafx.h"
#include "water_pass.h"

#include <algorithm>
#include <array>

#include "../gpu/gl_api.h"
#include "vehicle_pass.h"

#include "../../safeguards.h"

static constexpr std::array<const char *, 2> VERTEX_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/water.vert",
};
static constexpr std::array<const char *, 9> FRAGMENT_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/noise.glsl",
	"mini_ui/shaders/common.glsl",
	"mini_ui/shaders/climate.glsl",
	"mini_ui/shaders/sky.glsl",
	"mini_ui/shaders/cloud_field.glsl",
	"mini_ui/shaders/shadow.glsl",
	"mini_ui/shaders/lighting.glsl",
	"mini_ui/shaders/water.frag",
};

/* A wake trails this many ship lengths behind a ship at its top speed, and shows only where a tile spans enough pixels for its foam to read. */
static constexpr double WAKE_LENGTHS = 5.0;
static constexpr double WAKE_PIXELS = 5.0;

static double WakeReach(const ShipWake &wake)
{
	return wake.half_length * (1.0 + 2.0 * WAKE_LENGTHS * wake.pace);
}

static std::array<float, 4> Floats(double x, double y, double z, double w)
{
	return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(w)};
}

WaterPass::WaterPass(const WorldTextures &textures, TerrainField &field, const VehiclePass &vehicles) : textures(textures), field(field), vehicles(vehicles), program(VERTEX_SOURCES, FRAGMENT_SOURCES)
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
	this->UploadWakes(view);
	this->textures.Bind();
	this->field.DrawWater(view);
}

/* Each wake goes up as its hull's middle and heading, then its half length, half beam, pace and how far behind it reaches. */
void WaterPass::UploadWakes(const SceneView &view)
{
	this->wakes.clear();
	for (const ShipWake &wake : this->vehicles.Wakes()) {
		double reach = WakeReach(wake);
		Vec3 around = {reach, reach, reach};
		if (!view.Sees(wake.centre - around, wake.centre + around) || view.NearestTilePixels(wake.centre - around, wake.centre + around) < WAKE_PIXELS) continue;
		this->wakes.push_back(&wake);
	}
	auto nearness = [&view](const ShipWake *wake) { return Length(view.eye - wake->centre); };
	size_t shown = std::min<size_t>(this->wakes.size(), MOST_WAKES);
	std::ranges::partial_sort(this->wakes, this->wakes.begin() + shown, {}, nearness);

	std::array<std::array<float, 4>, MOST_WAKES> hulls{};
	std::array<std::array<float, 4>, MOST_WAKES> shapes{};
	for (size_t index = 0; index < shown; index++) {
		const ShipWake &wake = *this->wakes[index];
		hulls[index] = Floats(wake.centre.x, wake.centre.y, wake.heading.x, wake.heading.y);
		shapes[index] = Floats(wake.half_length, wake.half_beam, wake.pace, WakeReach(wake));
	}
	glUniform4fv(this->program.Uniform("u_wake_hulls"), MOST_WAKES, hulls.front().data());
	glUniform4fv(this->program.Uniform("u_wake_shapes"), MOST_WAKES, shapes.front().data());
	glUniform1i(this->program.Uniform("u_wakes"), static_cast<GLint>(shown));
}

void WaterPass::Release()
{
	this->program.Release();
}
