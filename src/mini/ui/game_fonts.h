/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file game_fonts.h The font the game's own font search settled on, read out for RmlUi. */

#ifndef MINI_UI_GAME_FONTS_H
#define MINI_UI_GAME_FONTS_H

#include <RmlUi/Core/StyleTypes.h>

#include <vector>

struct FontFace {
	std::vector<uint8_t> data;
	Rml::Style::FontWeight weight;
};

std::vector<FontFace> GameFontFaces();

#endif /* MINI_UI_GAME_FONTS_H */
