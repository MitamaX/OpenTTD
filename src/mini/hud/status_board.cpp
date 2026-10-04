/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file status_board.cpp The company's vehicles sorted by what is wrong with them. */

#include "../../stdafx.h"
#include "status_board.h"

#include "../../company_func.h"
#include "../../order_base.h"
#include "../../settings_type.h"
#include "../../station_base.h"
#include "../../timer/timer_game_tick.h"
#include "../../train.h"
#include "../../vehicle_base.h"
#include "../../vehicle_func.h"
#include "../ui/ui_text.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

StatusBoard _status_board;

std::string StatusLabel(VehicleStatus status)
{
	switch (status) {
		case VehicleStatus::Crashed: return GameText(STR_VEHICLE_STATUS_CRASHED);
		case VehicleStatus::Lost: return "LOST";
		case VehicleStatus::Stuck: return "STUCK";
		case VehicleStatus::Broken: return GameText(STR_VEHICLE_STATUS_BROKEN_DOWN);
		case VehicleStatus::NoOrders: return "NO ORDERS";
		case VehicleStatus::BadOrders: return "BAD ORDERS";
		case VehicleStatus::OldAge: return "OLD AGE";
		default: return "IN THE RED";
	}
}

bool StatusIsCritical(VehicleStatus status)
{
	return to_underlying(status) <= to_underlying(VehicleStatus::Broken);
}

/* A persistent row needs a hard error: void orders or a station the vehicle
 * cannot use. The softer advice cases of the native order review, too few
 * stations and a duplicate first and last entry, also flag valid schedules
 * like waypoint loops, so they stay with the one-shot native news. */
static bool HasBadOrders(const Vehicle *v)
{
	for (const Order &order : v->Orders()) {
		if (order.IsType(OT_DUMMY)) return true;
		if (order.IsType(OT_GOTO_STATION) && !CanVehicleUseStation(v, Station::Get(order.GetDestination().ToStationID()))) return true;
	}
	return false;
}

void StatusBoard::Scan()
{
	for (auto &list : this->lists) list.clear();
	auto add = [this](VehicleStatus status, const Vehicle *v) { this->lists[to_underlying(status)].push_back(v->index); };

	std::unordered_set<uint32_t> keep;
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->type > VEH_AIRCRAFT || !v->IsPrimaryVehicle() || v->owner != _local_company) continue;
		if (v->vehstatus.Test(VehState::Crashed)) {
			add(VehicleStatus::Crashed, v);
			continue;
		}
		if (v->vehicle_flags.Test(VehicleFlag::PathfinderLost)) add(VehicleStatus::Lost, v);
		if (v->type == VEH_TRAIN) {
			const Train *t = Train::From(v);
			uint32_t id = v->index.base();
			if (t->flags.Test(VehicleRailFlag::Stuck) && (t->wait_counter >= _settings_game.pf.wait_for_pbs_path * Ticks::DAY_TICKS || this->stuck_long.contains(id))) {
				keep.insert(id);
				add(VehicleStatus::Stuck, v);
			}
		}
		if (v->type != VEH_AIRCRAFT && v->breakdown_ctr == 1) add(VehicleStatus::Broken, v);
		if (v->GetNumOrders() == 0 && !v->vehstatus.Test(VehState::Stopped)) add(VehicleStatus::NoOrders, v);
		if (HasBadOrders(v)) add(VehicleStatus::BadOrders, v);
		if (v->age > v->max_age) add(VehicleStatus::OldAge, v);
		/* Last year alone would pin the row until new year even after the route
		 * was fixed; earning anything this year clears it. */
		if (v->economy_age >= VEHICLE_PROFIT_MIN_AGE && v->GetDisplayProfitLastYear() < 0 && v->GetDisplayProfitThisYear() < 0) add(VehicleStatus::Unprofitable, v);
	}
	this->stuck_long = std::move(keep);
}

/* Each call moves on to the next vehicle of the kind, so repeated clicks walk
 * the camera through all of them. */
const Vehicle *StatusBoard::Next(VehicleStatus status)
{
	const std::vector<VehicleID> &list = this->Vehicles(status);
	uint32_t &at = this->cursor[to_underlying(status)];
	for (size_t tried = 0; tried < list.size(); tried++) {
		const Vehicle *v = Vehicle::GetIfValid(list[at++ % list.size()]);
		if (v != nullptr) return v;
	}
	return nullptr;
}

void StatusBoard::Clear()
{
	for (auto &list : this->lists) list.clear();
	this->stuck_long.clear();
}
