/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file native_slot.cpp A panel element that shows the native screen lying under it. */

#include "../../stdafx.h"
#include "native_slot.h"

#include <RmlUi/Core.h>

#include "rml_renderer.h"

#include "../../safeguards.h"

NativeSlot::NativeSlot(const Rml::String &tag) : TextureBox(tag)
{
}

void NativeSlot::OnRender()
{
	Rml::RenderManager *render_manager = this->GetRenderManager();
	if (render_manager == nullptr) return;
	if (!this->screen) this->screen = render_manager->LoadTexture(RmlRenderer::SCREEN_SOURCE);

	Rml::Rectanglei rect = this->ScreenRect();
	Rml::Vector2f texels(Rml::Math::Max(this->screen.GetDimensions(), Rml::Vector2i(1)));
	Rml::Vector2f top_left = Rml::Vector2f(rect.TopLeft()) / texels;
	Rml::Vector2f bottom_right = Rml::Vector2f(rect.BottomRight()) / texels;
	/* The screen texture's rows run bottom up. */
	this->RenderTexture(rect, this->screen, {top_left.x, 1.0f - top_left.y}, {bottom_right.x, 1.0f - bottom_right.y});
}
