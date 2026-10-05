/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file station_panel.h A station: waiting cargo, the industries around it, the vehicles calling at it and its facts. */

#ifndef MINI_WINDOWS_STATION_PANEL_H
#define MINI_WINDOWS_STATION_PANEL_H

#include "../../station_type.h"
#include "window_panel.h"

struct Station;

class StationPanel final : public WindowPanel {
public:
	explicit StationPanel(StationID station);

	bool IsAlive() const override;

private:
	std::optional<CameraShot> Camera() const override;
	bool Renamable() const override;
	void Rename(std::string name) override;
	void Fill() override;

	void FillCargo(const Station &st);
	void FillIndustries(const Station &st);
	void FillVehicles(const Station &st);
	void FillFacts(const Station &st);
	void FillCommands(const Station &st);

	const StationID station;
};

#endif /* MINI_WINDOWS_STATION_PANEL_H */
