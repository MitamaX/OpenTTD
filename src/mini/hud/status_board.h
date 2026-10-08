/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file status_board.h The company's vehicles sorted by what is wrong with them. */

#ifndef MINI_HUD_STATUS_BOARD_H
#define MINI_HUD_STATUS_BOARD_H

#include <array>
#include <string>
#include <unordered_set>
#include <vector>

#include "../../core/enum_type.hpp"
#include "../../vehicle_type.h"

enum class VehicleStatus : uint8_t {
	Crashed,
	Lost,
	Stuck,
	Broken,
	NoOrders,
	BadOrders,
	OldAge,
	Unprofitable,
	End,
};

std::string StatusLabel(VehicleStatus status);
bool StatusIsCritical(VehicleStatus status);

/* No event cards, only live aggregated problem states, rescanned on the
 * stream's beat. */
class StatusBoard {
public:
	void Scan();
	const std::vector<VehicleID> &Vehicles(VehicleStatus status) const { return this->lists[to_underlying(status)]; }
	const Vehicle *Next(VehicleStatus status);
	void Clear();

private:
	static constexpr size_t COUNT = to_underlying(VehicleStatus::End);

	std::array<std::vector<VehicleID>, COUNT> lists;
	std::array<uint32_t, COUNT> cursor{};
	/* Waiting at a signal is normal traffic; a stuck train only becomes a
	 * status past the same wait the stuck news uses, and stays one until it
	 * moves. */
	std::unordered_set<uint32_t> stuck_long;
};

extern StatusBoard _status_board;

#endif /* MINI_HUD_STATUS_BOARD_H */
