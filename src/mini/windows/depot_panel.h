/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file depot_panel.h The shared depot workbench: the buy list beside the draft and every depot of one vehicle type. */

#ifndef MINI_WINDOWS_DEPOT_PANEL_H
#define MINI_WINDOWS_DEPOT_PANEL_H

#include "../../depot_type.h"
#include "../../engine_type.h"
#include "../../tile_type.h"
#include "../../vehicle_type.h"
#include "window_panel.h"

class ConsistDraft;
struct Vehicle;

class DepotPanel final : public WindowPanel {
public:
	DepotPanel();

	bool IsAlive() const override;

private:
	void Fill() override;
	VehicleType Type() const { return static_cast<VehicleType>(this->tab); }
	void Validate(VehicleType type);

	void FillCatalogue(VehicleType type);
	void FillInspected();
	void FillDraft(VehicleType type, const ConsistDraft &draft);
	void FillDepots(VehicleType type, const ConsistDraft &draft);
	void AddDepot(LedgerSection &yard, VehicleType type, TileIndex tile, uint destination, DepotID depot, const ConsistDraft &draft);
	void AddConsist(LedgerSection &yard, const Vehicle &head);
	void FillCommands(VehicleType type, const ConsistDraft &draft);
	void MarkUnit(VehicleID target, bool attach);

	VehicleID marked = VehicleID::Invalid();
	EngineID inspected = EngineID::Invalid();
	bool show_hidden = false;
	TileIndex sell_armed = INVALID_TILE;
};

#endif /* MINI_WINDOWS_DEPOT_PANEL_H */
