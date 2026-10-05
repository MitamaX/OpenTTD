/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file pixel_style.h Placing and sizing elements in whole screen pixels from code. */

#ifndef MINI_UI_PIXEL_STYLE_H
#define MINI_UI_PIXEL_STYLE_H

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Property.h>

/* Setting a property dirties the layout even when the value is unchanged, so an equal value is left alone. */
inline void SetPixels(Rml::Element &element, Rml::PropertyId id, float pixels)
{
	const Rml::Property *current = element.GetLocalProperty(id);
	if (current != nullptr && current->unit == Rml::Unit::PX && current->Get<float>() == pixels) return;
	element.SetProperty(id, Rml::Property(pixels, Rml::Unit::PX));
}

#endif /* MINI_UI_PIXEL_STYLE_H */
