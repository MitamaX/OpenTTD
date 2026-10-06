/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file input_mode.cpp What a click on the map means right now. */

#include "../../stdafx.h"
#include "input_mode.h"

#include "../../command_func.h"
#include "../../company_func.h"
#include "../../depot_map.h"
#include "../../gfx_func.h"
#include "../../industry.h"
#include "../../mini_ui.h"
#include "../../order_cmd.h"
#include "../../settings_type.h"
#include "../../station_base.h"
#include "../../station_map.h"
#include "../../vehicle_base.h"
#include "../map/vehicle_motion.h"
#include "../tools/build_tool.h"
#include "../tools/tile_pick.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

InputMode _mode;

void InputMode::Idle()
{
	_tool.Select(MiniTool::None);
	this->follow = VehicleID::Invalid();
	this->order_pick = VehicleID::Invalid();
}

void InputMode::ToggleBuild(MiniTool kind)
{
	bool holding = _tool.Kind() == kind;
	this->Idle();
	if (!holding) _tool.Select(kind);
}

void InputMode::ToggleFollow(VehicleID v)
{
	bool following = this->follow == v;
	this->Idle();
	if (!following) this->follow = v;
}

void InputMode::TogglePickOrders(VehicleID v)
{
	bool picking = this->order_pick == v;
	this->Idle();
	if (!picking) this->order_pick = v;
}

/* One layer per call: the drag in progress first, then the mode itself. */
bool InputMode::Unwind()
{
	if (_tool.Abort()) return true;
	if (!this->Engaged()) return false;
	this->Idle();
	return true;
}

std::optional<WorldPoint> InputMode::FollowTarget()
{
	if (!this->Following()) return std::nullopt;

	const Vehicle *v = Vehicle::GetIfValid(this->follow);
	if (v == nullptr || MiniUiPanKeys() != 0 || _middle_button_down) {
		this->Unfollow();
		return std::nullopt;
	}
	return _vehicle_motion.Position(v);
}

/* Stock order-window map picking, trimmed to the plain cases: a click
 * resolves to a depot, waypoint or station order for the picked vehicle. */
static Order OrderFromTile(const Vehicle *v, TileIndex tile)
{
	Order order{};

	if (IsDepotTypeTile(tile, (TransportType)(uint)v->type) && IsTileOwner(tile, _local_company)) {
		order.MakeGoToDepot(GetDepotDestinationIndex(tile),
				OrderDepotTypeFlag::PartOfOrders,
				(_settings_client.gui.new_nonstop && v->IsGroundVehicle()) ? OrderNonStopFlag::NoIntermediate : OrderNonStopFlags{});
		return order;
	}

	if ((IsRailWaypointTile(tile) && v->type == VEH_TRAIN && IsTileOwner(tile, _local_company)) ||
			(IsRoadWaypointTile(tile) && v->type == VEH_ROAD && IsTileOwner(tile, _local_company)) ||
			(IsBuoyTile(tile) && v->type == VEH_SHIP)) {
		order.MakeGoToWaypoint(GetStationIndex(tile));
		if (!IsBuoyTile(tile) && _settings_client.gui.new_nonstop) order.SetNonStopType({OrderNonStopFlag::NoIntermediate, OrderNonStopFlag::NoDestination});
		return order;
	}

	if (IsTileType(tile, MP_STATION) || IsTileType(tile, MP_INDUSTRY)) {
		const Station *st = IsTileType(tile, MP_STATION) ? Station::GetByTile(tile) : Industry::GetByTile(tile)->neutral_station;
		if (st != nullptr && (st->owner == _local_company || st->owner == OWNER_NONE)) {
			StationFacilities facil;
			switch (v->type) {
				case VEH_SHIP:     facil = StationFacility::Dock;    break;
				case VEH_TRAIN:    facil = StationFacility::Train;   break;
				case VEH_AIRCRAFT: facil = StationFacility::Airport; break;
				default:           facil = {StationFacility::BusStop, StationFacility::TruckStop}; break;
			}
			if (st->facilities.Any(facil)) {
				order.MakeGoToStation(st->index);
				if (_settings_client.gui.new_nonstop && v->IsGroundVehicle()) order.SetNonStopType(OrderNonStopFlag::NoIntermediate);
				order.SetStopLocation(v->type == VEH_TRAIN ? (OrderStopLocation)(_settings_client.gui.stop_location) : OrderStopLocation::FarEnd);
				return order;
			}
		}
	}

	order.Free();
	return order;
}

/* Returns false when the tile carries nothing the vehicle can be sent to, so
 * a list row can fall back to opening its own window. */
bool InputMode::AppendOrder(TileIndex tile) const
{
	const Vehicle *v = this->OrderTaker();
	if (v == nullptr) return false;

	Order order = OrderFromTile(v, tile);
	if (order.IsType(OT_NOTHING)) return false;
	Command<CMD_INSERT_ORDER>::Post(STR_ERROR_CAN_T_INSERT_NEW_ORDER, v->tile, v->index, (VehicleOrderID)v->GetNumOrders(), order);
	return true;
}

void InputMode::PickOrderAt(TilePoint at)
{
	if (this->OrderTaker() == nullptr) {
		this->Idle();
		return;
	}
	std::optional<TileIndex> tile = TileUnder(at);
	if (tile.has_value()) this->AppendOrder(*tile);
}

bool InputMode::Engaged() const
{
	return _tool.Kind() != MiniTool::None || this->Following() || this->PickingOrders();
}

const Vehicle *InputMode::OrderTaker() const
{
	const Vehicle *v = Vehicle::GetIfValid(this->order_pick);
	return v != nullptr && v->owner == _local_company ? v : nullptr;
}
