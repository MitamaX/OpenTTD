/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file draw_list.h Shapes and textured quads recorded for one frame as indexed vertices, batched by texture. */

#ifndef MINI_GPU_DRAW_LIST_H
#define MINI_GPU_DRAW_LIST_H

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <vector>

#include "../../core/geometry_type.hpp"
#include "texture_store.h"

inline constexpr uint CHANNEL_MAX = 0xFF;
inline constexpr uint ALPHA_SHIFT = 24;
inline constexpr uint RED_SHIFT = 16;
inline constexpr uint GREEN_SHIFT = 8;
inline constexpr uint BLUE_SHIFT = 0;
inline constexpr size_t TRIANGLE_CORNERS = 3;

constexpr uint Alpha(uint32_t argb)
{
	return (argb >> ALPHA_SHIFT) & CHANNEL_MAX;
}

constexpr uint Red(uint32_t argb)
{
	return (argb >> RED_SHIFT) & CHANNEL_MAX;
}

constexpr uint Green(uint32_t argb)
{
	return (argb >> GREEN_SHIFT) & CHANNEL_MAX;
}

constexpr uint Blue(uint32_t argb)
{
	return (argb >> BLUE_SHIFT) & CHANNEL_MAX;
}

constexpr uint32_t PackArgb(uint alpha, uint red, uint green, uint blue)
{
	return (alpha << ALPHA_SHIFT) | (red << RED_SHIFT) | (green << GREEN_SHIFT) | (blue << BLUE_SHIFT);
}

constexpr uint32_t WithAlpha(uint32_t argb, uint alpha)
{
	return PackArgb(alpha, Red(argb), Green(argb), Blue(argb));
}

/* The alpha that keeps this share of a full alpha, from none at 0 to all of it at 1. */
inline uint FadedAlpha(double share, uint alpha = CHANNEL_MAX)
{
	return static_cast<uint>(std::lround(alpha * std::clamp(share, 0.0, 1.0)));
}

using VertexColour = std::array<uint8_t, 4>;

struct DrawVertex {
	float x;
	float y;
	float u;
	float v;
	VertexColour rgba;
};

/* Indices count from the batch's first vertex, so each batch stands alone as a mesh. */
struct DrawBatch {
	TextureId texture;
	uint32_t first_vertex;
	uint32_t vertex_count;
	uint32_t first_index;
	uint32_t index_count;
};

struct UvRect {
	float left;
	float top;
	float right;
	float bottom;
};

static constexpr UvRect FULL_UV = {0.0f, 0.0f, 1.0f, 1.0f};

/* A continuous screen position: pixel n spans n to n + 1. */
struct ScreenPoint {
	float x;
	float y;
};

/* One corner of a textured shape: where it lands, the texel it samples and its tint. */
struct TexturedCorner {
	ScreenPoint at;
	float u;
	float v;
	uint32_t argb;
};

/* A plain white texel inside a texture sprites draw from, so untextured shapes join the sprites' batches. */
struct SolidTexel {
	TextureId texture = NO_TEXTURE;
	float u = 0.0f;
	float v = 0.0f;
};

/* Positions are pixels with inclusive right and bottom edges; colours are 0xAARRGGBB. */
class DrawList {
public:
	void Clear();
	void SetSolid(const SolidTexel &solid) { this->solid = solid; }

	void FillRect(int x0, int y0, int x1, int y1, uint32_t argb);
	void FillRoundRect(int x0, int y0, int x1, int y1, int radius, uint32_t argb);
	void FillGradient(int x0, int y0, int x1, int y1, uint32_t top_left, uint32_t top_right, uint32_t bottom_left, uint32_t bottom_right);
	void Line(int x0, int y0, int x1, int y1, int width, uint32_t argb);
	void FillCircle(int cx, int cy, int r, uint32_t argb);
	void FillDiamond(int cx, int cy, int r, uint32_t argb);
	void FillTriangle(int cx, int cy, int r, uint32_t argb);
	void FillQuad(const std::array<ScreenPoint, 4> &corners, uint32_t argb);
	void Image(TextureId texture, const Rect &dest, const UvRect &uv, uint32_t tint);
	void Polygon(TextureId texture, std::span<const TexturedCorner> convex);

	std::span<const DrawVertex> Vertices() const { return this->vertices; }
	std::span<const uint32_t> Indices() const { return this->indices; }
	std::span<const DrawBatch> Batches() const { return this->batches; }

private:
	DrawVertex Plain(float x, float y, const VertexColour &colour) const;
	void Use(TextureId texture);
	uint32_t Add(const DrawVertex &vertex);
	void FanFrom(uint32_t hub);
	template <typename Outline>
	void Fan(const Outline &outline);
	void Box(float left, float top, float right, float bottom, const VertexColour &colour);
	void Wedge(float cx, float cy, float radius, float start_deg, float sweep_deg, int segments, const VertexColour &colour);

	std::vector<DrawVertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<DrawBatch> batches;
	SolidTexel solid;
};

#endif /* MINI_GPU_DRAW_LIST_H */
