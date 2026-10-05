/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town_panel.h A town: growth, the local authority, cargo and the official cargo graph. */

#ifndef MINI_WINDOWS_TOWN_PANEL_H
#define MINI_WINDOWS_TOWN_PANEL_H

#include "../../town.h"
#include "window_panel.h"

class TownPanel final : public WindowPanel {
public:
	explicit TownPanel(TownID town);

	bool IsAlive() const override;

private:
	std::optional<CameraShot> Camera() const override;
	std::optional<EmbedTarget> Embed() const override;
	bool Renamable() const override;
	void Rename(std::string name) override;
	void Fill() override;

	void FillGrowth(const Town &town);
	void FillAuthority(const Town &town);
	void FillCargo(const Town &town);
	void FillCommands(const Town &town);
	LedgerLine ActionLine(TownAction action, bool available);

	const TownID town;
	std::optional<TownAction> chosen;
};

#endif /* MINI_WINDOWS_TOWN_PANEL_H */
