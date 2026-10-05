/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rml_renderer.cpp The upstream GL3 renderer, with the game's screen texture as one more image source. */

#include "../../stdafx.h"
#include "rml_renderer.h"

#include <RmlUi/Core/Core.h>

#include "../../safeguards.h"

/* The screen texture is remade whenever the window resizes, so RmlUi has to drop the old id and size. */
void RmlRenderer::SyncScreenTexture()
{
	if (this->lent_screen.name != 0 && this->lent_screen != _gpu.Screen()) Rml::ReleaseTexture(SCREEN_SOURCE, this);
}

Rml::TextureHandle RmlRenderer::LoadTexture(Rml::Vector2i &texture_dimensions, const Rml::String &source)
{
	if (source != SCREEN_SOURCE) return RenderInterface_GL3::LoadTexture(texture_dimensions, source);

	this->lent_screen = _gpu.Screen();
	texture_dimensions = Rml::Vector2i(static_cast<int>(this->lent_screen.size.width), static_cast<int>(this->lent_screen.size.height));
	return static_cast<Rml::TextureHandle>(this->lent_screen.name);
}

/* The mini UI owns the screen texture; RmlUi only borrows its id. */
void RmlRenderer::ReleaseTexture(Rml::TextureHandle texture_handle)
{
	if (texture_handle == this->lent_screen.name) {
		this->lent_screen = {};
		return;
	}
	RenderInterface_GL3::ReleaseTexture(texture_handle);
}
