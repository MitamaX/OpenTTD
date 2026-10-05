/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file industry_panel.cpp An industry: its output, the stations serving it, its needs and the official production graph. */

#include "../../stdafx.h"
#include "industry_panel.h"

#include "../../cargotype.h"
#include "../../core/format.hpp"
#include "../../core/math_func.hpp"
#include "../../graph_gui.h"
#include "../../gui.h"
#include "../../industry.h"
#include "../../station_base.h"
#include "../../window_type.h"
#include "../ui/ui_text.h"
#include "grades.h"
#include "window_links.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

void ShowIndustryCargoesWindow(IndustryType id);

enum IndustryTab : int {
	IT_OUTPUT,
	IT_STATIONS,
	IT_NEEDS,
	IT_PRODUCTION_GRAPH,
};

static void OpenProductionGraph(WindowNumber number)
{
	ShowIndustryProductionGraph(number);
}

IndustryPanel::IndustryPanel(IndustryID industry) :
	WindowPanel(fmt::format("industry{}", industry.base()), {}, {"상태", "역", GameText(STR_VEHICLE_DETAIL_TAB_INFORMATION), "생산"}),
	industry(industry)
{
}

bool IndustryPanel::IsAlive() const
{
	return Industry::IsValidID(this->industry);
}

std::optional<CameraShot> IndustryPanel::Camera() const
{
	return CameraShot{CarrierSubject::Industry, this->industry.base(), Industry::Get(this->industry)->location.GetCenterTile()};
}

std::optional<EmbedTarget> IndustryPanel::Embed() const
{
	if (this->tab != IT_PRODUCTION_GRAPH) return std::nullopt;
	return EmbedTarget{{WC_INDUSTRY_PRODUCTION, OpenProductionGraph}, this->industry};
}

void IndustryPanel::Fill()
{
	const Industry &industry = *Industry::Get(this->industry);
	this->title = GameText(STR_INDUSTRY_NAME, this->industry);
	switch (this->tab) {
		case IT_OUTPUT: this->FillOutput(industry); break;
		case IT_STATIONS: this->FillStations(industry); break;
		case IT_NEEDS: this->FillNeeds(industry); break;
		default: break;
	}
	this->FillCommands(industry);
}

void IndustryPanel::FillOutput(const Industry &industry)
{
	LedgerSection &output = this->Section();
	if (industry.prod_level == PRODLEVEL_CLOSURE) output.Add(LedgerLine::Text(GameText(STR_INDUSTRY_VIEW_INDUSTRY_ANNOUNCED_CLOSURE), Tone::Loss));

	bool produces = false;
	for (const auto &produced : industry.produced) {
		if (!IsValidCargoType(produced.cargo)) continue;
		produces = true;
		const auto &month = produced.history[LAST_MONTH];
		uint percent = ToPercent8(month.PctTransported());
		output.Add({GameText(CargoSpec::Get(produced.cargo)->name), fmt::format("{} · {}%", month.production, percent), ShareTone(percent)});
	}
	if (!produces) output.Add(LedgerLine::Text("생산 없음", Tone::Dim));

	if (industry.prod_level != PRODLEVEL_DEFAULT && industry.prod_level != PRODLEVEL_CLOSURE) {
		output.Add(LedgerLine::Text(GameText(STR_INDUSTRY_VIEW_PRODUCTION_LEVEL, RoundDivSU(industry.prod_level * 100, PRODLEVEL_DEFAULT))));
	}
}

void IndustryPanel::FillStations(const Industry &industry)
{
	LedgerSection &stations = this->Section();
	for (const Station *st : industry.stations_near) {
		StationID station = st->index;
		stations.Add(LedgerLine(GameText(STR_STATION_NAME, station)).Tint(Tone::Plain).OnClick([station] { OpenStationWindow(station); }));
	}
	if (stations.lines.empty()) stations.Add(LedgerLine::Text("주변 역 없음", Tone::Dim));
}

void IndustryPanel::FillNeeds(const Industry &industry)
{
	this->Section().Add(LedgerLine::Text(GameText(STR_LAND_AREA_INFORMATION_BUILD_DATE, industry.construction_date)));

	LedgerSection &needs = this->Section(GameText(STR_INDUSTRY_VIEW_REQUIRES));
	for (const auto &accepted : industry.accepted) {
		if (!IsValidCargoType(accepted.cargo)) continue;
		needs.Add({GameText(CargoSpec::Get(accepted.cargo)->name), accepted.waiting > 0 ? fmt::format("{}", accepted.waiting) : std::string("-")});
	}
	if (needs.lines.empty()) this->sections.pop_back();
}

void IndustryPanel::FillCommands(const Industry &industry)
{
	IndustryType type = industry.type;
	TileIndex centre = industry.location.GetCenterTile();
	this->commands = {
		{"계통", true, [type] { ShowIndustryCargoesWindow(type); }},
		{"이동", true, [centre] { ScrollToTile(centre); }},
	};
}
