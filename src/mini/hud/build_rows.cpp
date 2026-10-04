/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file build_rows.cpp Every variant the tool in hand can take, one row each, so a type is chosen by pointing at it. */

#include "../../stdafx.h"
#include "build_rows.h"

#include <span>

#include "../../airport.h"
#include "../../bridge.h"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../../core/format.hpp"
#include "../../direction_func.h"
#include "../../industrytype.h"
#include "../../newgrf_airport.h"
#include "../../rail.h"
#include "../../road.h"
#include "../../strings_func.h"
#include "../tools/build_tool.h"
#include "../tools/tool_choices.h"
#include "../ui/menu_tile.h"
#include "../ui/ui_text.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

/* The map is drawn with north-east up, so each diagonal direction is a
 * quarter turn clockwise from the one before it. */
static constexpr int DEGREES_PER_DIAGDIR = 90;

static constexpr DiagDirection FACING_TURNS[] = {DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_SW, DIAGDIR_NW};
static constexpr DiagDirection FACING_AXES[] = {DIAGDIR_SW, DIAGDIR_SE};

class RowList {
public:
	void Head(std::string_view text)
	{
		BuildRow row;
		row.text = text;
		row.head = true;
		this->rows.push_back(std::move(row));
	}

	void Choice(Rml::String text, ToolOption option, int value, bool active)
	{
		BuildRow row;
		row.text = std::move(text);
		row.option = to_underlying(option);
		row.value = value;
		row.active = active;
		this->rows.push_back(std::move(row));
	}

	void Facings(ToolOption option, std::span<const DiagDirection> facings, DiagDirection current, bool axis)
	{
		BuildRow row;
		row.option = to_underlying(option);
		row.mark = IconPath(axis ? "axis" : "arrow");
		for (DiagDirection d : facings) row.cells.push_back({d, fmt::format("rotate({}deg)", d * DEGREES_PER_DIAGDIR), d == current});
		this->rows.push_back(std::move(row));
	}

	Rml::Vector<BuildRow> Take() { return std::move(this->rows); }

private:
	Rml::Vector<BuildRow> rows;
};

static void AddRailTypes(RowList &list, const Company &company)
{
	list.Head("선로");
	for (RailType rt = RAILTYPE_BEGIN; rt != RAILTYPE_END; rt++) {
		if (company.avail_railtypes.Test(rt)) list.Choice(GameText(GetRailTypeInfo(rt)->strings.name), ToolOption::RailType, rt, rt == _choices.Rail());
	}
}

static void AddRoadTypes(RowList &list, const Company &company)
{
	list.Head("도로");
	for (RoadType rt = ROADTYPE_BEGIN; rt != ROADTYPE_END; rt++) {
		if (company.avail_roadtypes.Test(rt)) list.Choice(GameText(GetRoadTypeInfo(rt)->strings.name), ToolOption::RoadType, rt, rt == _choices.Road());
	}
}

static void AddStopShapes(RowList &list)
{
	bool through = _choices.StopThrough();
	list.Head("형태");
	list.Choice("통과", ToolOption::StopShape, 1, through);
	list.Choice("만입", ToolOption::StopShape, 0, !through);
	list.Head("방향");
	if (through) {
		list.Facings(ToolOption::Direction, FACING_AXES, _choices.StopFacing(), true);
	} else {
		list.Facings(ToolOption::Direction, FACING_TURNS, _choices.StopFacing(), false);
	}
}

static void AddPointFacings(RowList &list, MiniTool kind)
{
	list.Head("방향");
	if (kind == MiniTool::ShipDepot) {
		list.Facings(ToolOption::Direction, FACING_AXES, AxisToDiagDir(DiagDirToAxis(_choices.PointFacing())), true);
	} else {
		list.Facings(ToolOption::Direction, FACING_TURNS, _choices.PointFacing(), false);
	}
}

static void AddStationAxis(RowList &list)
{
	list.Head("승강장");
	list.Facings(ToolOption::StationAxis, FACING_AXES, AxisToDiagDir(_choices.StationAxis()), true);
}

static void AddSignalTypes(RowList &list)
{
	list.Head("신호");
	for (SignalType st : _choices.Signals()) list.Choice(Rml::String(SignalTypeLabel(st)), ToolOption::SignalType, st, st == _choices.Signal());
}

static void AddBridgeTypes(RowList &list, MiniTool kind)
{
	uint len = std::max(_tool.Plans().line.BridgeLength(), 1U);
	VehicleType vt = kind == MiniTool::RailBridge ? VEH_TRAIN : VEH_ROAD;
	list.Head("다리");
	for (BridgeType bt = 0; bt < MAX_BRIDGES; bt++) {
		if (!CheckBridgeAvailability(bt, len).Succeeded()) continue;
		const BridgeSpec *spec = GetBridgeSpec(bt);
		list.Choice(GameText(STR_SELECT_BRIDGE_INFO_NAME_MAX_SPEED, spec->material, PackVelocity(spec->speed, vt)), ToolOption::BridgeType, bt, bt == _choices.Bridge(len));
	}
}

static void AddAirportTypes(RowList &list)
{
	list.Head("공항");
	for (uint8_t i = 0; i < NUM_AIRPORTS; i++) {
		const AirportSpec *as = AirportSpec::Get(i);
		if (as->IsAvailable()) list.Choice(GameText(as->name), ToolOption::AirportType, i, i == _choices.Airport());
	}
}

static void AddIndustryTypes(RowList &list)
{
	list.Head("산업");
	for (IndustryType it = 0; it < NUM_INDUSTRYTYPES; it++) {
		if (!IndustryFundable(it)) continue;
		const IndustrySpec *indsp = GetIndustrySpec(it);
		list.Choice(fmt::format("{}  {}", GameText(indsp->name), GameText(STR_JUST_CURRENCY_LONG, indsp->GetConstructionCost())), ToolOption::IndustryType, it, it == _choices.Industry());
	}
}

Rml::Vector<BuildRow> CollectBuildRows(MiniTool kind)
{
	RowList list;
	const Company *company = Company::GetIfValid(_local_company);
	if (company != nullptr && ToolUsesRailType(kind)) AddRailTypes(list, *company);
	if (company != nullptr && ToolUsesRoadType(kind)) AddRoadTypes(list, *company);
	if (IsRoadStopTool(kind)) AddStopShapes(list);
	if (IsDirPointTool(kind)) AddPointFacings(list, kind);
	if (kind == MiniTool::Station) AddStationAxis(list);
	if (kind == MiniTool::Signal) AddSignalTypes(list);
	if (IsBridgeTool(kind)) AddBridgeTypes(list, kind);
	if (kind == MiniTool::Airport) AddAirportTypes(list);
	if (kind == MiniTool::Industry) AddIndustryTypes(list);
	return list.Take();
}
