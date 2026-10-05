/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file company_panel.h The player's company: the official company window and a roll-up of what it owns. */

#ifndef MINI_WINDOWS_COMPANY_PANEL_H
#define MINI_WINDOWS_COMPANY_PANEL_H

#include "window_panel.h"

struct Company;

class CompanyPanel final : public WindowPanel {
public:
	CompanyPanel();

	bool IsAlive() const override;

private:
	std::optional<EmbedTarget> Embed() const override;
	bool Renamable() const override;
	void Rename(std::string name) override;
	void Fill() override;

	void FillAssets(const Company &company);
};

#endif /* MINI_WINDOWS_COMPANY_PANEL_H */
