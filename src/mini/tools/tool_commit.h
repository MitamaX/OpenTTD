/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tool_commit.h The game commands the build tools post. */

#ifndef MINI_TOOLS_TOOL_COMMIT_H
#define MINI_TOOLS_TOOL_COMMIT_H

#include "../../station_type.h"
#include "../../tile_type.h"
#include "tool_kind.h"
#include "tool_plans.h"

class BuildTool;

void CommitClick(MiniTool kind, TileIndex tile, bool remove);
void CommitDrag(const BuildTool &tool);

void PostRun(const RailPlan &plan, const PlanRun &run, bool remove);
void PostRun(const LinePlan &plan, const PlanRun &run, bool remove);
void PostArea(MiniTool kind, TileIndex origin, TileIndex far, bool remove);
void PostRemoveRoadStop(TileIndex tile, RoadStopType type);

#endif /* MINI_TOOLS_TOOL_COMMIT_H */
