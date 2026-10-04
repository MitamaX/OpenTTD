/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file status_stream.h The problem rows hanging below the colony panel. */

#ifndef MINI_HUD_STATUS_STREAM_H
#define MINI_HUD_STATUS_STREAM_H

#include "../ui/hud_part.h"

struct StatusRow {
	int status = 0;
	Rml::String text;
	Rml::String tone;
	Rml::Vector<Rml::String> names;
};

/* A row shows the category and count, hovering lists the affected vehicles,
 * clicking cycles the camera through them. */
class StatusStream final : public HudPart {
public:
	StatusStream();

private:
	void Bind(Rml::DataModelConstructor &model) override;
	void Collect() override;
	void Cycle(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);

	Rml::Vector<StatusRow> rows;
};

#endif /* MINI_HUD_STATUS_STREAM_H */
