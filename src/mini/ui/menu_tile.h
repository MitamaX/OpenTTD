/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file menu_tile.h A square menu button: a picture with its name under it. */

#ifndef MINI_UI_MENU_TILE_H
#define MINI_UI_MENU_TILE_H

#include <string_view>

#include <RmlUi/Core/Types.h>

#include "../../strings_type.h"

struct MenuTile {
	Rml::String label;
	Rml::String icon;
	bool active = false;
};

Rml::String IconPath(std::string_view name);
MenuTile MakeMenuTile(StringID str, std::string_view fallback, std::string_view icon, bool active);

#endif /* MINI_UI_MENU_TILE_H */
