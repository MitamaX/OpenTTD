/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ledger_panel.cpp A mini window whose body is a ledger of sections. */

#include "../../stdafx.h"
#include "ledger_panel.h"

#include <RmlUi/Core.h>

#include "../../safeguards.h"

static constexpr const char LEDGER_DOCUMENT[] = "mini_ui/ledger.rml";

LedgerPanel::LedgerPanel(std::string key, Rml::String title, Rml::Vector<Rml::String> tabs) :
	Panel(std::move(key), LEDGER_DOCUMENT, std::move(title), std::move(tabs))
{
}

void LedgerPanel::BindSheet(Rml::DataModelConstructor &model)
{
	this->Expose(model, "columns", &this->columns);
	this->Expose(model, "camera", &this->camera);
	model.BindEventCallback("pick", &LedgerPanel::Pick, this);
	model.BindEventCallback("edit", &LedgerPanel::Edit, this);
	model.BindEventCallback("hover", &LedgerPanel::Hover, this);
	model.BindEventCallback("block", &LedgerPanel::PickBlock, this);
}

void LedgerPanel::Collect()
{
	this->columns.clear();
	this->NextColumn();
	this->commands.clear();
	this->Fill();
}

void LedgerPanel::Apply(const Rml::String &key, std::string text)
{
	for (const LedgerColumn &column : this->columns) {
		for (const LedgerSection &section : column.sections) {
			for (const LedgerLine &line : section.lines) {
				if (line.key != key || !line.renamer) continue;
				line.renamer(std::move(text));
				return;
			}
		}
	}
	Panel::Apply(key, std::move(text));
}

LedgerSection &LedgerPanel::Section(Rml::String title)
{
	return this->Sections().emplace_back(LedgerSection{std::move(title), {}});
}

/* A heading over nothing reads as a mistake, so a section left without rows goes. */
void LedgerPanel::DropEmptySection()
{
	Rml::Vector<LedgerSection> &sections = this->Sections();
	if (!sections.empty() && sections.back().lines.empty()) sections.pop_back();
}

/* Rows are addressed by column, section and line, in that order. */
const LedgerLine *LedgerPanel::LineAt(const Rml::VariantList &arguments) const
{
	const LedgerColumn *column = ArgumentItem(std::span<const LedgerColumn>(this->columns), arguments);
	if (column == nullptr) return nullptr;

	int section = ArgumentIndex(arguments, 1);
	if (section < 0 || static_cast<size_t>(section) >= column->sections.size()) return nullptr;

	const Rml::Vector<LedgerLine> &lines = column->sections[section].lines;
	int line = ArgumentIndex(arguments, 2);
	return line >= 0 && static_cast<size_t>(line) < lines.size() ? &lines[line] : nullptr;
}

void LedgerPanel::Pick(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	if (const LedgerLine *line = this->LineAt(arguments); line != nullptr && line->action) line->action();
}

void LedgerPanel::Edit(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	if (const LedgerLine *line = this->LineAt(arguments); line != nullptr && line->renamer) this->BeginEdit(line->key, line->label);
}

void LedgerPanel::Hover(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	if (const LedgerLine *line = this->LineAt(arguments); line != nullptr && line->hover) line->hover();
}

void LedgerPanel::PickBlock(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	const LedgerLine *line = this->LineAt(arguments);
	int block = ArgumentIndex(arguments, 3);
	if (line != nullptr && line->on_block && block >= 0 && static_cast<size_t>(block) < line->blocks.size()) line->on_block(block);
}
