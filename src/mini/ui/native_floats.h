/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file native_floats.h The topmost RmlUi document: a native slot over every official window that floats free of the panels. */

#ifndef MINI_UI_NATIVE_FLOATS_H
#define MINI_UI_NATIVE_FLOATS_H

#include <span>
#include <vector>

#include "../../core/geometry_type.hpp"

namespace Rml { class Context; class Element; class ElementDocument; }

class NativeFloats {
public:
	void Reset(Rml::Context *context);
	void Show(std::span<const Rect> windows);

private:
	Rml::ElementDocument *document = nullptr;
	std::vector<Rml::Element *> slots;
};

#endif /* MINI_UI_NATIVE_FLOATS_H */
