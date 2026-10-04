/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file window_bar.h The top-right corner: the window categories, the windows of the open one and the map overlay toggles. */

#ifndef MINI_HUD_WINDOW_BAR_H
#define MINI_HUD_WINDOW_BAR_H

#include <functional>

#include "../ui/hud_part.h"
#include "../ui/menu_tile.h"
#include "menu_shelf.h"
#include "window_catalog.h"

class WindowBar final : public HudPart {
public:
	using Opener = std::function<void(MiniWin win)>;

	explicit WindowBar(Opener open);

private:
	void Bind(Rml::DataModelConstructor &model) override;
	void Collect() override;
	void CollectWindows();
	void CollectOverlays();
	void MeasureBase();
	void Toggle(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);
	void Pick(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);
	void Overlay(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);

	const Opener open;
	Rml::Vector<MenuTile> categories;
	bool shelf = false;
	Rml::Vector<MenuTile> windows;
	Rml::Vector<MenuTile> overlays;
};

extern MenuShelf _window_shelf;

int WindowBarBottom();

#endif /* MINI_HUD_WINDOW_BAR_H */
