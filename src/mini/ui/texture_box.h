/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file texture_box.h A panel element that fills its content box, snapped to whole pixels, with one texture. */

#ifndef MINI_UI_TEXTURE_BOX_H
#define MINI_UI_TEXTURE_BOX_H

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Geometry.h>
#include <RmlUi/Core/Texture.h>

class TextureBox : public Rml::Element {
public:
	Rml::Rectanglei ScreenRect();

protected:
	explicit TextureBox(const Rml::String &tag);

	void RenderTexture(Rml::Rectanglei rect, Rml::Texture texture, Rml::Vector2f uv_top_left, Rml::Vector2f uv_bottom_right);

private:
	struct Quad {
		Rml::Vector2i size;
		Rml::Vector2f uv_top_left;
		Rml::Vector2f uv_bottom_right;

		bool operator==(const Quad &) const = default;
	};

	void Build(const Quad &quad);

	Rml::Geometry geometry;
	Quad built{};
};

#endif /* MINI_UI_TEXTURE_BOX_H */
