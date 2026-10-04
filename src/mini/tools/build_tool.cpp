/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file build_tool.cpp The build tool in the player's hand and the drag it lays out. */

#include "../../stdafx.h"
#include "build_tool.h"

#include <utility>

#include "../../map_func.h"
#include "../../settings_type.h"
#include "clear_filter.h"
#include "tile_pick.h"
#include "tool_commit.h"

#include "../../safeguards.h"

BuildTool _tool;

bool BuildTool::FiltersClear() const
{
	return this->kind == MiniTool::Demolish && !_clear_filter.TakesAll();
}

void BuildTool::Press(TilePoint at, bool ctrl)
{
	if (IsClickTool(this->kind)) {
		CommitClick(this->kind, SiteTileAt(at), ctrl);
		return;
	}

	this->dragging = true;
	this->remove = ctrl && CanDragRemove(this->kind);
	this->anchor = at;
	this->plans.Clear();
	this->Follow(at);
}

/* The plan has to be settled before anything asks the game about it, and the
 * camera it is measured against only settles late in the frame. Both the probe
 * and the blueprint read the plan from here on. */
void BuildTool::Follow(TilePoint cursor)
{
	if (!this->dragging) return;

	if (this->kind == MiniTool::Rail) this->plans.rail.Walk(PathTileAt(this->anchor), PathTileAt(cursor));
	if (this->kind == MiniTool::Road || IsBridgeTool(this->kind)) this->plans.line.Lay(this->anchor, cursor);
	if (this->kind == MiniTool::Signal) this->plans.signal.Lay(this->anchor, cursor);
	if (IsRectTool(this->kind)) this->plans.area.Span(SiteTileAt(this->anchor), SiteTileAt(cursor), this->AreaLimit());
}

void BuildTool::Release()
{
	if (!this->dragging) return;

	this->dragging = false;
	CommitDrag(*this);
	this->plans.Clear();
}

bool BuildTool::Abort()
{
	bool was_dragging = std::exchange(this->dragging, false);
	this->plans.Clear();
	return was_dragging;
}

int BuildTool::AreaLimit() const
{
	if (this->kind == MiniTool::Station) return _settings_game.station.station_spread;
	return std::max<int>(Map::SizeX(), Map::SizeY());
}
