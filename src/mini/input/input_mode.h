/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file input_mode.h What a click on the map means right now. */

#ifndef MINI_INPUT_INPUT_MODE_H
#define MINI_INPUT_INPUT_MODE_H

#include <optional>

#include "../../tile_type.h"
#include "../../vehicle_type.h"
#include "../core/camera.h"
#include "../tools/tool_kind.h"

struct Vehicle;

/* The screen has exactly one input mode: idle, building, following a vehicle
 * or picking its orders. Entering one drops the others. */
class InputMode {
public:
	bool Following() const { return this->follow != VehicleID::Invalid(); }
	bool PickingOrders() const { return this->order_pick != VehicleID::Invalid(); }
	VehicleID FollowedVehicle() const { return this->follow; }
	VehicleID OrderVehicle() const { return this->order_pick; }

	void Idle();
	void ToggleBuild(MiniTool kind);
	void ToggleFollow(VehicleID v);
	void TogglePickOrders(VehicleID v);
	void Unfollow() { this->follow = VehicleID::Invalid(); }
	bool Unwind();

	std::optional<WorldPoint> FollowTarget();
	bool AppendOrder(TileIndex tile) const;
	void PickOrderAt(TilePoint at);

private:
	bool Engaged() const;
	const Vehicle *OrderTaker() const;

	VehicleID follow = VehicleID::Invalid();
	VehicleID order_pick = VehicleID::Invalid();
};

extern InputMode _mode;

#endif /* MINI_INPUT_INPUT_MODE_H */
