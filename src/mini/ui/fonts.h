/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fonts.h Font files every mini UI layer renders with. */

#ifndef MINI_UI_FONTS_H
#define MINI_UI_FONTS_H

#include <array>

inline constexpr const char MINI_FONT_REGULAR[] = "C:\\Windows\\Fonts\\malgun.ttf";
inline constexpr const char MINI_FONT_BOLD[] = "C:\\Windows\\Fonts\\malgunbd.ttf";

inline constexpr std::array<const char *, 2> MINI_FONTS = {MINI_FONT_REGULAR, MINI_FONT_BOLD};

#endif /* MINI_UI_FONTS_H */
