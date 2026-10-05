/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file draw_list.h Shapes and textured quads recorded for one frame, batched by texture. */

#ifndef MINI_GPU_DRAW_LIST_H
#define MINI_GPU_DRAW_LIST_H

#include <array>
#include <span>
#include <vector>

#include "../../core/geometry_type.hpp"
#include "texture_store.h"

using VertexColour = std::array<uint8_t, 4>;

struct DrawVertex {
	float x;
	float y;
	float u;
	float v;
	VertexColour rgba;
};

struct DrawBatch {
	TextureId texture;
	uint32_t first;
	uint32_t count;
};

struct UvRect {
	float left;
	float top;
	float right;
	float bottom;
};

static constexpr UvRect FULL_UV = {0.0f, 0.0f, 1.0f, 1.0f};

/* Positions are pixels with inclusive right and bottom edges; colours are 0xAARRGGBB. */
class DrawList {
public:
	void Clear();

	void FillRect(int x0, int y0, int x1, int y1, uint32_t argb);
	void FillRoundRect(int x0, int y0, int x1, int y1, int radius, uint32_t argb);
	void FillGradient(int x0, int y0, int x1, int y1, uint32_t top_left, uint32_t top_right, uint32_t bottom_left, uint32_t bottom_right);
	void Line(int x0, int y0, int x1, int y1, int width, uint32_t argb);
	void FillCircle(int cx, int cy, int r, uint32_t argb);
	void FillDiamond(int cx, int cy, int r, uint32_t argb);
	void FillTriangle(int cx, int cy, int r, uint32_t argb);
	void Image(TextureId texture, const Rect &dest, const UvRect &uv, int angle_deg, uint32_t tint);

	std::span<const DrawVertex> Vertices() const { return this->vertices; }
	std::span<const DrawBatch> Batches() const { return this->batches; }

private:
	void Use(TextureId texture);
	void Triangle(const DrawVertex &a, const DrawVertex &b, const DrawVertex &c);
	void Quad(const DrawVertex &top_left, const DrawVertex &top_right, const DrawVertex &bottom_right, const DrawVertex &bottom_left);
	void Box(float left, float top, float right, float bottom, const VertexColour &colour);
	void Wedge(float cx, float cy, float radius, float start_deg, float sweep_deg, int segments, const VertexColour &colour);

	std::vector<DrawVertex> vertices;
	std::vector<DrawBatch> batches;
};

#endif /* MINI_GPU_DRAW_LIST_H */
