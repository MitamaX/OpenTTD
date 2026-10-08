/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_pass.cpp The map's trains, road vehicles, ships and aircraft in 3D, moving every frame, lit and casting shadows. */

#include "../../stdafx.h"
#include "vehicle_pass.h"

#include <algorithm>

#include "../gpu/gl_api.h"
#include "../map/map_overlay.h"
#include "shadow_map.h"

#include "../../safeguards.h"

/* Vehicles are whole where a tile spans this many pixels, bare masses below it, and cast shadows only where they would read. */
static constexpr double FULL_DETAIL_PIXELS = 26.0;
static constexpr double CAST_PIXELS = 7.0;
static constexpr double VEHICLE_CASTER_WIDTH = 0.15;
/* A click this many pixels off a vehicle still picks it. */
static constexpr double PICK_SLACK_PIXELS = 5.0;
/* Solids never fade out, as vehicles have nothing on the ground standing in for them. */
static constexpr float NEVER_FADES = -1.0f;

/* In a view from far out, vehicles grow until a tile spans this many pixels at their scale: trains and road vehicles only across and up, as they run nose to tail. */
static constexpr float READABLE_TILE_PIXELS = 30.0f;

struct Growth {
	float along;
	float across;
};

static constexpr Growth GROUND_GROWTH = {1.0f, 1.8f};
static constexpr Growth SHIP_GROWTH = {1.6f, 1.6f};
static constexpr Growth AIRCRAFT_GROWTH = {2.0f, 2.0f};

/* How far any model, grown its most, stands out from where its unit is placed. */
static double FurthestReach(std::span<const VehicleBounds> bounds)
{
	double most_growth = std::max({GROUND_GROWTH.along, GROUND_GROWTH.across, SHIP_GROWTH.along, SHIP_GROWTH.across, AIRCRAFT_GROWTH.along, AIRCRAFT_GROWTH.across});
	double furthest = 0.0;
	for (const VehicleBounds &box : bounds) {
		furthest = std::max(furthest, Length({std::max(-box.low.x, box.high.x), std::max(-box.low.y, box.high.y), std::max(-box.low.z, box.high.z)}));
	}
	return furthest * most_growth;
}

static Growth MostGrowthOf(VehicleLook look)
{
	if (look < VehicleLook::Ferry) return GROUND_GROWTH;
	if (look < VehicleLook::PropPlane) return SHIP_GROWTH;
	return AIRCRAFT_GROWTH;
}

static MiniLayer LayerOf(VehicleLook look)
{
	if (look < VehicleLook::Bus) return MiniLayer::Rail;
	if (look < VehicleLook::Ferry) return MiniLayer::Road;
	return MiniLayer::None;
}

VehiclePass::VehiclePass() : program(VEHICLE_VERTEX_SOURCES, VEHICLE_FRAGMENT_SOURCES), caster(CasterProgram(VEHICLE_VERTEX_SOURCES))
{
}

void VehiclePass::Reload()
{
	this->program.Reload();
	this->caster.Reload();
}

/* The models are built the first time vehicles are laid out, as the layout measures them. */
void VehiclePass::Prepare(const SceneView &view)
{
	if (this->meshes.empty()) {
		this->meshes = BuildVehicleModels();
		this->bounds = MeasureVehicleModels(this->meshes);
		this->reach = FurthestReach(this->bounds);
	}
	this->seen = view;
	this->fleet.Lay(view, this->reach);
}

void VehiclePass::Cast(const ShadowView &view)
{
	if (!this->caster.Ready() || !view.Resolves(VEHICLE_CASTER_WIDTH)) return;
	this->Gather(view.camera, view.frustum, CAST_PIXELS);
	this->caster.Use();
	this->DrawBatch(this->caster, view.camera);
}

void VehiclePass::Draw(const SceneView &view)
{
	if (!this->program.Ready()) return;
	this->Gather(view, view.frustum, 0.0);
	this->program.Use();
	UploadOverlay(this->program);
	glUniform2f(this->program.Uniform("u_fade"), NEVER_FADES * 2.0f, NEVER_FADES);
	glUniform1i(this->program.Uniform("u_own_colours"), GL_TRUE);
	this->DrawBatch(this->program, view);
}

void VehiclePass::Gather(const SceneView &camera, const Frustum &frustum, double fewest_pixels)
{
	this->batch.Clear(VEHICLE_MODELS);
	for (const PlacedUnit &unit : this->fleet.Units()) {
		Growth growth = MostGrowthOf(unit.look);
		double radius = unit.radius * std::max(growth.along, growth.across);
		Vec3 reach = {radius, radius, radius};
		if (!BoxMeets(frustum, unit.centre - reach, unit.centre + reach)) continue;
		double pixels = camera.TilePixelsAt(Length(camera.eye - unit.centre));
		if (pixels < fewest_pixels) continue;
		VehicleDetail detail = pixels >= FULL_DETAIL_PIXELS ? VehicleDetail::Full : VehicleDetail::Simple;
		this->batch.Add(VehicleModelIndex(unit.look, detail), unit.instance);
	}
}

/* Each model's copies learn which layer of the overlay they belong to. */
void VehiclePass::DrawBatch(const ShaderProgram &program, const SceneView &camera)
{
	if (this->meshes.empty()) return;
	if (!this->models.Ready()) this->models.Upload<ModelMesh>(this->meshes, MODEL_LAYOUT, VEHICLE_INSTANCE_LAYOUT, sizeof(VehicleInstance));
	int way = program.Uniform("u_way");
	int most_growth = program.Uniform("u_most_growth");
	glUniform1f(program.Uniform("u_readable_pixels"), READABLE_TILE_PIXELS);
	glUniform1f(program.Uniform("u_view_pixels"), static_cast<float>(camera.focus_pixels));
	this->batch.Draw(this->models, [way, most_growth](size_t model) {
		VehicleLook look = LookOfModel(model);
		Growth growth = MostGrowthOf(look);
		glUniform1i(way, to_underlying(LayerOf(look)));
		glUniform2f(most_growth, growth.along, growth.across);
		return model;
	});
}

std::optional<VehicleHit> VehiclePass::Pick(const Vec3 &origin, const Vec3 &direction) const
{
	std::optional<VehicleHit> nearest;
	for (const PlacedUnit &unit : this->fleet.Units()) {
		double slack = PICK_SLACK_PIXELS / this->seen.TilePixelsAt(Length(this->seen.eye - unit.centre));
		std::optional<double> distance = unit.Meets(origin, direction, slack);
		if (distance.has_value() && (!nearest.has_value() || *distance < nearest->distance)) nearest = VehicleHit{unit.vehicle, *distance};
	}
	return nearest;
}

void VehiclePass::Release()
{
	this->models.Release();
	this->program.Release();
	this->caster.Release();
}
