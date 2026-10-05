/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file screen_target.h The framebuffer the game paints its own screen into while the mini UI is up. */

#ifndef MINI_GPU_SCREEN_TARGET_H
#define MINI_GPU_SCREEN_TARGET_H

#include "texture_store.h"

class ScreenTarget {
public:
	bool Bind(Dimension size);
	void Unbind() const;
	void Release();

	TextureId Texture() const { return this->texture; }
	Dimension Size() const { return this->size; }

private:
	bool Build(Dimension size);

	uint32_t framebuffer = 0;
	TextureId texture = NO_TEXTURE;
	Dimension size{};
};

#endif /* MINI_GPU_SCREEN_TARGET_H */
