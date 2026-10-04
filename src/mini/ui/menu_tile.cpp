/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file menu_tile.cpp A square menu button: a picture with its name under it. */

#include "../../stdafx.h"
#include "menu_tile.h"

#include "../../core/format.hpp"
#include "ui_text.h"

#include "../../safeguards.h"

Rml::String IconPath(std::string_view name)
{
	return fmt::format("icons/{}.svg", name);
}

MenuTile MakeMenuTile(StringID str, std::string_view fallback, std::string_view icon, bool active)
{
	return {GameTextOr(str, fallback), IconPath(icon), active};
}
