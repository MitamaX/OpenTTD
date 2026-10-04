/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tone.h The emphasis a piece of text carries, named the way the stylesheets match it. */

#ifndef MINI_UI_TONE_H
#define MINI_UI_TONE_H

#include <RmlUi/Core/Types.h>

enum class Tone : uint8_t {
	Plain,
	Accent,
	Warn,
	Loss,
	Dim,
};

Rml::String ToneName(Tone tone);

#endif /* MINI_UI_TONE_H */
