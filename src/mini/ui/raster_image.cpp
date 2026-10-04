/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file raster_image.cpp A panel element that shows pixels the game draws on the CPU. */

#include "../../stdafx.h"
#include "raster_image.h"

#include <RmlUi/Core.h>

#include "../../safeguards.h"

static const Rml::Vector2f UV_TOP_LEFT(0.0f, 0.0f);
static const Rml::Vector2f UV_BOTTOM_RIGHT(1.0f, 1.0f);

static Rml::byte Premultiplied(uint32_t channel, uint32_t alpha)
{
	return static_cast<Rml::byte>((channel & 0xFF) * alpha / 0xFF);
}

RasterImage::RasterImage(const Rml::String &tag) : TextureBox(tag)
{
}

/* RmlUi takes premultiplied RGBA bytes; the mini UI paints in 0xAARRGGBB. */
void RasterImage::Show(std::span<const uint32_t> argb, Rml::Vector2i size)
{
	this->rgba.clear();
	this->rgba.reserve(argb.size() * 4);
	for (uint32_t pixel : argb) {
		uint32_t alpha = pixel >> 24;
		this->rgba.insert(this->rgba.end(), {Premultiplied(pixel >> 16, alpha), Premultiplied(pixel >> 8, alpha), Premultiplied(pixel, alpha), static_cast<Rml::byte>(alpha)});
	}
	this->size = size;
	this->texture.Release();
}

void RasterImage::OnRender()
{
	if (this->rgba.empty()) return;
	Rml::RenderManager *render_manager = this->GetRenderManager();
	if (render_manager == nullptr) return;
	if (!this->texture) this->texture = render_manager->MakeCallbackTexture([this](const Rml::CallbackTextureInterface &texture_interface) { return this->Upload(texture_interface); });

	this->RenderTexture(this->ScreenRect(), this->texture, UV_TOP_LEFT, UV_BOTTOM_RIGHT);
}

bool RasterImage::Upload(const Rml::CallbackTextureInterface &texture_interface) const
{
	return texture_interface.GenerateTexture(this->rgba, this->size);
}
