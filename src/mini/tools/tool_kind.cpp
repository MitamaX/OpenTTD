/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tool_kind.cpp The build tools of the mini UI and how each one is used. */

#include "../../stdafx.h"
#include "tool_kind.h"

#include "../../safeguards.h"

bool IsRectTool(MiniTool t)
{
	return t == MiniTool::Station || t == MiniTool::Demolish || t == MiniTool::Terraform || t == MiniTool::Canal || t == MiniTool::Convert ||
			t == MiniTool::RoadConvert || t == MiniTool::Trees || t == MiniTool::BuyLand;
}

bool IsPointTool(MiniTool t)
{
	return t == MiniTool::BusStop || t == MiniTool::TruckStop || t == MiniTool::TrainDepot || t == MiniTool::RoadDepot || t == MiniTool::Signal || t == MiniTool::RailTunnel || t == MiniTool::RoadTunnel || t == MiniTool::RailWaypoint || t == MiniTool::RoadWaypoint || t == MiniTool::ShipDepot || t == MiniTool::Dock || t == MiniTool::Buoy || t == MiniTool::Airport || t == MiniTool::Lock || t == MiniTool::Headquarters || t == MiniTool::Industry || t == MiniTool::Sign;
}

/* Signals are aimed like the other point tools but laid in runs, so they
 * start a drag where the rest place on the click. */
bool IsClickTool(MiniTool t)
{
	return IsPointTool(t) && t != MiniTool::Signal;
}

bool IsBridgeTool(MiniTool t)
{
	return t == MiniTool::RailBridge || t == MiniTool::RoadBridge;
}

bool IsTunnelTool(MiniTool t)
{
	return t == MiniTool::RailTunnel || t == MiniTool::RoadTunnel;
}

bool IsRoadTool(MiniTool t)
{
	return t == MiniTool::Road || t == MiniTool::BusStop || t == MiniTool::TruckStop ||
			t == MiniTool::RoadDepot || t == MiniTool::RoadTunnel || t == MiniTool::RoadBridge ||
			t == MiniTool::RoadConvert || t == MiniTool::RoadWaypoint;
}

bool IsRoadStopTool(MiniTool t)
{
	return t == MiniTool::BusStop || t == MiniTool::TruckStop;
}

bool IsDirPointTool(MiniTool t)
{
	return t == MiniTool::TrainDepot || t == MiniTool::RoadDepot || t == MiniTool::ShipDepot;
}

bool CanDragRemove(MiniTool t)
{
	return t != MiniTool::Convert && t != MiniTool::RoadConvert && !IsBridgeTool(t);
}

bool ToolUsesRailType(MiniTool t)
{
	return t == MiniTool::Rail || t == MiniTool::Convert || t == MiniTool::Station ||
			t == MiniTool::TrainDepot || t == MiniTool::RailTunnel || t == MiniTool::RailBridge;
}

/* A waypoint drops onto road that is already there, so it takes no type. */
bool ToolUsesRoadType(MiniTool t)
{
	return IsRoadTool(t) && t != MiniTool::RoadWaypoint;
}

MiniLayer ToolLayer(MiniTool t)
{
	switch (t) {
		case MiniTool::Rail:
		case MiniTool::Convert:
		case MiniTool::Station:
		case MiniTool::RailWaypoint:
		case MiniTool::TrainDepot:
		case MiniTool::Signal:
		case MiniTool::RailTunnel:
		case MiniTool::RailBridge:
			return MiniLayer::Rail;
		case MiniTool::Road:
		case MiniTool::BusStop:
		case MiniTool::TruckStop:
		case MiniTool::RoadDepot:
		case MiniTool::RoadTunnel:
		case MiniTool::RoadBridge:
		case MiniTool::RoadConvert:
		case MiniTool::RoadWaypoint:
			return MiniLayer::Road;
		default:
			return MiniLayer::None;
	}
}
