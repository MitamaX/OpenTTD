/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fleet_deploy.cpp A depot producing a drafted consist unit by unit. */

#include "../../stdafx.h"
#include "fleet_deploy.h"

#include <algorithm>

#include "../../cargo_type.h"
#include "../../command_func.h"
#include "../../depot_map.h"
#include "../../engine_base.h"
#include "../../network/network_type.h"
#include "../../train_cmd.h"
#include "../../vehicle_base.h"
#include "../../vehicle_cmd.h"
#include "../../vehicle_func.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static constexpr int WAIT_LIMIT_FRAMES = 180;

FleetDeploy _deploy;

size_t FleetDeploy::Unit() const
{
	return std::min(this->next + 1, this->units.size());
}

void FleetDeploy::Start(TileIndex depot, VehicleType vt, std::span<const EngineID> units)
{
	if (this->Running()) return;

	this->Reset();
	this->depot = depot;
	this->vt = vt;
	this->units.assign(units.begin(), units.end());
}

/* Answers the finished consist, the one thing the player wants next: its
 * window is where orders and the start command live. */
VehicleID FleetDeploy::Step()
{
	if (!this->Running()) return VehicleID::Invalid();
	if (!IsDepotTile(this->depot) || GetDepotVehicleType(this->depot) != this->vt) {
		this->Reset();
		return VehicleID::Invalid();
	}

	switch (this->stage) {
		case Stage::Build: return this->Build();
		case Stage::Spot: this->Spot(); break;
		case Stage::Attach: this->Attach(); break;
	}
	return VehicleID::Invalid();
}

std::vector<VehicleID> FleetDeploy::DepotVehicles() const
{
	std::vector<VehicleID> ids;
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->type == this->vt && v->tile == this->depot) ids.push_back(v->index);
	}
	return ids;
}

VehicleID FleetDeploy::NewHead() const
{
	for (VehicleID id : this->DepotVehicles()) {
		if (std::ranges::find(this->before, id) != this->before.end()) continue;
		const Vehicle *v = Vehicle::GetIfValid(id);
		if (v != nullptr && v->First() == v) return id;
	}
	return VehicleID::Invalid();
}

VehicleID FleetDeploy::Build()
{
	if (this->next >= this->units.size()) {
		VehicleID built = this->head;
		this->Reset();
		return built;
	}

	const Engine *e = Engine::GetIfValid(this->units[this->next]);
	if (e == nullptr) {
		this->next++;
		return VehicleID::Invalid();
	}

	this->before = this->DepotVehicles();
	Command<CMD_BUILD_VEHICLE>::Post(GetCmdBuildVehMsg(this->vt), this->depot, e->index, true, INVALID_CARGO, INVALID_CLIENT_ID);
	this->Enter(Stage::Spot);
	return VehicleID::Invalid();
}

void FleetDeploy::Spot()
{
	this->fresh = this->NewHead();
	if (this->fresh == VehicleID::Invalid()) {
		this->Wait();
		return;
	}
	if (this->vt != VEH_TRAIN || this->head == VehicleID::Invalid()) {
		this->head = this->fresh;
		this->NextUnit();
		return;
	}

	const Vehicle *hv = Vehicle::GetIfValid(this->head);
	if (hv == nullptr || hv->tile != this->depot) {
		this->Reset();
		return;
	}
	Command<CMD_MOVE_RAIL_VEHICLE>::Post(STR_ERROR_CAN_T_MOVE_VEHICLE, this->depot, this->fresh, hv->Last()->index, false);
	this->Enter(Stage::Attach);
}

void FleetDeploy::Attach()
{
	const Vehicle *nv = Vehicle::GetIfValid(this->fresh);
	if (nv == nullptr) {
		this->Reset();
		return;
	}
	if (nv->First()->index == this->head) {
		this->NextUnit();
		return;
	}
	this->Wait();
}

void FleetDeploy::Enter(Stage stage)
{
	this->stage = stage;
	this->waited = 0;
}

void FleetDeploy::NextUnit()
{
	this->fresh = VehicleID::Invalid();
	this->next++;
	this->stage = Stage::Build;
}

void FleetDeploy::Wait()
{
	if (++this->waited > WAIT_LIMIT_FRAMES) this->Reset();
}
