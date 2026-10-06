/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file draw_list.cpp Shapes and textured quads recorded for one frame as indexed vertices, batched by texture. */

#include "../../stdafx.h"
#include "draw_list.h"

#include <cmath>
#include <numbers>
#include <ranges>

#include "../../safeguards.h"

static constexpr int DOT_RADIUS = 1;
static constexpr int SMALL_CIRCLE_RADIUS = 4;
static constexpr int SMALL_CIRCLE_SIDES = 8;
static constexpr int LARGE_CIRCLE_SIDES = 20;
static constexpr int DIAMOND_SIDES = 4;
static constexpr int CORNER_SEGMENTS = 6;
static constexpr float FULL_TURN = 360.0f;
static constexpr float QUARTER_TURN = 90.0f;
static constexpr float RIGHTWARD = 0.0f;
static constexpr float DOWNWARD = 90.0f;
static constexpr float LEFTWARD = 180.0f;
static constexpr float UPWARD = 270.0f;

static float Radians(float degrees)
{
	return degrees * std::numbers::pi_v<float> / 180.0f;
}

static VertexColour Unpack(uint32_t argb)
{
	return {static_cast<uint8_t>(Red(argb)), static_cast<uint8_t>(Green(argb)), static_cast<uint8_t>(Blue(argb)), static_cast<uint8_t>(Alpha(argb))};
}

static DrawVertex CornerVertex(const TexturedCorner &corner)
{
	return {corner.at.x, corner.at.y, corner.u, corner.v, Unpack(corner.argb)};
}

void DrawList::Clear()
{
	this->vertices.clear();
	this->indices.clear();
	this->batches.clear();
}

/* The corners go in order around a convex outline, in either winding; the triangles fan out from the first. */
template <typename Outline>
void DrawList::Fan(const Outline &outline)
{
	uint32_t hub = this->batches.back().vertex_count;
	for (const DrawVertex &vertex : outline) this->Add(vertex);
	this->FanFrom(hub);
}

void DrawList::FillRect(int x0, int y0, int x1, int y1, uint32_t argb)
{
	if (x1 < x0 || y1 < y0) return;
	this->Box(x0, y0, x1 + 1, y1 + 1, Unpack(argb));
}

/* The corners never round past half the shorter side. */
void DrawList::FillRoundRect(int x0, int y0, int x1, int y1, int radius, uint32_t argb)
{
	if (x1 < x0 || y1 < y0) return;
	float left = x0;
	float top = y0;
	float right = x1 + 1;
	float bottom = y1 + 1;
	float r = std::min({static_cast<float>(radius), (right - left) / 2, (bottom - top) / 2});
	VertexColour colour = Unpack(argb);
	if (r <= 0.0f) {
		this->Box(left, top, right, bottom, colour);
		return;
	}

	this->Box(left + r, top, right - r, bottom, colour);
	this->Box(left, top + r, left + r, bottom - r, colour);
	this->Box(right - r, top + r, right, bottom - r, colour);
	this->Wedge(left + r, top + r, r, LEFTWARD, QUARTER_TURN, CORNER_SEGMENTS, colour);
	this->Wedge(right - r, top + r, r, UPWARD, QUARTER_TURN, CORNER_SEGMENTS, colour);
	this->Wedge(right - r, bottom - r, r, RIGHTWARD, QUARTER_TURN, CORNER_SEGMENTS, colour);
	this->Wedge(left + r, bottom - r, r, DOWNWARD, QUARTER_TURN, CORNER_SEGMENTS, colour);
}

void DrawList::FillGradient(int x0, int y0, int x1, int y1, uint32_t top_left, uint32_t top_right, uint32_t bottom_left, uint32_t bottom_right)
{
	if (x1 < x0 || y1 < y0) return;
	float right = x1 + 1;
	float bottom = y1 + 1;
	this->Use(this->solid.texture);
	this->Fan(std::array{this->Plain(x0, y0, Unpack(top_left)), this->Plain(right, y0, Unpack(top_right)), this->Plain(right, bottom, Unpack(bottom_right)), this->Plain(x0, bottom, Unpack(bottom_left))});
}

/* The stroke is centred on the segment and stops square at both ends. */
void DrawList::Line(int x0, int y0, int x1, int y1, int width, uint32_t argb)
{
	float dx = x1 - x0;
	float dy = y1 - y0;
	float length = std::hypot(dx, dy);
	if (length <= 0.0f || width <= 0) return;

	float scale = width / (2.0f * length);
	float nx = -scale * dy;
	float ny = scale * dx;
	this->FillQuad({ScreenPoint{x0 - nx, y0 - ny}, ScreenPoint{x0 + nx, y0 + ny}, ScreenPoint{x1 + nx, y1 + ny}, ScreenPoint{x1 - nx, y1 - ny}}, argb);
}

/* Single-pixel dots stay square; small discs need few sides. */
void DrawList::FillCircle(int cx, int cy, int r, uint32_t argb)
{
	if (r <= DOT_RADIUS) {
		this->FillRect(cx - r, cy - r, cx + r, cy + r, argb);
		return;
	}
	this->Wedge(cx, cy, r, RIGHTWARD, FULL_TURN, r <= SMALL_CIRCLE_RADIUS ? SMALL_CIRCLE_SIDES : LARGE_CIRCLE_SIDES, Unpack(argb));
}

void DrawList::FillDiamond(int cx, int cy, int r, uint32_t argb)
{
	this->Wedge(cx, cy, r, RIGHTWARD, FULL_TURN, DIAMOND_SIDES, Unpack(argb));
}

void DrawList::FillTriangle(int cx, int cy, int r, uint32_t argb)
{
	VertexColour colour = Unpack(argb);
	this->Use(this->solid.texture);
	this->Fan(std::array{this->Plain(cx, cy - r, colour), this->Plain(cx - r, cy + r, colour), this->Plain(cx + r, cy + r, colour)});
}

/* The corners go in order around the quad, in either winding. */
void DrawList::FillQuad(const std::array<ScreenPoint, 4> &corners, uint32_t argb)
{
	std::array<TexturedCorner, 4> textured;
	std::ranges::transform(corners, textured.begin(), [&](const ScreenPoint &at) { return TexturedCorner{at, this->solid.u, this->solid.v, argb}; });
	this->Polygon(this->solid.texture, textured);
}

/* The quad turns about its centre, so angle 0 lands exactly on the destination. */
void DrawList::Image(TextureId texture, const Rect &dest, const UvRect &uv, float angle_deg, uint32_t tint)
{
	if (texture == NO_TEXTURE || dest.Width() <= 0 || dest.Height() <= 0) return;

	float half_w = dest.Width() / 2.0f;
	float half_h = dest.Height() / 2.0f;
	float cx = dest.left + half_w;
	float cy = dest.top + half_h;
	float turn_cos = std::cos(Radians(angle_deg));
	float turn_sin = std::sin(Radians(angle_deg));
	auto corner = [&](float ox, float oy, float u, float v) {
		return TexturedCorner{{cx + ox * turn_cos - oy * turn_sin, cy + ox * turn_sin + oy * turn_cos}, u, v, tint};
	};

	this->Polygon(texture, std::array{corner(-half_w, -half_h, uv.left, uv.top), corner(half_w, -half_h, uv.right, uv.top), corner(half_w, half_h, uv.right, uv.bottom), corner(-half_w, half_h, uv.left, uv.bottom)});
}

void DrawList::Polygon(TextureId texture, std::span<const TexturedCorner> convex)
{
	if (convex.size() < TRIANGLE_CORNERS) return;
	this->Use(texture);
	this->Fan(convex | std::views::transform(CornerVertex));
}

DrawVertex DrawList::Plain(float x, float y, const VertexColour &colour) const
{
	return {x, y, this->solid.u, this->solid.v, colour};
}

/* A batch that never received a vertex is taken over rather than left empty. */
void DrawList::Use(TextureId texture)
{
	if (!this->batches.empty()) {
		DrawBatch &last = this->batches.back();
		if (last.texture == texture) return;
		if (last.vertex_count == 0) {
			last.texture = texture;
			return;
		}
	}
	this->batches.push_back({texture, static_cast<uint32_t>(this->vertices.size()), 0, static_cast<uint32_t>(this->indices.size()), 0});
}

/* The vertex's index within the current batch. */
uint32_t DrawList::Add(const DrawVertex &vertex)
{
	this->vertices.push_back(vertex);
	return this->batches.back().vertex_count++;
}

/* Joins every vertex added since the hub into triangles fanning out from it. */
void DrawList::FanFrom(uint32_t hub)
{
	DrawBatch &batch = this->batches.back();
	for (uint32_t rim = hub + 2; rim < batch.vertex_count; rim++) this->indices.insert(this->indices.end(), {hub, rim - 1, rim});
	batch.index_count = static_cast<uint32_t>(this->indices.size()) - batch.first_index;
}

void DrawList::Box(float left, float top, float right, float bottom, const VertexColour &colour)
{
	if (right <= left || bottom <= top) return;
	this->Use(this->solid.texture);
	this->Fan(std::array{this->Plain(left, top, colour), this->Plain(right, top, colour), this->Plain(right, bottom, colour), this->Plain(left, bottom, colour)});
}

/* Angles run clockwise on screen, from the rightward axis; the rim shares its corners between neighbouring slices. */
void DrawList::Wedge(float cx, float cy, float radius, float start_deg, float sweep_deg, int segments, const VertexColour &colour)
{
	this->Use(this->solid.texture);
	uint32_t hub = this->Add(this->Plain(cx, cy, colour));
	for (int i = 0; i <= segments; i++) {
		float angle = Radians(start_deg + sweep_deg * i / segments);
		this->Add(this->Plain(cx + radius * std::cos(angle), cy + radius * std::sin(angle), colour));
	}
	this->FanFrom(hub);
}
