/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file waypoint_panel.cpp A rail or road waypoint, or a buoy. */

#include "../../stdafx.h"
#include "waypoint_panel.h"

#include "../../command_func.h"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../../core/format.hpp"
#include "../../map_func.h"
#include "../../town.h"
#include "../../waypoint_base.h"
#include "../../waypoint_cmd.h"
#include "../ui/ui_text.h"
#include "window_links.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static std::string_view WaypointKind(const Waypoint &wp)
{
	if (wp.facilities.Test(StationFacility::Train)) return "철도 대기점";
	if (wp.facilities.Test(StationFacility::TruckStop) || wp.facilities.Test(StationFacility::BusStop)) return "도로 대기점";
	return "부표";
}

WaypointPanel::WaypointPanel(StationID waypoint) : LedgerPanel(fmt::format("waypoint{}", waypoint.base()), {}, {}), waypoint(waypoint)
{
}

bool WaypointPanel::IsAlive() const
{
	return Waypoint::IsValidID(this->waypoint);
}

bool WaypointPanel::Renamable() const
{
	return Waypoint::Get(this->waypoint)->owner == _local_company;
}

void WaypointPanel::Rename(std::string name)
{
	Command<CMD_RENAME_WAYPOINT>::Post(STR_ERROR_CAN_T_CHANGE_WAYPOINT_NAME, this->waypoint, std::move(name));
}

void WaypointPanel::Collect()
{
	const Waypoint &wp = *Waypoint::Get(this->waypoint);
	this->title = GameText(STR_WAYPOINT_NAME, this->waypoint);

	this->sections.clear();
	LedgerSection &facts = this->Section();
	facts.Add({"종류", Rml::String(WaypointKind(wp))});
	if (Company::IsValidID(wp.owner)) facts.Add({"소유", GameText(STR_COMPANY_NAME, wp.owner)});
	if (wp.town != nullptr) facts.Add({"도시", GameText(STR_TOWN_NAME, wp.town->index)});
	facts.Add({"좌표", fmt::format("{} · {}", TileX(wp.xy), TileY(wp.xy))});

	TileIndex tile = wp.xy;
	this->commands = {
		{"지도에서 보기", true, [tile] { ScrollToTile(tile); }},
	};
}
