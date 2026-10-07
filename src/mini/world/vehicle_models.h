/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_models.h The low poly trains, road vehicles, ships and aircraft, at each level of detail, built once in code. */

#ifndef MINI_WORLD_VEHICLE_MODELS_H
#define MINI_WORLD_VEHICLE_MODELS_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

#include "../../core/enum_type.hpp"
#include "../gpu/instanced_meshes.h"
#include "../model/model_mesh.h"

enum class VehicleLook : uint8_t {
	SteamEngine,
	DieselEngine,
	DieselStreamliner,
	ElectricEngine,
	ElectricStreamliner,
	MonorailEngine,
	MaglevEngine,
	Coach,
	SleekCoach,
	MailVan,
	BoxVan,
	Reefer,
	ArmouredVan,
	TankWagon,
	OpenWagon,
	CoveredHopper,
	FlatWagon,
	LivestockVan,
	Bus,
	BoxTruck,
	TankerTruck,
	TipperTruck,
	FlatbedTruck,
	LivestockTruck,
	Tram,
	Ferry,
	CargoShip,
	OilTanker,
	PropPlane,
	Jet,
	Helicopter,
	Rotor,
	End,
};

/** How finely a vehicle is drawn: whole near the eye, its bare masses further off. */
enum class VehicleDetail : uint8_t {
	Full,
	Simple,
	End,
};

inline constexpr size_t VEHICLE_LOOKS = to_underlying(VehicleLook::End);
inline constexpr size_t VEHICLE_DETAILS = to_underlying(VehicleDetail::End);
inline constexpr size_t VEHICLE_MODELS = VEHICLE_LOOKS * VEHICLE_DETAILS;

constexpr size_t VehicleModelIndex(VehicleLook look, VehicleDetail detail)
{
	return to_underlying(look) * VEHICLE_DETAILS + to_underlying(detail);
}

constexpr VehicleLook LookOfModel(size_t model)
{
	return static_cast<VehicleLook>(model / VEHICLE_DETAILS);
}

/** The owner's colours and the cargo carried, which models take on wherever they are painted with a paintwork tone. */
enum class Paintwork : uint8_t {
	Primary,
	Secondary,
	Cargo,
};

/* A tone the vehicle shader swaps for a paintwork colour, darkened or lightened by the shade, which runs up to twice as bright.
 * No other part of a vehicle model may be painted full red without green, full green without red, or blue alone. */
constexpr uint32_t PaintworkTone(Paintwork paintwork, double shade)
{
	uint32_t blue = static_cast<uint32_t>(shade * 127.5 + 0.5);
	switch (paintwork) {
		case Paintwork::Primary: return 0xFF0000 | blue;
		case Paintwork::Secondary: return 0x00FF00 | blue;
		default: return std::max(blue, 1u);
	}
}

/* Where a unit stands in render space, its yaw, pitch and roll, its length as a share of a full unit,
 * the paintwork colours, and in the cargo colour's last byte how full it is. */
struct VehicleInstance {
	float x;
	float y;
	float z;
	float yaw;
	float pitch;
	float roll;
	float length;
	std::array<uint8_t, 4> primary;
	std::array<uint8_t, 4> secondary;
	std::array<uint8_t, 4> cargo;
};

inline constexpr std::array<VertexAttribute, 6> VEHICLE_INSTANCE_LAYOUT = {{
	{3, 3, AttributeType::Float, offsetof(VehicleInstance, x)},
	{4, 3, AttributeType::Float, offsetof(VehicleInstance, yaw)},
	{5, 1, AttributeType::Float, offsetof(VehicleInstance, length)},
	{6, 4, AttributeType::NormalisedUnsignedByte, offsetof(VehicleInstance, primary)},
	{7, 4, AttributeType::NormalisedUnsignedByte, offsetof(VehicleInstance, secondary)},
	{8, 4, AttributeType::NormalisedUnsignedByte, offsetof(VehicleInstance, cargo)},
}};

using VehicleBatch = InstanceBatch<VehicleInstance>;

/* The programs whatever is drawn as a vehicle model shades through. */
inline constexpr std::array<const char *, 2> VEHICLE_VERTEX_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/vehicle.vert",
};
inline constexpr std::array<const char *, 9> VEHICLE_FRAGMENT_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/noise.glsl",
	"mini_ui/shaders/common.glsl",
	"mini_ui/shaders/sky.glsl",
	"mini_ui/shaders/cloud_field.glsl",
	"mini_ui/shaders/shadow.glsl",
	"mini_ui/shaders/lighting.glsl",
	"mini_ui/shaders/overlay.glsl",
	"mini_ui/shaders/solid.frag",
};

/* Where a steam engine's chimney stands on its model, how tall and how wide at the mouth its smoke leaves. */
inline constexpr Vec3 STEAM_CHIMNEY_FOOT = {0.185, 0.0, 0.21};
inline constexpr double STEAM_CHIMNEY_HEIGHT = 0.075;
inline constexpr double STEAM_CHIMNEY_MOUTH = 0.028;

/* The box a model fills, in its own frame. */
struct VehicleBounds {
	Vec3 low;
	Vec3 high;
};

/* Models stand on the ground, rail or water at the origin with their front along x, in tiles; a ground vehicle's spans one full length unit.
 * Listed by look, then by detail. */
std::vector<ModelMesh> BuildVehicleModels();
std::array<VehicleBounds, VEHICLE_LOOKS> MeasureVehicleModels(const std::vector<ModelMesh> &models);

#endif /* MINI_WORLD_VEHICLE_MODELS_H */
