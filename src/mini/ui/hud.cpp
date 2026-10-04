/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file hud.cpp The screen-wide RmlUi document holding every HUD region. */

#include "../../stdafx.h"
#include "hud.h"

#include <RmlUi/Core.h>

#include "../../safeguards.h"

static constexpr const char HUD_DOCUMENT[] = "mini_ui/hud.rml";

/* RmlUi binds the regions to their data models while parsing, so every model has to exist before the document loads. */
void Hud::Reset(Rml::Context *context)
{
	this->document = nullptr;
	if (context == nullptr) return;

	for (const auto &part : this->parts) {
		if (!part->Attach(*context)) return;
	}
	this->document = context->LoadDocument(HUD_DOCUMENT);
	if (this->document != nullptr) this->document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
}

void Hud::Refresh()
{
	if (this->document == nullptr) return;
	for (const auto &part : this->parts) part->Refresh();
}

void Hud::ReloadStyleSheet()
{
	if (this->document != nullptr) this->document->ReloadStyleSheet();
}
