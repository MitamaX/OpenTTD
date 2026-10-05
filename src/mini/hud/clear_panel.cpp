/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file clear_panel.cpp The clear tool's filter rows above the command bar. */

#include "../../stdafx.h"
#include "clear_panel.h"

#include <RmlUi/Core.h>

#include "../tools/build_tool.h"
#include "../tools/clear_filter.h"

#include "../../safeguards.h"

ClearPanel::ClearPanel() : HudPart("clear")
{
}

void ClearPanel::Bind(Rml::DataModelConstructor &model)
{
	if (Rml::StructHandle<OptionRow> row = model.RegisterStruct<OptionRow>()) {
		row.RegisterMember("label", &OptionRow::label);
		row.RegisterMember("active", &OptionRow::active);
	}
	model.RegisterArray<Rml::Vector<OptionRow>>();
	this->Expose(model, "shown", &this->shown);
	this->Expose(model, "rows", &this->rows);
	model.BindEventCallback("pick", &ClearPanel::Pick, this);
}

void ClearPanel::Collect()
{
	this->shown = _tool.Kind() == MiniTool::Demolish;
	this->rows.clear();
	for (const MiniClearCategory &cat : ClearCategories()) this->rows.push_back({Rml::String(cat.label), _clear_filter.Mode() == cat.mode});
}

void ClearPanel::Pick(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	if (const MiniClearCategory *cat = ArgumentItem(ClearCategories(), arguments); cat != nullptr) _clear_filter.Select(cat->mode);
}
