/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file native_panel.cpp A panel that adopts a standalone official window, so it wears mini chrome instead of its own frame. */

#include "../../stdafx.h"
#include "native_panel.h"

#include "../../core/format.hpp"
#include "../../window_func.h"
#include "../../window_gui.h"
#include "../dock/native_window.h"
#include "../ui/view_host.h"

#include "../../safeguards.h"

static constexpr const char UNTITLED[] = "창";

NativePanel::NativePanel(WindowClass window_class, WindowNumber number) :
	WindowPanel(fmt::format("native{}:{}", to_underlying(window_class), static_cast<int32_t>(number)), {}, {}),
	window_class(window_class), number(number)
{
}

/* Every standalone official window on screen gets a panel, so nothing is left wearing the official frame. */
void NativePanel::AdoptAll(ViewHost &views)
{
	for (Window *w : Window::Iterate()) {
		if (NativeWrappable(w) && _dock.Find(w) == nullptr) views.Show(std::make_unique<NativePanel>(w->window_class, w->window_number));
	}
}

bool NativePanel::IsAlive() const
{
	return FindWindowById(this->window_class, this->number) != nullptr;
}

std::optional<EmbedTarget> NativePanel::Embed() const
{
	return EmbedTarget{{this->window_class, nullptr}, this->number};
}

void NativePanel::Fill()
{
	std::string caption = NativeCaption(FindWindowById(this->window_class, this->number));
	this->title = caption.empty() ? Rml::String(UNTITLED) : caption;
}

void NativePanel::OnDismiss()
{
	if (Window *w = FindWindowById(this->window_class, this->number); w != nullptr) w->Close();
}
