/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file build_rows.h Every variant the tool in hand can take, one row each, so a type is chosen by pointing at it. */

#ifndef MINI_HUD_BUILD_ROWS_H
#define MINI_HUD_BUILD_ROWS_H

#include <RmlUi/Core/Types.h>

#include "../tools/tool_kind.h"

struct FacingCell {
	int value = 0;
	Rml::String turn;
	bool active = false;

	bool operator==(const FacingCell &) const = default;
};

struct BuildRow {
	Rml::String text;
	int option = 0;
	int value = 0;
	bool active = false;
	bool head = false;
	Rml::String mark;
	Rml::Vector<FacingCell> cells;

	bool operator==(const BuildRow &) const = default;
};

Rml::Vector<BuildRow> CollectBuildRows(MiniTool kind);

#endif /* MINI_HUD_BUILD_ROWS_H */
