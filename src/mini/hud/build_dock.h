/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file build_dock.h The bottom-left corner: the build categories, the tools of the open one and the choices of the tool in hand. */

#ifndef MINI_HUD_BUILD_DOCK_H
#define MINI_HUD_BUILD_DOCK_H

#include "../ui/hud_part.h"
#include "../ui/menu_tile.h"
#include "build_rows.h"
#include "menu_shelf.h"

class BuildDock final : public HudPart {
public:
	BuildDock();

private:
	void Bind(Rml::DataModelConstructor &model) override;
	void Collect() override;
	void CollectTools();
	void CollectChoices();
	void ScrollChoicesHome();
	void Toggle(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);
	void Pick(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);
	void Apply(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);

	Rml::Vector<MenuTile> categories;
	bool shelf = false;
	Rml::Vector<MenuTile> tools;
	Rml::String title;
	Rml::Vector<BuildRow> rows;
	MiniTool listed = MiniTool::None;
};

extern MenuShelf _build_shelf;

#endif /* MINI_HUD_BUILD_DOCK_H */
