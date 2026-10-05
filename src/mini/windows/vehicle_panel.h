/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_panel.h A vehicle: where it is headed, what it carries, its orders and its facts. */

#ifndef MINI_WINDOWS_VEHICLE_PANEL_H
#define MINI_WINDOWS_VEHICLE_PANEL_H

#include "../../cargo_type.h"
#include "../../order_type.h"
#include "../../vehicle_type.h"
#include "window_panel.h"

struct Order;
struct Vehicle;

class VehiclePanel final : public WindowPanel {
public:
	explicit VehiclePanel(VehicleID vehicle);

	bool IsAlive() const override;
	VehicleID Subject() const { return this->vehicle; }

private:
	std::optional<CameraShot> Camera() const override;
	bool Renamable() const override;
	void Rename(std::string name) override;
	void Fill() override;

	void FillStatus(const Vehicle &v);
	void FillCargo(const Vehicle &v);
	void FillRefits(const Vehicle &v);
	void FillOrders(const Vehicle &v);
	LedgerLine OrderLine(const Vehicle &v, VehicleOrderID index, const Order &order);
	void FillOrderEditor(const Vehicle &v, VehicleOrderID index, const Order &order);
	void AddOrderRefits(LedgerSection &editor, const Vehicle &v, VehicleOrderID index, const Order &order);
	void FillOrderSources(const Vehicle &v);
	void FillInfo(const Vehicle &v);
	void AddServiceInterval(LedgerSection &info, const Vehicle &v);
	void FillCommands(const Vehicle &v);

	void MoveOrder(VehicleOrderID from, int to);
	void RefitOrder(VehicleOrderID index, CargoType cargo);

	const VehicleID vehicle;
	std::optional<VehicleOrderID> selected_order;
	bool order_refit_open = false;
	bool share_orders = false;
};

#endif /* MINI_WINDOWS_VEHICLE_PANEL_H */
