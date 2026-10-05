/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file directory_panel.cpp A list of one kind of thing, ranked by name, by amount or by grade. */

#include "../../stdafx.h"
#include "directory_panel.h"

#include "../../core/format.hpp"

#include "../../safeguards.h"

enum DirectoryTab : int {
	DT_NAME,
	DT_AMOUNT,
	DT_GRADE,
};

static Rml::Vector<Rml::String> DirectoryTabs(Rml::String amount_tab, Rml::String grade_tab, Rml::Vector<Rml::String> more_tabs)
{
	Rml::Vector<Rml::String> tabs = {"이름", std::move(amount_tab), std::move(grade_tab)};
	std::ranges::move(more_tabs, std::back_inserter(tabs));
	return tabs;
}

DirectoryPanel::DirectoryPanel(std::string key, Rml::String title, Rml::String amount_tab, Rml::String grade_tab, Rml::Vector<Rml::String> more_tabs) :
	WindowPanel(std::move(key), std::move(title), DirectoryTabs(std::move(amount_tab), std::move(grade_tab), std::move(more_tabs)))
{
}

void DirectoryPanel::Fill()
{
	if (this->tab > DT_GRADE) return;

	LedgerSection &list = this->Section();

	std::vector<DirectoryEntry> entries = this->Entries();
	if (entries.empty()) {
		list.Add(LedgerLine::Text(this->Emptiness(), Tone::Dim));
		return;
	}

	this->Rank(entries);
	for (DirectoryEntry &entry : entries) list.Add(this->Line(entry));
}

void DirectoryPanel::Rank(std::vector<DirectoryEntry> &entries) const
{
	switch (this->tab) {
		case DT_AMOUNT: std::ranges::sort(entries, std::greater{}, &DirectoryEntry::amount); break;
		case DT_GRADE: std::ranges::sort(entries, std::less{}, &DirectoryEntry::grade); break;
		default: std::ranges::sort(entries, std::less{}, &DirectoryEntry::name); break;
	}
}

LedgerLine DirectoryPanel::Line(DirectoryEntry &entry) const
{
	LedgerLine line = this->tab == DT_GRADE ? LedgerLine(entry.name, entry.grade_text, entry.grade_tone) : LedgerLine(entry.name, fmt::format("{}", entry.amount));
	line.Tint(entry.name_tone).OnClick(std::move(entry.open));
	return line;
}
