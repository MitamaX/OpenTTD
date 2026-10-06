/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_models.cpp The low poly trains, road vehicles, ships and aircraft, at each level of detail, built once in code. */

#include "../../stdafx.h"
#include "vehicle_models.h"

#include <algorithm>
#include <limits>

#include "vehicle_parts.h"

#include "../../safeguards.h"

/* Ships and aircraft are drawn larger than life beside the ways and buildings, so they read at a glance. */
static constexpr double SHIP_SCALE = 1.4;
static constexpr double AIRCRAFT_SCALE = 1.3;

static double ScaleOf(VehicleLook look)
{
	if (look < VehicleLook::Ferry) return 1.0;
	return look < VehicleLook::PropPlane ? SHIP_SCALE : AIRCRAFT_SCALE;
}

static ModelMesh VehicleModel(VehicleLook look, VehicleDetail detail)
{
	if (look < VehicleLook::Bus) return RailVehicleModel(look, detail);
	if (look < VehicleLook::Ferry) return RoadVehicleModel(look, detail);
	if (look < VehicleLook::PropPlane) return ShipModel(look, detail);
	return AircraftModel(look, detail);
}

std::vector<ModelMesh> BuildVehicleModels()
{
	std::vector<ModelMesh> models(VEHICLE_MODELS);
	for (size_t model = 0; model < VEHICLE_MODELS; model++) {
		VehicleDetail detail = static_cast<VehicleDetail>(model % VEHICLE_DETAILS);
		double scale = ScaleOf(LookOfModel(model));
		models[model] = VehicleModel(LookOfModel(model), detail).Transform(Mat4::Scaling({scale, scale, scale}));
	}
	return models;
}

std::array<VehicleBounds, VEHICLE_LOOKS> MeasureVehicleModels(const std::vector<ModelMesh> &models)
{
	std::array<VehicleBounds, VEHICLE_LOOKS> bounds;
	for (size_t look = 0; look < VEHICLE_LOOKS; look++) {
		constexpr double FAR = std::numeric_limits<double>::max();
		VehicleBounds box = {{FAR, FAR, FAR}, {-FAR, -FAR, -FAR}};
		for (const ModelVertex &vertex : models[VehicleModelIndex(static_cast<VehicleLook>(look), VehicleDetail::Full)].vertices) {
			box.low = {std::min(box.low.x, static_cast<double>(vertex.x)), std::min(box.low.y, static_cast<double>(vertex.y)), std::min(box.low.z, static_cast<double>(vertex.z))};
			box.high = {std::max(box.high.x, static_cast<double>(vertex.x)), std::max(box.high.y, static_cast<double>(vertex.y)), std::max(box.high.z, static_cast<double>(vertex.z))};
		}
		bounds[look] = box;
	}
	return bounds;
}
