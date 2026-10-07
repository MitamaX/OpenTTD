/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_pass.h The map's trains, road vehicles, ships and aircraft in 3D, moving every frame, lit and casting shadows. */

#ifndef MINI_WORLD_VEHICLE_PASS_H
#define MINI_WORLD_VEHICLE_PASS_H

#include <array>
#include <optional>
#include <vector>

#include "../../vehicle_type.h"
#include "fleet_layout.h"
#include "shader_program.h"
#include "vehicle_models.h"
#include "world_pass.h"

struct VehicleHit {
	VehicleID vehicle;
	double distance;
};

class VehiclePass final : public WorldPass {
public:
	VehiclePass();

	std::string_view Name() const override { return "vehicles"; }
	void Reload() override;
	void Prepare(const SceneView &view) override;
	void Cast(const ShadowView &view) override;
	void Draw(const SceneView &view) override;
	void Release() override;

	/* The vehicle a sight line from the eye meets first, as the last frame laid them out. */
	std::optional<VehicleHit> Pick(const Vec3 &origin, const Vec3 &direction) const;
	std::span<const ShipWake> Wakes() const { return this->fleet.Wakes(); }
	std::span<const SmokeVent> Funnels() const { return this->fleet.Funnels(); }

private:
	void Gather(const SceneView &camera, const Frustum &frustum, double fewest_pixels);
	void DrawBatch(const ShaderProgram &program, const SceneView &camera);

	std::vector<ModelMesh> meshes;
	std::array<VehicleBounds, VEHICLE_LOOKS> bounds{};
	FleetLayout fleet{bounds};
	SceneView seen{};
	InstancedMeshes models;
	VehicleBatch batch;
	ShaderProgram program;
	ShaderProgram caster;
};

#endif /* MINI_WORLD_VEHICLE_PASS_H */
