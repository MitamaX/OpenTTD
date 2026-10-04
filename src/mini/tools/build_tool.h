/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file build_tool.h The build tool in the player's hand and the drag it lays out. */

#ifndef MINI_TOOLS_BUILD_TOOL_H
#define MINI_TOOLS_BUILD_TOOL_H

#include "../core/camera.h"
#include "tool_kind.h"
#include "tool_plans.h"

class BuildTool {
public:
	MiniTool Kind() const { return this->kind; }
	bool Dragging() const { return this->dragging; }
	bool Removing() const { return this->remove; }
	TilePoint Anchor() const { return this->anchor; }
	const ToolPlans &Plans() const { return this->plans; }
	bool FiltersClear() const;

	void Select(MiniTool kind) { this->kind = kind; }
	void Press(TilePoint at, bool ctrl);
	void Follow(TilePoint cursor);
	void Release();
	bool Abort();

private:
	int AreaLimit() const;

	MiniTool kind = MiniTool::None;
	bool dragging = false;
	bool remove = false;
	TilePoint anchor{};
	ToolPlans plans;
};

extern BuildTool _tool;

#endif /* MINI_TOOLS_BUILD_TOOL_H */
