/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ledger.h Label and value rows grouped under headings, the common body of mini panels. */

#ifndef MINI_UI_LEDGER_H
#define MINI_UI_LEDGER_H

#include <RmlUi/Core/Types.h>

enum class Tone : uint8_t {
	Plain,
	Accent,
	Warn,
	Loss,
	Dim,
};

struct LedgerLine {
	LedgerLine(Rml::String label, Rml::String value = {}, Tone tone = Tone::Plain);

	static LedgerLine Total(Rml::String label, Rml::String value, Tone tone = Tone::Plain);

	Rml::String label;
	Rml::String value;
	Rml::String tone;
	bool total = false;
};

struct LedgerSection {
	Rml::String title;
	Rml::Vector<LedgerLine> lines;
};

#endif /* MINI_UI_LEDGER_H */
