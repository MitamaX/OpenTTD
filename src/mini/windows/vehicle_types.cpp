/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_types.cpp The four kinds of company vehicle, named the way the official replace window names them. */

#include "../../stdafx.h"
#include "vehicle_types.h"

#include "../ui/ui_text.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static constexpr StringID VEHICLE_TYPE_NAMES[] = {STR_REPLACE_VEHICLE_TRAIN, STR_REPLACE_VEHICLE_ROAD_VEHICLE, STR_REPLACE_VEHICLE_SHIP, STR_REPLACE_VEHICLE_AIRCRAFT};

Rml::String VehicleTypeName(VehicleType type)
{
	return GameText(VEHICLE_TYPE_NAMES[type]);
}

Rml::Vector<Rml::String> VehicleTypeTabs()
{
	Rml::Vector<Rml::String> tabs;
	for (VehicleType type = VEH_BEGIN; type < VEH_COMPANY_END; type++) tabs.push_back(VehicleTypeName(type));
	return tabs;
}
