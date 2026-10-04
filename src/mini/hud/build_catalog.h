/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file build_catalog.h The build tools as the menus list them, and what each one is called. */

#ifndef MINI_HUD_BUILD_CATALOG_H
#define MINI_HUD_BUILD_CATALOG_H

#include <span>
#include <string>
#include <string_view>

#include "../../strings_type.h"
#include "../tools/tool_kind.h"

struct MiniMenuItem {
	StringID str;
	std::string_view fallback;
	MiniTool tool;
};

struct MiniMenuCategory {
	StringID str;
	std::string_view fallback;
	MiniTool icon;
	std::span<const MiniMenuItem> items;
};

std::span<const MiniMenuCategory> BuildCategories();
std::span<const MiniMenuItem> CommandItems();
std::string ToolLabel(MiniTool tool);

#endif /* MINI_HUD_BUILD_CATALOG_H */
