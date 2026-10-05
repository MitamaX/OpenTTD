/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town_list_panel.cpp Every town, ranked by name, size or the company's standing. */

#include "../../stdafx.h"
#include "town_list_panel.h"

#include "../../company_base.h"
#include "../../company_func.h"
#include "../../town.h"
#include "../ui/ui_text.h"
#include "grades.h"
#include "window_links.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static DirectoryEntry TownEntry(const Town &town)
{
	DirectoryEntry entry;
	entry.name = GameText(STR_TOWN_NAME, town.index);
	entry.amount = town.cache.population;

	if (Company::IsValidID(_local_company) && town.have_ratings.Test(_local_company)) {
		int rating = town.ratings[_local_company];
		entry.grade = rating;
		entry.grade_text = GameText(TownRatingString(rating));
		entry.grade_tone = TownRatingTone(rating);
	}

	TownID id = town.index;
	entry.open = [id] { OpenTownWindow(id); };
	return entry;
}

TownListPanel::TownListPanel() : DirectoryPanel("town-list", "도시 목록", "인구", "평판")
{
}

std::vector<DirectoryEntry> TownListPanel::Entries() const
{
	std::vector<DirectoryEntry> entries;
	for (const Town *town : Town::Iterate()) entries.push_back(TownEntry(*town));
	return entries;
}

Rml::String TownListPanel::Emptiness() const
{
	return "도시 없음";
}
