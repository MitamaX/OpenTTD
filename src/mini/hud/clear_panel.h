/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file clear_panel.h The clear tool's filter rows above the command bar. */

#ifndef MINI_HUD_CLEAR_PANEL_H
#define MINI_HUD_CLEAR_PANEL_H

#include "../ui/hud_part.h"

struct OptionRow {
	Rml::String label;
	bool active = false;
};

/* One row per transport system, so the drag takes that system off the area
 * and leaves the rest of the map standing. */
class ClearPanel final : public HudPart {
public:
	ClearPanel();

private:
	void Bind(Rml::DataModelConstructor &model) override;
	void Collect() override;
	void Pick(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);

	bool shown = false;
	Rml::Vector<OptionRow> rows;
};

#endif /* MINI_HUD_CLEAR_PANEL_H */
