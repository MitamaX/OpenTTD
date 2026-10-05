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
	model.Bind("sections", &this->sections);
	model.Bind("camera", &this->camera);
	model.BindEventCallback("pick", &LedgerPanel::Pick, this);
	model.BindEventCallback("edit", &LedgerPanel::Edit, this);
}

void LedgerPanel::Collect()
{
	this->sections.clear();
	this->commands.clear();
	this->Fill();
}

void LedgerPanel::Apply(const Rml::String &key, std::string text)
{
	for (const LedgerSection &section : this->sections) {
		for (const LedgerLine &line : section.lines) {
			if (line.key != key || !line.renamer) continue;
			line.renamer(std::move(text));
			return;
		}
	}
	Panel::Apply(key, std::move(text));
}

LedgerSection &LedgerPanel::Section(Rml::String title)
{
	return this->sections.emplace_back(LedgerSection{std::move(title), {}});
}

const LedgerLine *LedgerPanel::LineAt(const Rml::VariantList &arguments) const
{
	const LedgerSection *section = ArgumentItem(std::span<const LedgerSection>(this->sections), arguments);
	if (section == nullptr) return nullptr;

	int line = ArgumentIndex(arguments, 1);
	return line >= 0 && static_cast<size_t>(line) < section->lines.size() ? &section->lines[line] : nullptr;
}

void LedgerPanel::Pick(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	if (const LedgerLine *line = this->LineAt(arguments); line != nullptr && line->action) line->action();
}

void LedgerPanel::Edit(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	if (const LedgerLine *line = this->LineAt(arguments); line != nullptr && line->renamer) this->BeginEdit(line->key, line->label);
}
