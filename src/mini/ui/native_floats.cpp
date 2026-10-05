/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file native_floats.cpp The topmost RmlUi document: a native slot over every official window that floats free of the panels. */

#include "../../stdafx.h"
#include "native_floats.h"

#include <RmlUi/Core.h>

#include "native_slot.h"
#include "pixel_style.h"

#include "../../safeguards.h"

static constexpr const char FLOATS_DOCUMENT[] = "mini_ui/floats.rml";

static void Place(Rml::Element &slot, const Rect &window)
{
	SetPixels(slot, Rml::PropertyId::Left, static_cast<float>(window.left));
	SetPixels(slot, Rml::PropertyId::Top, static_cast<float>(window.top));
	SetPixels(slot, Rml::PropertyId::Width, static_cast<float>(window.Width()));
	SetPixels(slot, Rml::PropertyId::Height, static_cast<float>(window.Height()));
}

void NativeFloats::Reset(Rml::Context *context)
{
	this->slots.clear();
	this->document = context == nullptr ? nullptr : context->LoadDocument(FLOATS_DOCUMENT);
	if (this->document != nullptr) this->document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
}

/* Windows come back to front, and later slots stack over earlier ones the same way. */
void NativeFloats::Show(std::span<const Rect> windows)
{
	if (this->document == nullptr) return;

	while (this->slots.size() < windows.size()) this->slots.push_back(this->document->AppendChild(this->document->CreateElement(NativeSlot::TAG)));
	while (this->slots.size() > windows.size()) {
		this->document->RemoveChild(this->slots.back());
		this->slots.pop_back();
	}
	for (size_t i = 0; i < windows.size(); i++) Place(*this->slots[i], windows[i]);
}
