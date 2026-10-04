/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file native_slot.h A panel element that shows the native screen lying under it. */

#ifndef MINI_UI_NATIVE_SLOT_H
#define MINI_UI_NATIVE_SLOT_H

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Geometry.h>
#include <RmlUi/Core/Texture.h>

class NativeSlot final : public Rml::Element {
public:
	static void Register();

	explicit NativeSlot(const Rml::String &tag);

	Rml::Rectanglei ScreenRect();

protected:
	void OnRender() override;

private:
	void Sample(Rml::RenderManager &render_manager, Rml::Rectanglei rect, Rml::Vector2i screen);

	Rml::Texture texture;
	Rml::Geometry geometry;
	Rml::Rectanglei sampled_rect;
	Rml::Vector2i sampled_screen;
};

#endif /* MINI_UI_NATIVE_SLOT_H */
