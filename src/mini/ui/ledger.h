/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ledger.h Label and value rows grouped under headings, the common body of mini panels. */

#ifndef MINI_UI_LEDGER_H
#define MINI_UI_LEDGER_H

#include <functional>
#include <string>

#include <RmlUi/Core/Types.h>

#include "tone.h"

struct LedgerLine {
	using Action = std::function<void()>;
	using Renamer = std::function<void(std::string name)>;

	LedgerLine(Rml::String label, Rml::String value = {}, Tone tone = Tone::Plain);

	static LedgerLine Total(Rml::String label, Rml::String value, Tone tone = Tone::Plain);
	static LedgerLine Text(Rml::String text, Tone tone = Tone::Plain);

	LedgerLine &Tint(Tone tone);
	LedgerLine &OnClick(Action action);
	LedgerLine &Mark(bool active = true);
	LedgerLine &Renames(Rml::String key, Renamer renamer);

	Rml::String label;
	Rml::String value;
	Rml::String tone;
	Rml::String label_tone;
	Rml::String key;
	bool total = false;
	bool link = false;
	bool active = false;
	Action action;
	Renamer renamer;
};

struct LedgerSection {
	Rml::String title;
	Rml::Vector<LedgerLine> lines;

	LedgerLine &Add(LedgerLine line);
};

#endif /* MINI_UI_LEDGER_H */
