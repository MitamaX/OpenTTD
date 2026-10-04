/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rml_renderer.cpp The upstream GL3 renderer, with the raylib screen texture as one more image source. */

#include "../../stdafx.h"
#include "rml_renderer.h"

#include <RmlUi/Core/Core.h>

#include "../../safeguards.h"

/* raylib recreates the screen texture whenever the window resizes, so RmlUi has to drop the old id and size. */
void RmlRenderer::SyncScreenTexture()
{
	if (this->lent_screen.id != 0 && this->lent_screen != RlwScreenTexture()) Rml::ReleaseTexture(SCREEN_SOURCE, this);
}

Rml::TextureHandle RmlRenderer::LoadTexture(Rml::Vector2i &texture_dimensions, const Rml::String &source)
{
	if (source != SCREEN_SOURCE) return RenderInterface_GL3::LoadTexture(texture_dimensions, source);

	this->lent_screen = RlwScreenTexture();
	texture_dimensions = Rml::Vector2i(this->lent_screen.width, this->lent_screen.height);
	return static_cast<Rml::TextureHandle>(this->lent_screen.id);
}

/* raylib owns the screen texture; RmlUi only borrows its id. */
void RmlRenderer::ReleaseTexture(Rml::TextureHandle texture_handle)
{
	if (texture_handle == this->lent_screen.id) {
		this->lent_screen = {};
		return;
	}
	RenderInterface_GL3::ReleaseTexture(texture_handle);
}
