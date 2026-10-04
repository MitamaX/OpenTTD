/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rml_input.cpp OpenTTD's pointer and key state in the shape RmlUi takes it. */

#include "../../stdafx.h"
#include "rml_input.h"

#include "../../gfx_func.h"

#include "../../safeguards.h"

using namespace Rml::Input;

struct KeyRange {
	uint first;
	uint last;
	KeyIdentifier identifier;
};

static constexpr KeyRange KEY_RANGES[] = {
	{WKC_ESC, WKC_ESC, KI_ESCAPE},
	{WKC_BACKSPACE, WKC_BACKSPACE, KI_BACK},
	{WKC_INSERT, WKC_DELETE, KI_INSERT},
	{WKC_PAGEUP, WKC_DOWN, KI_PRIOR},
	{WKC_RETURN, WKC_RETURN, KI_RETURN},
	{WKC_TAB, WKC_TAB, KI_TAB},
	{WKC_SPACE, WKC_SPACE, KI_SPACE},
	{WKC_F1, WKC_F12, KI_F1},
	{'0', '9', KI_0},
	{'A', 'Z', KI_A},
	{WKC_NUM_ENTER, WKC_NUM_ENTER, KI_NUMPADENTER},
};

static constexpr std::pair<WindowKeyCodes, int> KEY_MODIFIERS[] = {
	{WKC_CTRL, KM_CTRL},
	{WKC_SHIFT, KM_SHIFT},
	{WKC_ALT, KM_ALT},
	{WKC_META, KM_META},
};

static KeyIdentifier Identify(uint key)
{
	for (const KeyRange &range : KEY_RANGES) {
		if (key >= range.first && key <= range.last) return static_cast<KeyIdentifier>(range.identifier + key - range.first);
	}
	return KI_UNKNOWN;
}

RmlPointer RmlPointer::Current()
{
	RmlPointer pointer;
	pointer.x = _cursor.pos.x;
	pointer.y = _cursor.pos.y;
	pointer.buttons = {_left_button_down, _right_button_down, _middle_button_down};
	pointer.wheel = _cursor.wheel;
	pointer.modifiers = (_ctrl_pressed ? KM_CTRL : 0) | (_shift_pressed ? KM_SHIFT : 0);
	return pointer;
}

RmlKey RmlKey::FromKeycode(uint keycode)
{
	RmlKey key;
	key.identifier = Identify(keycode & ~WKC_SPECIAL_KEYS);
	for (const auto &[flag, modifier] : KEY_MODIFIERS) {
		if ((keycode & flag) != 0) key.modifiers |= modifier;
	}
	return key;
}
