/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file command_bar.cpp The bottom-right corner: the area commands, apart from construction. */

#include "../../stdafx.h"
#include "command_bar.h"

#include <RmlUi/Core.h>

#include "../input/input_mode.h"
#include "build_catalog.h"

#include "../../safeguards.h"

CommandBar::CommandBar() : HudPart("commands")
{
}

void CommandBar::Bind(Rml::DataModelConstructor &model)
{
	this->Expose(model, "commands", &this->commands);
	model.BindEventCallback("pick", &CommandBar::Pick, this);
}

void CommandBar::Collect()
{
	this->commands.clear();
	for (const MiniMenuItem &item : CommandItems()) this->commands.push_back(ToolTile(item));
}

void CommandBar::Pick(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	if (const MiniMenuItem *item = ArgumentItem(CommandItems(), arguments); item != nullptr) _mode.ToggleBuild(item->tool);
}
