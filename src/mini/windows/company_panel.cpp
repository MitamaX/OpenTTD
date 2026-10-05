/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file company_panel.cpp The player's company: the official company window and a roll-up of what it owns. */

#include "../../stdafx.h"
#include "company_panel.h"

#include "../../command_func.h"
#include "../../company_base.h"
#include "../../company_cmd.h"
#include "../../company_func.h"
#include "../../company_gui.h"
#include "../../core/format.hpp"
#include "../ui/ui_text.h"
#include "vehicle_types.h"
#include "window_links.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

enum CompanyTab : int {
	CT_OVERVIEW,
	CT_ASSETS,
};

static void OpenCompanyOverview(WindowNumber)
{
	ShowCompany(_local_company);
}

static LedgerLine Tally(Rml::String label, Rml::String value, uint amount)
{
	return {std::move(label), std::move(value), amount > 0 ? Tone::Plain : Tone::Dim};
}

CompanyPanel::CompanyPanel() : WindowPanel("company", {}, {"개요", "자산"})
{
}

bool CompanyPanel::IsAlive() const
{
	return Company::IsValidID(_local_company);
}

std::optional<EmbedTarget> CompanyPanel::Embed() const
{
	if (this->tab != CT_OVERVIEW) return std::nullopt;
	return EmbedTarget{{WC_COMPANY, OpenCompanyOverview}, _local_company};
}

bool CompanyPanel::Renamable() const
{
	return true;
}

void CompanyPanel::Rename(std::string name)
{
	Command<CMD_RENAME_COMPANY>::Post(STR_ERROR_CAN_T_CHANGE_COMPANY_NAME, std::move(name));
}

void CompanyPanel::Fill()
{
	const Company &company = *Company::Get(_local_company);
	this->title = GameText(STR_COMPANY_NAME, _local_company);
	if (this->tab == CT_ASSETS) this->FillAssets(company);

	TileIndex headquarters = company.location_of_HQ;
	this->commands = {
		{"본사 보기", headquarters != INVALID_TILE, [headquarters] { ScrollToTile(headquarters); }},
	};
}

void CompanyPanel::FillAssets(const Company &company)
{
	LedgerSection &fleet = this->Section(GameText(STR_COMPANY_VIEW_VEHICLES_TITLE));
	uint vehicles = 0;
	for (VehicleType type = VEH_BEGIN; type < VEH_COMPANY_END; type++) {
		uint amount = company.group_all[type].num_vehicle;
		vehicles += amount;
		fleet.Add(Tally(VehicleTypeName(type), fmt::format("{}대", amount), amount));
	}
	if (vehicles == 0) fleet.Add(LedgerLine::Text(GameText(STR_COMPANY_VIEW_VEHICLES_NONE), Tone::Dim));

	const CompanyInfrastructure &infrastructure = company.infrastructure;
	uint rail = infrastructure.GetRailTotal() + infrastructure.signal;
	uint road = infrastructure.GetRoadTotal() + infrastructure.GetTramTotal();
	this->Section(GameText(STR_COMPANY_VIEW_INFRASTRUCTURE)).lines = {
		Tally("선로", fmt::format("{}", rail), rail),
		Tally("도로", fmt::format("{}", road), road),
		Tally("수로", fmt::format("{}", infrastructure.water), infrastructure.water),
		Tally("역 타일", fmt::format("{}", infrastructure.station), infrastructure.station),
		Tally("공항", fmt::format("{}", infrastructure.airport), infrastructure.airport),
	};
}
