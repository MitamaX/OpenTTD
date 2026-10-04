/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tool_choices.h The types and facings the player picked for the build tools. */

#ifndef MINI_TOOLS_TOOL_CHOICES_H
#define MINI_TOOLS_TOOL_CHOICES_H

#include <span>
#include <string_view>

#include "../../bridge.h"
#include "../../direction_type.h"
#include "../../industry_type.h"
#include "../../rail_type.h"
#include "../../road_type.h"
#include "../../signal_type.h"

enum class ToolOption : uint8_t {
	RailType,
	RoadType,
	SignalType,
	AirportType,
	IndustryType,
	BridgeType,
	StopShape,
	Direction,
	StationAxis,
};

/* Every type choice belongs to the build panel instead of a picker window,
 * and a choice the game no longer offers falls back to a default. */
class ToolChoices {
public:
	RailType Rail() const;
	RoadType Road() const;
	BridgeType Bridge(uint len) const;
	std::span<const SignalType> Signals() const;
	SignalType Signal() const;
	uint8_t Airport() const;
	IndustryType Industry() const;
	bool StopThrough() const { return this->stop_through; }
	DiagDirection StopFacing() const;
	DiagDirection PointFacing() const { return this->point_dir; }
	Axis StationAxis() const;
	uint64_t Key() const;

	void Apply(ToolOption option, int value);
	void Turn(DiagDirDiff diff);

private:
	RailType rail = INVALID_RAILTYPE;
	/* Out of range until the player picks one, so the road tools default to
	 * the first plain road type and tram types stay opt-in. */
	RoadType road = INVALID_ROADTYPE;
	/* Out of range until the player picks one, so spans keep taking the
	 * fastest bridge the year allows. */
	BridgeType bridge = MAX_BRIDGES;
	SignalType signal = SIGTYPE_PBS;
	uint8_t airport = 0;
	IndustryType industry = 0;
	/* Road stops come in two shapes: a bay entered from one side, or a
	 * drive-through pair along an axis. Drive-through along X stays the
	 * default. */
	bool stop_through = true;
	DiagDirection stop_dir = DIAGDIR_NE;
	DiagDirection point_dir = DIAGDIR_SE;
	/* A square drag leaves the platform direction ambiguous and a rectangle
	 * can still want the short side, so the axis is derived and Q/E flips it. */
	bool station_flip = false;
};

extern ToolChoices _choices;

bool IndustryFundable(IndustryType it);
std::string_view SignalTypeLabel(SignalType t);

#endif /* MINI_TOOLS_TOOL_CHOICES_H */
