/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file gl_state.h The GL state a painter borrows from RmlUi, taken when it starts and handed back when it is done. */

#ifndef MINI_GPU_GL_STATE_H
#define MINI_GPU_GL_STATE_H

#include <array>

class GlStateScope {
public:
	static constexpr int TEXTURE_UNITS = 8;

	GlStateScope();
	~GlStateScope();
	GlStateScope(const GlStateScope &) = delete;
	GlStateScope &operator=(const GlStateScope &) = delete;

	void Restore() const;

private:
	struct Switches {
		bool scissor;
		bool stencil;
		bool depth;
		bool blend;
		bool cull;
	};

	struct Blending {
		int source_rgb;
		int destination_rgb;
		int source_alpha;
		int destination_alpha;
		int equation_rgb;
		int equation_alpha;
	};

	Switches switches{};
	Blending blending{};
	int draw_framebuffer = 0;
	int read_framebuffer = 0;
	std::array<int, 4> viewport{};
	std::array<int, 4> scissor_box{};
	std::array<unsigned char, 4> colour_mask{};
	std::array<float, 4> clear_colour{};
	double clear_depth = 1.0;
	unsigned char depth_mask = 0;
	int depth_func = 0;
	int program = 0;
	int vertex_array = 0;
	int array_buffer = 0;
	int uniform_buffer = 0;
	int active_texture = 0;
	std::array<int, TEXTURE_UNITS> textures{};
};

#endif /* MINI_GPU_GL_STATE_H */
