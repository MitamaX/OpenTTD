/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file draw_list.cpp Shapes and textured quads recorded for one frame, batched by texture. */

#include "../../stdafx.h"
#include "draw_list.h"

#include <cmath>
#include <numbers>

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
	return {static_cast<uint8_t>(argb >> 16), static_cast<uint8_t>(argb >> 8), static_cast<uint8_t>(argb), static_cast<uint8_t>(argb >> 24)};
}

static DrawVertex Plain(float x, float y, const VertexColour &colour)
{
	return {x, y, 0.0f, 0.0f, colour};
}

void DrawList::Clear()
{
	this->vertices.clear();
	this->batches.clear();
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
	this->Use(NO_TEXTURE);
	this->Quad(Plain(x0, y0, Unpack(top_left)), Plain(right, y0, Unpack(top_right)), Plain(right, bottom, Unpack(bottom_right)), Plain(x0, bottom, Unpack(bottom_left)));
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
	VertexColour colour = Unpack(argb);
	this->Use(NO_TEXTURE);
	this->Quad(Plain(x0 - nx, y0 - ny, colour), Plain(x0 + nx, y0 + ny, colour), Plain(x1 + nx, y1 + ny, colour), Plain(x1 - nx, y1 - ny, colour));
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
	this->Use(NO_TEXTURE);
	this->Triangle(Plain(cx, cy - r, colour), Plain(cx - r, cy + r, colour), Plain(cx + r, cy + r, colour));
}

/* The quad turns about its centre, so angle 0 lands exactly on the destination. */
void DrawList::Image(TextureId texture, const Rect &dest, const UvRect &uv, int angle_deg, uint32_t tint)
{
	if (texture == NO_TEXTURE || dest.Width() <= 0 || dest.Height() <= 0) return;

	float half_w = dest.Width() / 2.0f;
	float half_h = dest.Height() / 2.0f;
	float cx = dest.left + half_w;
	float cy = dest.top + half_h;
	float turn_cos = std::cos(Radians(angle_deg));
	float turn_sin = std::sin(Radians(angle_deg));
	VertexColour colour = Unpack(tint);
	auto corner = [&](float ox, float oy, float u, float v) {
		return DrawVertex{cx + ox * turn_cos - oy * turn_sin, cy + ox * turn_sin + oy * turn_cos, u, v, colour};
	};

	this->Use(texture);
	this->Quad(corner(-half_w, -half_h, uv.left, uv.top), corner(half_w, -half_h, uv.right, uv.top), corner(half_w, half_h, uv.right, uv.bottom), corner(-half_w, half_h, uv.left, uv.bottom));
}

/* A batch that never received a vertex is taken over rather than left empty. */
void DrawList::Use(TextureId texture)
{
	if (!this->batches.empty()) {
		DrawBatch &last = this->batches.back();
		if (last.texture == texture) return;
		if (last.count == 0) {
			last.texture = texture;
			return;
		}
	}
	this->batches.push_back({texture, static_cast<uint32_t>(this->vertices.size()), 0});
}

void DrawList::Triangle(const DrawVertex &a, const DrawVertex &b, const DrawVertex &c)
{
	this->vertices.insert(this->vertices.end(), {a, b, c});
	this->batches.back().count += 3;
}

void DrawList::Quad(const DrawVertex &top_left, const DrawVertex &top_right, const DrawVertex &bottom_right, const DrawVertex &bottom_left)
{
	this->Triangle(top_left, top_right, bottom_right);
	this->Triangle(top_left, bottom_right, bottom_left);
}

void DrawList::Box(float left, float top, float right, float bottom, const VertexColour &colour)
{
	if (right <= left || bottom <= top) return;
	this->Use(NO_TEXTURE);
	this->Quad(Plain(left, top, colour), Plain(right, top, colour), Plain(right, bottom, colour), Plain(left, bottom, colour));
}

/* Angles run clockwise on screen, from the rightward axis. */
void DrawList::Wedge(float cx, float cy, float radius, float start_deg, float sweep_deg, int segments, const VertexColour &colour)
{
	this->Use(NO_TEXTURE);
	DrawVertex centre = Plain(cx, cy, colour);
	DrawVertex previous = Plain(cx + radius * std::cos(Radians(start_deg)), cy + radius * std::sin(Radians(start_deg)), colour);
	for (int i = 1; i <= segments; i++) {
		float angle = Radians(start_deg + sweep_deg * i / segments);
		DrawVertex next = Plain(cx + radius * std::cos(angle), cy + radius * std::sin(angle), colour);
		this->Triangle(centre, previous, next);
		previous = next;
	}
}
