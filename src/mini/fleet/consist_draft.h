/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file consist_draft.h The consist the fleet window assembles before a depot produces it. */

#ifndef MINI_FLEET_CONSIST_DRAFT_H
#define MINI_FLEET_CONSIST_DRAFT_H

#include <vector>

#include "../../economy_type.h"
#include "../../engine_type.h"
#include "../../vehicle_type.h"

struct Engine;

struct DraftSummary {
	Money cost = 0;
	uint64_t power = 0;
	uint64_t weight = 0;
	uint16_t speed = 0;
	uint capacity = 0;
	int length = 0;
};

/* A draft is assembled in the fleet window and produced whole at a depot.
 * Only a train takes several units; any other draft is the one engine picked. */
class ConsistDraft {
public:
	explicit ConsistDraft(VehicleType vt) : vt(vt) {}

	const std::vector<EngineID> &Units() const { return this->units; }
	bool Empty() const { return this->units.empty(); }
	DraftSummary Summary() const;

	void Add(EngineID engine);
	void Remove(size_t i);
	void Prune();
	void Clear() { this->units.clear(); }

private:
	VehicleType vt;
	std::vector<EngineID> units;
};

int UnitLength(const Engine *e);
ConsistDraft &FleetDraft(VehicleType vt);
void ClearFleetDrafts();

#endif /* MINI_FLEET_CONSIST_DRAFT_H */
