/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file consist_draft.cpp The consist the fleet window assembles before a depot produces it. */

#include "../../stdafx.h"
#include "consist_draft.h"

#include <array>

#include "../../engine_base.h"

#include "../../safeguards.h"

static std::array<ConsistDraft, VEH_COMPANY_END> _drafts = {
	ConsistDraft{VEH_TRAIN},
	ConsistDraft{VEH_ROAD},
	ConsistDraft{VEH_SHIP},
	ConsistDraft{VEH_AIRCRAFT},
};

int UnitLength(const Engine *e)
{
	if (e->type != VEH_TRAIN) return VEHICLE_LENGTH;
	return VEHICLE_LENGTH - e->VehInfo<RailVehicleInfo>().shorten_factor;
}

DraftSummary ConsistDraft::Summary() const
{
	DraftSummary sum;
	for (EngineID id : this->units) {
		const Engine *e = Engine::Get(id);
		sum.cost += e->GetCost();
		sum.power += e->GetPower();
		sum.weight += e->GetDisplayWeight();
		uint16_t speed = e->GetDisplayMaxSpeed();
		if (speed > 0) sum.speed = sum.speed == 0 ? speed : std::min(sum.speed, speed);
		sum.capacity += e->GetDisplayDefaultCapacity();
		sum.length += UnitLength(e);
	}
	return sum;
}

void ConsistDraft::Add(EngineID engine)
{
	if (this->vt == VEH_TRAIN) {
		this->units.push_back(engine);
	} else {
		this->units.assign(1, engine);
	}
}

void ConsistDraft::Remove(size_t i)
{
	if (i < this->units.size()) this->units.erase(this->units.begin() + i);
}

void ConsistDraft::Prune()
{
	std::erase_if(this->units, [](EngineID id) {
		const Engine *e = Engine::GetIfValid(id);
		return e == nullptr || !e->IsEnabled();
	});
}

ConsistDraft &FleetDraft(VehicleType vt)
{
	return _drafts[vt];
}

void ClearFleetDrafts()
{
	for (ConsistDraft &draft : _drafts) draft.Clear();
}
