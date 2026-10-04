/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file hud.h The screen-wide RmlUi document holding every HUD region. */

#ifndef MINI_UI_HUD_H
#define MINI_UI_HUD_H

#include <memory>
#include <vector>

#include "hud_part.h"

namespace Rml { class ElementDocument; }

class Hud {
public:
	void Reset(Rml::Context *context);
	void Refresh();
	void ReloadStyleSheet();

private:
	std::vector<std::unique_ptr<HudPart>> parts;
	Rml::ElementDocument *document = nullptr;
};

#endif /* MINI_UI_HUD_H */
