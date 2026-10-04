/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tool_choices.cpp The types and facings the player picked for the build tools. */

#include "../../stdafx.h"
#include "tool_choices.h"

#include "../../company_base.h"
#include "../../company_func.h"
#include "../../industrytype.h"
#include "../../newgrf_airport.h"
#include "../../newgrf_industries.h"
#include "../../road.h"
#include "../../settings_type.h"
#include "build_tool.h"
#include "command_probe.h"

#include "../../safeguards.h"

/* The signal GUI setting decides which types the build panel offers. */
static const SignalType _signals_path[] = {SIGTYPE_PBS, SIGTYPE_PBS_ONEWAY};
static const SignalType _signals_all[] = {SIGTYPE_BLOCK, SIGTYPE_ENTRY, SIGTYPE_EXIT, SIGTYPE_COMBO, SIGTYPE_PBS, SIGTYPE_PBS_ONEWAY};

ToolChoices _choices;

RailType ToolChoices::Rail() const
{
	const Company *c = Company::GetIfValid(_local_company);
	if (c == nullptr) return RAILTYPE_RAIL;
	if (this->rail != INVALID_RAILTYPE && c->avail_railtypes.Test(this->rail)) return this->rail;
	/* Electric rail runs everything plain rail does, so once it exists it is
	 * the better default; mono and maglev stay an explicit choice. */
	if (c->avail_railtypes.Test(RAILTYPE_ELECTRIC)) return RAILTYPE_ELECTRIC;
	for (RailType rt = RAILTYPE_BEGIN; rt != RAILTYPE_END; rt++) {
		if (c->avail_railtypes.Test(rt)) return rt;
	}
	return RAILTYPE_RAIL;
}

RoadType ToolChoices::Road() const
{
	const Company *c = Company::GetIfValid(_local_company);
	if (c != nullptr) {
		if (this->road < ROADTYPE_END && c->avail_roadtypes.Test(this->road)) return this->road;
		for (RoadType rt = ROADTYPE_BEGIN; rt != ROADTYPE_END; rt++) {
			if (GetRoadTramType(rt) == RTT_ROAD && c->avail_roadtypes.Test(rt)) return rt;
		}
	}
	return ROADTYPE_ROAD;
}

static BridgeType FastestBridge(uint len)
{
	BridgeType best = 0;
	uint best_speed = 0;
	for (BridgeType bt = 0; bt < MAX_BRIDGES; bt++) {
		if (!CheckBridgeAvailability(bt, len).Succeeded()) continue;
		if (_bridge[bt].speed > best_speed) {
			best = bt;
			best_speed = _bridge[bt].speed;
		}
	}
	return best;
}

BridgeType ToolChoices::Bridge(uint len) const
{
	if (this->bridge < MAX_BRIDGES && CheckBridgeAvailability(this->bridge, len).Succeeded()) return this->bridge;
	return FastestBridge(len);
}

std::span<const SignalType> ToolChoices::Signals() const
{
	if (_settings_client.gui.signal_gui_mode == SIGNAL_GUI_ALL) return _signals_all;
	return _signals_path;
}

SignalType ToolChoices::Signal() const
{
	std::span<const SignalType> choices = this->Signals();
	for (SignalType t : choices) {
		if (t == this->signal) return t;
	}
	return choices.front();
}

uint8_t ToolChoices::Airport() const
{
	if (AirportSpec::Get(this->airport)->IsAvailable()) return this->airport;
	for (uint8_t i = 0; i < NUM_AIRPORTS; i++) {
		if (AirportSpec::Get(i)->IsAvailable()) return i;
	}
	return this->airport;
}

IndustryType ToolChoices::Industry() const
{
	if (IndustryFundable(this->industry)) return this->industry;
	for (IndustryType it = 0; it < NUM_INDUSTRYTYPES; it++) {
		if (IndustryFundable(it)) return it;
	}
	return IT_INVALID;
}

DiagDirection ToolChoices::StopFacing() const
{
	if (this->stop_through) return AxisToDiagDir(DiagDirToAxis(this->stop_dir));
	return this->stop_dir;
}

Axis ToolChoices::StationAxis() const
{
	const AreaPlan &area = _tool.Plans().area;
	return (area.Width() >= area.Height()) != this->station_flip ? AXIS_X : AXIS_Y;
}

uint64_t ToolChoices::Key() const
{
	ProbeKey key;
	key.Add(this->Rail());
	key.Add(this->Road());
	key.Add(this->Signal());
	key.Add(this->Airport());
	key.Add(this->bridge);
	key.Add(this->point_dir);
	key.Add(this->stop_dir);
	key.Add(this->stop_through);
	key.Add(this->station_flip);
	return key.Value();
}

void ToolChoices::Apply(ToolOption option, int value)
{
	switch (option) {
		case ToolOption::RailType: this->rail = (RailType)value; break;
		case ToolOption::RoadType: this->road = (RoadType)value; break;
		case ToolOption::SignalType: this->signal = (SignalType)value; break;
		case ToolOption::AirportType: this->airport = (uint8_t)value; break;
		case ToolOption::IndustryType: this->industry = (IndustryType)value; break;
		case ToolOption::BridgeType: this->bridge = (BridgeType)value; break;
		case ToolOption::StopShape: this->stop_through = value != 0; break;

		case ToolOption::Direction:
			if (IsRoadStopTool(_tool.Kind())) {
				this->stop_dir = (DiagDirection)value;
			} else {
				this->point_dir = (DiagDirection)value;
			}
			break;

		/* The axis follows the drag, so the flip is what a click can set. */
		case ToolOption::StationAxis:
			if (this->StationAxis() != DiagDirToAxis((DiagDirection)value)) this->station_flip = !this->station_flip;
			break;
	}
}

void ToolChoices::Turn(DiagDirDiff diff)
{
	MiniTool kind = _tool.Kind();
	if (kind == MiniTool::Station) {
		this->station_flip = !this->station_flip;
	} else if (IsRoadStopTool(kind)) {
		this->stop_dir = ChangeDiagDir(this->stop_dir, diff);
	} else if (IsDirPointTool(kind)) {
		this->point_dir = ChangeDiagDir(this->point_dir, diff);
	}
}

bool IndustryFundable(IndustryType it)
{
	if (it >= NUM_INDUSTRYTYPES) return false;
	const IndustrySpec *indsp = GetIndustrySpec(it);
	if (!indsp->enabled) return false;
	if (indsp->IsRawIndustry() && _settings_game.construction.raw_industry_construction == 0) return false;
	return GetIndustryProbabilityCallback(it, IACT_USERCREATION, 1) > 0;
}

std::string_view SignalTypeLabel(SignalType t)
{
	switch (t) {
		case SIGTYPE_ENTRY: return "ENTRY";
		case SIGTYPE_EXIT: return "EXIT";
		case SIGTYPE_COMBO: return "COMBO";
		case SIGTYPE_PBS: return "PATH";
		case SIGTYPE_PBS_ONEWAY: return "ONE-WAY PATH";
		default: return "BLOCK";
	}
}
