/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_overlay.cpp The info layer the map highlights over a greyed base. */

#include "../../stdafx.h"
#include "map_overlay.h"

#include "../core/tuning.h"

#include "../../safeguards.h"

MapOverlay _overlay;

MiniLayer MapOverlay::Filter() const
{
	return _tuning.filter_alpha > 0 ? this->shown : MiniLayer::None;
}

void MapOverlay::Toggle(MiniLayer layer)
{
	this->shown = this->shown == layer ? MiniLayer::None : layer;
	this->automatic = false;
}

void MapOverlay::FollowTool(MiniLayer tool_layer)
{
	if (tool_layer == this->last_tool_layer) return;

	if (tool_layer != MiniLayer::None) {
		this->shown = tool_layer;
		this->automatic = true;
	} else if (this->automatic) {
		this->shown = MiniLayer::None;
		this->automatic = false;
	}
	this->last_tool_layer = tool_layer;
}

void MapOverlay::Reset()
{
	*this = {};
}
