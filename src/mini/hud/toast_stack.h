/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file toast_stack.h The toast cards above the command bar. */

#ifndef MINI_HUD_TOAST_STACK_H
#define MINI_HUD_TOAST_STACK_H

#include <functional>

#include "../../news_type.h"
#include "../ui/hud_part.h"

struct ToastCard {
	Rml::String head;
	Rml::String detail;
	Rml::String tone;
	float opacity = 1.0f;
};

class ToastStack final : public HudPart {
public:
	using Follow = std::function<void(const NewsReference &ref)>;

	explicit ToastStack(Follow follow);

private:
	void Bind(Rml::DataModelConstructor &model) override;
	void Collect() override;
	void Open(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);

	const Follow follow;
	Rml::Vector<ToastCard> cards;
};

#endif /* MINI_HUD_TOAST_STACK_H */
