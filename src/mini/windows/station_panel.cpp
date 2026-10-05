/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file station_panel.cpp A station: waiting cargo, the industries around it, the vehicles calling at it and its facts. */

#include "../../stdafx.h"
#include "station_panel.h"

#include "../../cargotype.h"
#include "../../command_func.h"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../../core/format.hpp"
#include "../../core/math_func.hpp"
#include "../../industry.h"
#include "../../map_func.h"
#include "../../station_base.h"
#include "../../station_cmd.h"
#include "../../station_func.h"
#include "../../town.h"
#include "../../vehicle_base.h"
#include "../../vehiclelist.h"
#include "../../viewport_func.h"
#include "../ui/ui_text.h"
#include "grades.h"
#include "window_links.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

extern const Station *_viewport_highlight_station;

enum StationTab : int {
	ST_CARGO,
	ST_INDUSTRIES,
	ST_VEHICLES,
	ST_FACTS,
};

struct FacilityName {
	StationFacility facility;
	std::string_view name;
};

static constexpr FacilityName FACILITY_NAMES[] = {
	{StationFacility::Train, "철도"},
	{StationFacility::BusStop, "버스"},
	{StationFacility::TruckStop, "트럭"},
	{StationFacility::Dock, "부두"},
	{StationFacility::Airport, "공항"},
};

static constexpr VehicleType CALLING_TYPES[] = {VEH_TRAIN, VEH_ROAD, VEH_SHIP, VEH_AIRCRAFT};

static TileIndex StationCentre(const Station &st)
{
	if (st.rect.IsEmpty()) return st.xy;
	return TileXY((st.rect.left + st.rect.right) / 2, (st.rect.top + st.rect.bottom) / 2);
}

static bool ProducesCargo(const Industry &industry)
{
	return std::ranges::any_of(industry.produced, [](const auto &p) { return IsValidCargoType(p.cargo); });
}

static LedgerLine IndustryLine(const Industry &industry)
{
	TileIndex tile = industry.location.tile;
	LedgerLine line(GameText(STR_INDUSTRY_NAME, industry.index));
	line.Tint(Tone::Plain).OnClick([tile] { ScrollToTile(tile); });
	return line;
}

static std::string FacilitiesText(const Station &st)
{
	std::string text;
	for (const FacilityName &entry : FACILITY_NAMES) {
		if (!st.facilities.Test(entry.facility)) continue;
		if (!text.empty()) text += " ";
		text += entry.name;
	}
	return text;
}

static uint LongestPlatform(const Station &st)
{
	uint longest = 0;
	for (TileIndex tile : st.train_station) {
		if (st.TileBelongsToRailStation(tile)) longest = std::max(longest, st.GetPlatformLength(tile));
	}
	return longest;
}

StationPanel::StationPanel(StationID station) :
	WindowPanel(fmt::format("station{}", station.base()), {}, {"상태", GameText(STR_SMALLMAP_TYPE_INDUSTRIES), GameText(STR_SMALLMAP_TYPE_VEHICLES), GameText(STR_VEHICLE_DETAIL_TAB_INFORMATION)}),
	station(station)
{
}

bool StationPanel::IsAlive() const
{
	return Station::IsValidID(this->station);
}

std::optional<CameraShot> StationPanel::Camera() const
{
	return CameraShot{CarrierSubject::Station, this->station.base(), StationCentre(*Station::Get(this->station))};
}

bool StationPanel::Renamable() const
{
	return Station::Get(this->station)->owner == _local_company;
}

void StationPanel::Rename(std::string name)
{
	Command<CMD_RENAME_STATION>::Post(STR_ERROR_CAN_T_RENAME_STATION, this->station, std::move(name));
}

void StationPanel::Fill()
{
	const Station &st = *Station::Get(this->station);
	this->title = GameText(STR_STATION_NAME, this->station);
	switch (this->tab) {
		case ST_CARGO: this->FillCargo(st); break;
		case ST_INDUSTRIES: this->FillIndustries(st); break;
		case ST_VEHICLES: this->FillVehicles(st); break;
		case ST_FACTS: this->FillFacts(st); break;
	}
	this->FillCommands(st);
}

/* The rating decides how much of what waits actually gets picked up, so it belongs beside the count. */
void StationPanel::FillCargo(const Station &st)
{
	LedgerSection &cargo = this->Section();
	for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
		const GoodsEntry &goods = st.goods[cs->Index()];
		if (!goods.HasRating()) continue;
		uint percent = ToPercent8(goods.rating);
		cargo.Add({GameText(cs->name), fmt::format("{}  {}%", goods.TotalCount(), percent), ShareTone(percent)});
	}
	if (cargo.lines.empty()) cargo.Add(LedgerLine::Text("대기 화물 없음", Tone::Dim));
}

void StationPanel::FillIndustries(const Station &st)
{
	std::vector<const Industry *> suppliers;
	for (const Industry *industry : Industry::Iterate()) {
		if (industry->stations_near.contains(const_cast<Station *>(&st)) && ProducesCargo(*industry)) suppliers.push_back(industry);
	}

	if (!suppliers.empty()) {
		LedgerSection &supply = this->Section("공급처");
		for (const Industry *industry : suppliers) supply.Add(IndustryLine(*industry));
	}
	if (!st.industries_near.empty()) {
		LedgerSection &delivery = this->Section("납품처");
		for (const IndustryListEntry &entry : st.industries_near) delivery.Add(IndustryLine(*entry.industry));
	}
	if (this->Sections().empty()) this->Section().Add(LedgerLine::Text("주변 산업 없음", Tone::Dim));
}

void StationPanel::FillVehicles(const Station &st)
{
	LedgerSection &calling = this->Section();
	for (VehicleType type : CALLING_TYPES) {
		VehicleList list;
		if (!GenerateVehicleSortList(&list, VehicleListIdentifier(VL_STATION_LIST, type, _local_company, st.index))) continue;
		for (const Vehicle *v : list) {
			bool heading_here = v->current_order.IsType(OT_GOTO_STATION) && v->current_order.GetDestination().ToStationID() == st.index;
			VehicleID head = v->First()->index;
			calling.Add(LedgerLine(GameText(STR_VEHICLE_NAME, v->index)).Tint(heading_here ? Tone::Accent : Tone::Plain).OnClick([head] { OpenVehicleWindow(head); }));
		}
	}
	if (calling.lines.empty()) calling.Add(LedgerLine::Text("이 역에 오는 차량 없음", Tone::Dim));
}

void StationPanel::FillFacts(const Station &st)
{
	LedgerSection &facts = this->Section();
	if (st.town != nullptr) {
		TownID town = st.town->index;
		facts.Add(LedgerLine("도시", GameText(STR_TOWN_NAME, town)).OnClick([town] { OpenTownWindow(town); }));
	}
	if (Company::IsValidID(st.owner)) facts.Add({"소유", GameText(STR_COMPANY_NAME, st.owner)});
	if (std::string facilities = FacilitiesText(st); !facilities.empty()) facts.Add({"시설", std::move(facilities)});
	facts.Add(LedgerLine::Text(GameText(STR_LAND_AREA_INFORMATION_BUILD_DATE, st.build_date)));
	if (st.facilities.Test(StationFacility::Train) && st.train_station.tile != INVALID_TILE) {
		facts.Add({GameText(STR_STATION_BUILD_PLATFORM_LENGTH), fmt::format("{}칸", LongestPlatform(st))});
	}
	facts.Add(LedgerLine::Text(GameText(STR_STATION_VIEW_ACCEPTS_CARGO, GetAcceptanceMask(&st))));

	LedgerSection &ratings = this->Section("화물 처리 평가");
	for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
		const GoodsEntry &goods = st.goods[cs->Index()];
		if (!goods.HasRating()) continue;
		uint percent = ToPercent8(goods.rating);
		ratings.Add({GameText(cs->name), fmt::format("{} {}%", GameText(STR_CARGO_RATING_APPALLING + (goods.rating >> 5)), percent), ShareTone(percent)});
	}
	this->DropEmptySection();
}

void StationPanel::FillCommands(const Station &st)
{
	bool own = st.owner == _local_company || st.owner == OWNER_NONE;
	bool highlighted = _viewport_highlight_station == &st;
	StationID station = st.index;
	TileIndex tile = st.xy;
	this->commands = {
		{"범위", own, [station, highlighted] { SetViewportCatchmentStation(Station::GetIfValid(station), !highlighted); }, highlighted},
		{"이동", true, [tile] { ScrollToTile(tile); }},
	};
}
