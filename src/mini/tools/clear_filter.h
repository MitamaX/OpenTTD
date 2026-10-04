/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file clear_filter.h Which kind of tile the clear tool takes off its drag. */

#ifndef MINI_TOOLS_CLEAR_FILTER_H
#define MINI_TOOLS_CLEAR_FILTER_H

#include <span>
#include <string_view>

#include "../../tile_type.h"
#include "tool_kind.h"

/* The clear tool takes a filter: the drag still covers an area, but only the
 * kind of tile the player picked is taken off it. */
enum class MiniClear : uint8_t {
	All,
	RailAny,
	RailTrack,
	Signal,
	RailStation,
	RailDepot,
	RailWaypoint,
	RailTunnelBridge,
	RoadAny,
	Road,
	RoadStop,
	RoadDepot,
	RoadWaypoint,
	RoadTunnelBridge,
	WaterAny,
	Canal,
	Dock,
	Buoy,
	ShipDepot,
	Aqueduct,
	AirAny,
	Airport,
	LandAny,
	Tree,
	House,
	Industry,
	Object,
};

struct MiniClearItem {
	std::string_view label;
	MiniTool icon;
	MiniClear mode;
};

/* The category is a filter in its own right: it takes everything its items
 * cover, so one click clears a whole transport system off the drag. */
struct MiniClearCategory {
	std::string_view label;
	MiniTool icon;
	MiniClear mode;
	std::span<const MiniClearItem> items;
};

std::span<const MiniClearCategory> ClearCategories();

class ClearFilter {
public:
	MiniClear Mode() const { return this->mode; }
	bool TakesAll() const { return this->mode == MiniClear::All; }
	std::string_view Label() const;
	void Select(MiniClear mode) { this->mode = mode; }

	bool Matches(TileIndex tile) const;
	bool Post(TileIndex tile) const;

private:
	MiniClear mode = MiniClear::All;
};

extern ClearFilter _clear_filter;

#endif /* MINI_TOOLS_CLEAR_FILTER_H */
