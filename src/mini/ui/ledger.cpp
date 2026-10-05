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

LedgerLine LedgerLine::Text(Rml::String text, Tone tone)
{
	LedgerLine line(std::move(text));
	line.Tint(tone);
	return line;
}

LedgerLine LedgerLine::Strip(Rml::Vector<StripBlock> blocks, BlockAction on_block)
{
	LedgerLine line{Rml::String()};
	line.blocks = std::move(blocks);
	line.on_block = std::move(on_block);
	return line;
}

LedgerLine &LedgerLine::Tint(Tone tone)
{
	this->label_tone = ToneName(tone);
	return *this;
}

LedgerLine &LedgerLine::OnClick(Action action)
{
	this->action = std::move(action);
	this->link = true;
	return *this;
}

LedgerLine &LedgerLine::Mark(bool active)
{
	this->active = active;
	return *this;
}

LedgerLine &LedgerLine::Renames(Rml::String key, Renamer renamer)
{
	this->key = std::move(key);
	this->renamer = std::move(renamer);
	return *this;
}

LedgerLine &LedgerLine::OnHover(Action hover)
{
	this->hover = std::move(hover);
	return *this;
}

LedgerLine &LedgerSection::Add(LedgerLine line)
{
	return this->lines.emplace_back(std::move(line));
}
