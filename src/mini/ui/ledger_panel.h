/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ledger_panel.h A mini window whose body is a ledger of sections. */

#ifndef MINI_UI_LEDGER_PANEL_H
#define MINI_UI_LEDGER_PANEL_H

#include "ledger.h"
#include "panel.h"

class LedgerPanel : public Panel {
protected:
	LedgerPanel(std::string key, Rml::String title, Rml::Vector<Rml::String> tabs);

	void BindSheet(Rml::DataModelConstructor &model) override;
	void Collect() override;
	void Apply(const Rml::String &key, std::string text) override;

	virtual void Fill() = 0;

	LedgerSection &Section(Rml::String title = {});
	Rml::Vector<LedgerSection> &Sections() { return this->columns.back().sections; }
	void NextColumn() { this->columns.emplace_back(); }
	void DropEmptySection();

	bool camera = false;

private:
	const LedgerLine *LineAt(const Rml::VariantList &arguments) const;
	void Pick(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);
	void Edit(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);
	void Hover(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);
	void PickBlock(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);

	Rml::Vector<LedgerColumn> columns;
};

#endif /* MINI_UI_LEDGER_PANEL_H */
