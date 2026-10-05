/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file group_panel.h The fleet workbench: groups and autoreplace beside the vehicles of the chosen group. */

#ifndef MINI_WINDOWS_GROUP_PANEL_H
#define MINI_WINDOWS_GROUP_PANEL_H

#include "../../engine_type.h"
#include "../../group_type.h"
#include "../../vehicle_type.h"
#include "window_panel.h"

struct Company;
struct Group;

class GroupPanel final : public WindowPanel {
public:
	GroupPanel();

	bool IsAlive() const override;

private:
	void Fill() override;
	VehicleType Type() const { return static_cast<VehicleType>(this->tab); }
	void Validate(VehicleType type);

	void FillGroups(VehicleType type);
	LedgerLine GroupLine(GroupID group, Rml::String name, uint count);
	void FillReplacement(VehicleType type, const Company &company);
	void FillFleet(VehicleType type);
	void FillMembers(VehicleType type);
	void FillCommands(VehicleType type);

	GroupID group = ALL_GROUP;
	EngineID engine = EngineID::Invalid();
};

#endif /* MINI_WINDOWS_GROUP_PANEL_H */
