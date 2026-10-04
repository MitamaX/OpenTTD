/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file toast_stack.cpp The toast cards above the command bar. */

#include "../../stdafx.h"
#include "toast_stack.h"

#include <RmlUi/Core.h>

#include "../../core/format.hpp"
#include "../ui/tone.h"
#include "toast_feed.h"

#include "../../safeguards.h"

ToastStack::ToastStack(Follow follow) : HudPart("toasts"), follow(std::move(follow))
{
}

void ToastStack::Bind(Rml::DataModelConstructor &model)
{
	if (Rml::StructHandle<ToastCard> card = model.RegisterStruct<ToastCard>()) {
		card.RegisterMember("head", &ToastCard::head);
		card.RegisterMember("detail", &ToastCard::detail);
		card.RegisterMember("tone", &ToastCard::tone);
		card.RegisterMember("opacity", &ToastCard::opacity);
	}
	model.RegisterArray<Rml::Vector<ToastCard>>();
	model.Bind("cards", &this->cards);
	model.BindEventCallback("open", &ToastStack::Open, this);
}

void ToastStack::Collect()
{
	this->cards.clear();
	for (const Toast &t : _toast_feed.Items()) {
		Rml::String head = t.repeat > 1 ? fmt::format("{} ×{}", t.summary, t.repeat) : t.summary;
		this->cards.push_back({std::move(head), t.detail, ToneName(t.warn ? Tone::Loss : Tone::Warn), t.Opacity()});
	}
}

void ToastStack::Open(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	int index = ArgumentIndex(arguments);
	if (index < 0) return;
	std::optional<NewsReference> ref = _toast_feed.Dismiss(index);
	if (ref.has_value()) this->follow(*ref);
}
