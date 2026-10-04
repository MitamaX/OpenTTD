/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fleet_deploy.h A depot producing a drafted consist unit by unit. */

#ifndef MINI_FLEET_FLEET_DEPLOY_H
#define MINI_FLEET_FLEET_DEPLOY_H

#include <span>
#include <vector>

#include "../../engine_type.h"
#include "../../tile_type.h"
#include "../../vehicle_type.h"

/* The draft is produced unit by unit and each new vehicle is attached to the
 * job's own consist head by explicit id, so the result is one connected train
 * regardless of what else sits in the depot. Commands are asynchronous under
 * network play, hence the stepwise stages. */
class FleetDeploy {
public:
	bool Running() const { return this->depot != INVALID_TILE; }
	bool Running(VehicleType vt) const { return this->Running() && this->vt == vt; }
	size_t Unit() const;
	size_t Units() const { return this->units.size(); }

	void Start(TileIndex depot, VehicleType vt, std::span<const EngineID> units);
	void Reset() { *this = {}; }
	VehicleID Step();

private:
	/* Build issues the next unit, Spot finds it among the depot's chains,
	 * Attach waits for its move onto the consist to apply before the next
	 * unit goes out. Single player resolves each stage within a frame. */
	enum class Stage : uint8_t {
		Build,
		Spot,
		Attach,
	};

	std::vector<VehicleID> DepotVehicles() const;
	VehicleID NewHead() const;
	VehicleID Build();
	void Spot();
	void Attach();
	void Enter(Stage stage);
	void NextUnit();
	void Wait();

	TileIndex depot = INVALID_TILE;
	VehicleType vt = VEH_TRAIN;
	std::vector<EngineID> units;
	size_t next = 0;
	Stage stage = Stage::Build;
	VehicleID head = VehicleID::Invalid();
	VehicleID fresh = VehicleID::Invalid();
	std::vector<VehicleID> before;
	int waited = 0;
};

extern FleetDeploy _deploy;

#endif /* MINI_FLEET_FLEET_DEPLOY_H */
