/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file note_layer.h Text floating over the map: the pause banner and the note beside the cursor. */

#ifndef MINI_HUD_NOTE_LAYER_H
#define MINI_HUD_NOTE_LAYER_H

#include "../ui/hud_part.h"

/* The active tool announces itself beside the cursor: official name on top,
 * action and rotation hints below, live size while dragging, and what the
 * plan would charge. */
class NoteLayer final : public HudPart {
public:
	NoteLayer();

private:
	void Bind(Rml::DataModelConstructor &model) override;
	void Collect() override;
	void CollectCost();
	void FollowCursor();

	bool paused = false;
	Rml::String pause_text;
	bool tool_shown = false;
	Rml::String title;
	Rml::String hint;
	Rml::String cost;
	Rml::String cost_tone;
};

#endif /* MINI_HUD_NOTE_LAYER_H */
