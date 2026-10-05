/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file draw_pass.h The GL program that puts a draw list on the bound framebuffer. */

#ifndef MINI_GPU_DRAW_PASS_H
#define MINI_GPU_DRAW_PASS_H

#include "draw_list.h"

class DrawPass {
public:
	void Draw(const DrawList &list, TextureStore &textures, Dimension screen);
	void Release();

private:
	bool Prepare();

	uint32_t program = 0;
	uint32_t vertex_array = 0;
	uint32_t vertex_buffer = 0;
	uint32_t white = 0;
	int screen_uniform = -1;
	bool broken = false;
};

#endif /* MINI_GPU_DRAW_PASS_H */
