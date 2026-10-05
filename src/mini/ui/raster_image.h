/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file raster_image.h A panel element that shows pixels the game draws on the CPU. */

#ifndef MINI_UI_RASTER_IMAGE_H
#define MINI_UI_RASTER_IMAGE_H

#include <RmlUi/Core/CallbackTexture.h>

#include <span>
#include <vector>

#include "texture_box.h"

class RasterImage final : public TextureBox {
public:
	static constexpr const char TAG[] = "raster-image";

	explicit RasterImage(const Rml::String &tag);

	void Show(std::span<const uint32_t> argb, Rml::Vector2i size);

protected:
	void OnRender() override;

private:
	bool Upload(const Rml::CallbackTextureInterface &texture_interface) const;

	std::vector<Rml::byte> rgba;
	Rml::Vector2i size;
	Rml::CallbackTexture texture;
};

#endif /* MINI_UI_RASTER_IMAGE_H */
