/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file waypoint_panel.h A rail or road waypoint, or a buoy. */

#ifndef MINI_WINDOWS_WAYPOINT_PANEL_H
#define MINI_WINDOWS_WAYPOINT_PANEL_H

#include "../../station_type.h"
#include "../ui/ledger_panel.h"

class WaypointPanel final : public LedgerPanel {
public:
	explicit WaypointPanel(StationID waypoint);

	bool IsAlive() const override;

private:
	void Fill() override;
	bool Renamable() const override;
	void Rename(std::string name) override;

	const StationID waypoint;
};

#endif /* MINI_WINDOWS_WAYPOINT_PANEL_H */
