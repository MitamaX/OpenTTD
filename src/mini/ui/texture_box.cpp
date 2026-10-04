/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file texture_box.cpp A panel element that fills its content box, snapped to whole pixels, with one texture. */

#include "../../stdafx.h"
#include "texture_box.h"

#include <RmlUi/Core.h>

#include "../../safeguards.h"

TextureBox::TextureBox(const Rml::String &tag) : Rml::Element(tag)
{
}

Rml::Rectanglei TextureBox::ScreenRect()
{
	Rml::Vector2f origin = this->GetAbsoluteOffset(Rml::BoxArea::Content).Round();
	Rml::Vector2f size = this->GetBox().GetSize(Rml::BoxArea::Content).Round();
	return Rml::Rectanglei::FromPositionSize(Rml::Vector2i(origin), Rml::Vector2i(size));
}

void TextureBox::RenderTexture(Rml::Rectanglei rect, Rml::Texture texture, Rml::Vector2f uv_top_left, Rml::Vector2f uv_bottom_right)
{
	Quad quad{rect.Size(), uv_top_left, uv_bottom_right};
	if (quad != this->built || !this->geometry) this->Build(quad);
	this->geometry.Render(Rml::Vector2f(rect.TopLeft()), texture);
}

void TextureBox::Build(const Quad &quad)
{
	Rml::RenderManager *render_manager = this->GetRenderManager();
	if (render_manager == nullptr) return;

	Rml::Mesh mesh = this->geometry.Release(Rml::Geometry::ReleaseMode::ClearMesh);
	Rml::MeshUtilities::GenerateQuad(mesh, Rml::Vector2f(0.0f), Rml::Vector2f(quad.size), Rml::ColourbPremultiplied(255), quad.uv_top_left, quad.uv_bottom_right);
	this->geometry = render_manager->MakeGeometry(std::move(mesh));
	this->built = quad;
}
