/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file goal_list_panel.cpp The company's goals and the global ones; a row opens what the goal points at. */

#include "../../stdafx.h"
#include "goal_list_panel.h"

#include "../../company_func.h"
#include "../../goal_base.h"
#include "../../industry.h"
#include "../../map_func.h"
#include "../../town.h"
#include "../ui/ui_text.h"
#include "window_links.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

enum GoalListTab : int {
	GLT_COMPANY,
	GLT_GLOBAL,
};

/* Story pages have no mini window, so goals pointing at one stay inert. */
static void OpenTarget(GoalType type, GoalTypeID target)
{
	switch (type) {
		case GT_TILE:
			if (IsValidTile(TileIndex{target})) ScrollToTile(TileIndex{target});
			break;
		case GT_TOWN:
			if (Town::IsValidID(target)) OpenTownWindow(TownID(static_cast<uint16_t>(target)));
			break;
		case GT_INDUSTRY:
			if (Industry::IsValidID(target)) OpenIndustryWindow(IndustryID(static_cast<uint16_t>(target)));
			break;
		case GT_COMPANY:
			if (CompanyID(static_cast<uint8_t>(target)) == _local_company) OpenCompanyWindow();
			break;
		default:
			break;
	}
}

static LedgerLine GoalLine(const Goal &goal)
{
	std::string progress = goal.progress.empty() ? std::string() : StrMakeValid(goal.progress.GetDecodedString(), {});
	GoalType type = goal.type;
	GoalTypeID target = goal.dst;
	LedgerLine line(StrMakeValid(goal.text.GetDecodedString(), {}), std::move(progress), goal.completed ? Tone::Accent : Tone::Plain);
	line.Tint(goal.completed ? Tone::Dim : Tone::Plain).OnClick([type, target] { OpenTarget(type, target); });
	return line;
}

GoalListPanel::GoalListPanel() : LedgerPanel("goals", "목표", {"회사", "전체"})
{
}

void GoalListPanel::Fill()
{
	CompanyID owner = this->tab == GLT_GLOBAL ? CompanyID::Invalid() : _local_company;
	LedgerSection &list = this->Section();
	for (const Goal *goal : Goal::Iterate()) {
		if (goal->company == owner) list.Add(GoalLine(*goal));
	}
	if (list.lines.empty()) list.Add(LedgerLine::Text(GameText(STR_GOALS_NONE), Tone::Dim));
}
