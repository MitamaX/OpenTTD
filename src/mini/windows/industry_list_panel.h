/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file industry_list_panel.h Every industry, ranked by name, output or share transported, and the official cargo chain. */

#ifndef MINI_WINDOWS_INDUSTRY_LIST_PANEL_H
#define MINI_WINDOWS_INDUSTRY_LIST_PANEL_H

#include "directory_panel.h"

class IndustryListPanel final : public DirectoryPanel {
public:
	IndustryListPanel();

private:
	std::optional<EmbedTarget> Embed() const override;
	std::vector<DirectoryEntry> Entries() const override;
	Rml::String Emptiness() const override;
};

#endif /* MINI_WINDOWS_INDUSTRY_LIST_PANEL_H */
