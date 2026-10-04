/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ledger.cpp Label and value rows grouped under headings, the common body of mini panels. */

#include "../../stdafx.h"
#include "ledger.h"

#include "../../safeguards.h"

static Rml::String ToneName(Tone tone)
{
	switch (tone) {
		case Tone::Plain: return "plain";
		case Tone::Accent: return "accent";
		case Tone::Warn: return "warn";
		case Tone::Loss: return "loss";
		case Tone::Dim: return "dim";
	}
	NOT_REACHED();
}

LedgerLine::LedgerLine(Rml::String label, Rml::String value, Tone tone) :
	label(std::move(label)), value(std::move(value)), tone(ToneName(tone))
{
}

LedgerLine LedgerLine::Total(Rml::String label, Rml::String value, Tone tone)
{
	LedgerLine line(std::move(label), std::move(value), tone);
	line.total = true;
	return line;
}
