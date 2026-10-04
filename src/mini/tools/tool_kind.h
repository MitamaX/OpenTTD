/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tool_kind.h The build tools of the mini UI and how each one is used. */

#ifndef MINI_TOOLS_TOOL_KIND_H
#define MINI_TOOLS_TOOL_KIND_H

#include "../map/map_overlay.h"

enum class MiniTool : uint8_t {
	None,
	Rail,
	Convert,
	Road,
	Station,
	RailWaypoint,
	RoadWaypoint,
	BusStop,
	TruckStop,
	TrainDepot,
	RoadDepot,
	ShipDepot,
	Dock,
	Buoy,
	Canal,
	Lock,
	Airport,
	Demolish,
	Signal,
	RailTunnel,
	RoadTunnel,
	RailBridge,
	RoadBridge,
	RoadConvert,
	Terraform,
	Headquarters,
	Trees,
	BuyLand,
	Industry,
	Sign,
};

bool IsRectTool(MiniTool t);
bool IsPointTool(MiniTool t);
bool IsClickTool(MiniTool t);
bool IsBridgeTool(MiniTool t);
bool IsTunnelTool(MiniTool t);
bool IsRoadTool(MiniTool t);
bool IsRoadStopTool(MiniTool t);
bool IsDirPointTool(MiniTool t);
bool CanDragRemove(MiniTool t);
bool ToolUsesRailType(MiniTool t);
bool ToolUsesRoadType(MiniTool t);
MiniLayer ToolLayer(MiniTool t);

#endif /* MINI_TOOLS_TOOL_KIND_H */
