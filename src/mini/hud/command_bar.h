/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file command_bar.h The bottom-right corner: the area commands, apart from construction. */

#ifndef MINI_HUD_COMMAND_BAR_H
#define MINI_HUD_COMMAND_BAR_H

#include "../ui/hud_part.h"
#include "../ui/menu_tile.h"

class CommandBar final : public HudPart {
public:
	CommandBar();

private:
	void Bind(Rml::DataModelConstructor &model) override;
	void Collect() override;
	void Pick(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);

	Rml::Vector<MenuTile> commands;
};

#endif /* MINI_HUD_COMMAND_BAR_H */
