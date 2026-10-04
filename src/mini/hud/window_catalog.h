/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file window_catalog.h The status windows as the window bar lists them, and what each one is called. */

#ifndef MINI_HUD_WINDOW_CATALOG_H
#define MINI_HUD_WINDOW_CATALOG_H

#include <span>
#include <string_view>

#include "../../strings_type.h"

enum class MiniWin : uint8_t {
	Finances,
	CompanyInfo,
	Goals,
	League,
	Graph,
	Stations,
	Trains,
	RoadVehicles,
	Ships,
	Aircraft,
	News,
	Towns,
	Industries,
	Subsidies,
	Buy,
	Groups,
	Map,
	Signs,
	Save,
	Load,
	Options,
	Music,
	Abandon,
	Quit,
};

struct MiniWinItem {
	StringID str;
	std::string_view fallback;
	MiniWin win;
	std::string_view icon;
};

struct MiniWinCategory {
	StringID str;
	std::string_view fallback;
	std::string_view icon;
	std::span<const MiniWinItem> items;
};

std::span<const MiniWinCategory> WindowCategories();

#endif /* MINI_HUD_WINDOW_CATALOG_H */
