/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rml_input.h OpenTTD's pointer and key state in the shape RmlUi takes it. */

#ifndef MINI_UI_RML_INPUT_H
#define MINI_UI_RML_INPUT_H

#include <RmlUi/Core/Input.h>

#include <array>

struct RmlPointer {
	static RmlPointer Current();

	int x = 0;
	int y = 0;
	std::array<bool, 3> buttons{};
	int wheel = 0;
	int modifiers = 0;
};

struct RmlKey {
	static RmlKey FromKeycode(uint keycode);

	Rml::Input::KeyIdentifier identifier = Rml::Input::KI_UNKNOWN;
	int modifiers = 0;
};

#endif /* MINI_UI_RML_INPUT_H */
