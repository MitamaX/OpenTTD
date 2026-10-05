/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file directory_panel.h A list of one kind of thing, ranked by name, by amount or by grade. */

#ifndef MINI_WINDOWS_DIRECTORY_PANEL_H
#define MINI_WINDOWS_DIRECTORY_PANEL_H

#include <vector>

#include "../ui/ledger_panel.h"

struct DirectoryEntry {
	Rml::String name;
	uint64_t amount = 0;
	int grade = std::numeric_limits<int>::min();
	Rml::String grade_text = "-";
	Tone grade_tone = Tone::Dim;
	Tone name_tone = Tone::Plain;
	LedgerLine::Action open;
};

class DirectoryPanel : public LedgerPanel {
protected:
	DirectoryPanel(std::string key, Rml::String title, Rml::String amount_tab, Rml::String grade_tab);

	void Collect() override;

	virtual std::vector<DirectoryEntry> Entries() const = 0;
	virtual Rml::String Emptiness() const = 0;

private:
	void Rank(std::vector<DirectoryEntry> &entries) const;
	LedgerLine Line(DirectoryEntry &entry) const;
};

#endif /* MINI_WINDOWS_DIRECTORY_PANEL_H */
