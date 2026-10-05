/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file industry_list_panel.cpp Every industry, ranked by name, output or share transported, and the official cargo chain. */

#include "../../stdafx.h"
#include "industry_list_panel.h"

#include "../../core/format.hpp"
#include "../../core/math_func.hpp"
#include "../../gui.h"
#include "../../industry.h"
#include "../../window_type.h"
#include "../ui/ui_text.h"
#include "grades.h"
#include "window_links.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static constexpr int CHAIN_TAB = 3;

static void OpenCargoChain(WindowNumber)
{
	ShowIndustryCargoesWindow();
}

/* Several cargoes from one industry weigh their transported share by how much of each was made. */
static DirectoryEntry IndustryEntry(const Industry &industry)
{
	DirectoryEntry entry;
	entry.name = GameText(STR_INDUSTRY_NAME, industry.index);
	entry.name_tone = industry.prod_level == PRODLEVEL_CLOSURE ? Tone::Loss : Tone::Plain;

	uint64_t weighted = 0;
	for (const auto &produced : industry.produced) {
		if (!IsValidCargoType(produced.cargo)) continue;
		const auto &month = produced.history[LAST_MONTH];
		entry.amount += month.production;
		weighted += static_cast<uint64_t>(month.production) * ToPercent8(month.PctTransported());
	}
	uint percent = entry.amount > 0 ? static_cast<uint>(weighted / entry.amount) : 0;
	entry.grade = percent;
	entry.grade_text = fmt::format("{}%", percent);
	entry.grade_tone = ShareTone(percent);

	IndustryID id = industry.index;
	entry.open = [id] { OpenIndustryWindow(id); };
	return entry;
}

IndustryListPanel::IndustryListPanel() : DirectoryPanel("industry-list", "산업 목록", "생산", "수송", {"연쇄"})
{
}

std::optional<EmbedTarget> IndustryListPanel::Embed() const
{
	if (this->tab != CHAIN_TAB) return std::nullopt;
	return EmbedTarget{{WC_INDUSTRY_CARGOES, OpenCargoChain}};
}

std::vector<DirectoryEntry> IndustryListPanel::Entries() const
{
	std::vector<DirectoryEntry> entries;
	for (const Industry *industry : Industry::Iterate()) entries.push_back(IndustryEntry(*industry));
	return entries;
}

Rml::String IndustryListPanel::Emptiness() const
{
	return "산업 없음";
}
