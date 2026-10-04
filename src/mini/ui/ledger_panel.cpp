/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ledger_panel.cpp A mini window whose body is a ledger of sections. */

#include "../../stdafx.h"
#include "ledger_panel.h"

#include "../../safeguards.h"

static constexpr const char LEDGER_DOCUMENT[] = "mini_ui/ledger.rml";

LedgerPanel::LedgerPanel(std::string key, Rml::String title, Rml::Vector<Rml::String> tabs) :
	Panel(std::move(key), LEDGER_DOCUMENT, std::move(title), std::move(tabs))
{
}

void LedgerPanel::Bind(Rml::DataModelConstructor &model)
{
	model.Bind("sections", &this->sections);
}
