/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file subsidy_list_panel.cpp Subsidies on offer and awarded; a row opens the route's source. */

#include "../../stdafx.h"
#include "subsidy_list_panel.h"

#include "../../cargotype.h"
#include "../../company_func.h"
#include "../../core/format.hpp"
#include "../../industry.h"
#include "../../subsidy_base.h"
#include "../../timer/timer_game_economy.h"
#include "../../town.h"
#include "../ui/ui_text.h"
#include "window_links.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

enum SubsidyListTab : int {
	SLT_OFFERED,
	SLT_AWARDED,
};

static constexpr uint SUBSIDY_ENDING = 3;

static void OpenSource(const Source &source)
{
	switch (source.type) {
		case SourceType::Industry:
			if (Industry::IsValidID(source.ToIndustryID())) OpenIndustryWindow(source.ToIndustryID());
			break;
		case SourceType::Town:
			if (Town::IsValidID(source.ToTownID())) OpenTownWindow(source.ToTownID());
			break;
		default:
			break;
	}
}

static std::string Route(const Subsidy &subsidy)
{
	std::string route = fmt::format("{}  {} → {}",
			GameText(CargoSpec::Get(subsidy.cargo_type)->name),
			GameText(subsidy.src.GetFormat(), subsidy.src.id),
			GameText(subsidy.dst.GetFormat(), subsidy.dst.id));
	return subsidy.IsAwarded() ? fmt::format("{}  {}", GameText(STR_COMPANY_NAME, subsidy.awarded), route) : route;
}

/* The month in progress is not counted down yet, so it still counts as time left. */
static uint TimeLeft(const Subsidy &subsidy)
{
	return subsidy.remaining + 1;
}

static std::string TimeLeftText(uint left)
{
	return TimerGameEconomy::UsingWallclockUnits() ? fmt::format("{}분", left) : fmt::format("{}개월", left);
}

static LedgerLine SubsidyLine(const Subsidy &subsidy)
{
	uint left = TimeLeft(subsidy);
	Source source = subsidy.src;
	LedgerLine line(Route(subsidy), TimeLeftText(left), left <= SUBSIDY_ENDING ? Tone::Warn : Tone::Plain);
	line.Tint(subsidy.awarded == _local_company ? Tone::Accent : Tone::Plain).OnClick([source] { OpenSource(source); });
	return line;
}

SubsidyListPanel::SubsidyListPanel() : LedgerPanel("subsidies", "보조금", {"제안", "수주"})
{
}

void SubsidyListPanel::Collect()
{
	bool awarded = this->tab == SLT_AWARDED;
	this->sections.clear();
	LedgerSection &list = this->Section();
	for (const Subsidy *subsidy : Subsidy::Iterate()) {
		if (subsidy->IsAwarded() == awarded) list.Add(SubsidyLine(*subsidy));
	}
	if (list.lines.empty()) list.Add(LedgerLine::Text(awarded ? "수주한 보조금 없음" : "제안된 보조금 없음", Tone::Dim));
}
