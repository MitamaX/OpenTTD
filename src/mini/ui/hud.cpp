/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file hud.cpp The screen-wide RmlUi documents holding the HUD regions, one per dock so a change lays out only its own dock. */

#include "../../stdafx.h"
#include "hud.h"

#include <RmlUi/Core.h>

#include "../gpu/frame_capture.h"

#include "../../safeguards.h"

static constexpr const char *HUD_DOCUMENTS_BOTTOM_UP[] = {"mini_ui/hud_map.rml", "mini_ui/hud_left.rml", "mini_ui/hud_right.rml", "mini_ui/hud_bottom.rml", "mini_ui/hud_notes.rml"};
static constexpr const char MAP_ONLY_CLASS[] = "map-only";

Hud::Hud(std::vector<std::unique_ptr<HudPart>> parts) : parts(std::move(parts))
{
}

static Rml::Element *FindRegion(const std::vector<Rml::ElementDocument *> &documents, const Rml::String &id)
{
	for (Rml::ElementDocument *document : documents) {
		if (Rml::Element *region = document->GetElementById(id); region != nullptr) return region;
	}
	return nullptr;
}

/* RmlUi binds the regions to their data models while parsing, so every model has to exist before the documents load. */
void Hud::Reset(Rml::Context *context)
{
	this->documents.clear();
	for (const auto &part : this->parts) part->Place(nullptr);
	if (context == nullptr) return;

	for (const auto &part : this->parts) {
		if (!part->Attach(*context)) return;
	}
	for (const char *path : HUD_DOCUMENTS_BOTTOM_UP) {
		Rml::ElementDocument *document = context->LoadDocument(path);
		if (document == nullptr) {
			this->documents.clear();
			return;
		}
		this->documents.push_back(document);
	}

	for (const auto &part : this->parts) part->Place(FindRegion(this->documents, part->ModelName()));
	for (Rml::ElementDocument *document : this->documents) {
		document->SetClass(MAP_ONLY_CLASS, _frame_capture.HidesHud());
		document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
	}
}

void Hud::Refresh()
{
	if (this->documents.empty()) return;
	for (const auto &part : this->parts) part->Refresh();
}

void Hud::ReloadStyleSheet()
{
	for (Rml::ElementDocument *document : this->documents) document->ReloadStyleSheet();
}
