/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file hud_part.h A region of the HUD document, bound to a data model named after it. */

#ifndef MINI_UI_HUD_PART_H
#define MINI_UI_HUD_PART_H

#include "view.h"

namespace Rml { class Element; }

class HudPart : public View {
public:
	bool Attach(Rml::Context &context);
	void Place(Rml::Element *root) { this->root = root; }

protected:
	explicit HudPart(Rml::String region);

	Rml::Element *Root() const { return this->root; }

private:
	const Rml::String region;
	Rml::Element *root = nullptr;
};

#endif /* MINI_UI_HUD_PART_H */
