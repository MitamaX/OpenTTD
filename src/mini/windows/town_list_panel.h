/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town_list_panel.h Every town, ranked by name, size or the company's standing. */

#ifndef MINI_WINDOWS_TOWN_LIST_PANEL_H
#define MINI_WINDOWS_TOWN_LIST_PANEL_H

#include "directory_panel.h"

class TownListPanel final : public DirectoryPanel {
public:
	TownListPanel();

private:
	std::vector<DirectoryEntry> Entries() const override;
	Rml::String Emptiness() const override;
};

#endif /* MINI_WINDOWS_TOWN_LIST_PANEL_H */
