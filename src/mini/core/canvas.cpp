/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file canvas.cpp Shapes and text the mini UI records into the map draw list. */

#include "../../stdafx.h"
#include "canvas.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "../../core/backup_type.hpp"
#include "../../gfx_func.h"
#include "../gpu/texture_store.h"
#include "tuning.h"

#include "../../safeguards.h"

static constexpr uint64_t TEXT_PRUNE_INTERVAL_MASK = 0xFF;
static constexpr uint64_t TEXT_IDLE_FRAMES = 600;
static constexpr int MIN_ATLAS_RADIUS = 2;
static constexpr uint32_t GLYPH_WHITE = 0xFFFFFFFFU;

Canvas _canvas;
DrawList _map_draw;

void Canvas::BeginFrame()
{
	this->frame++;
	this->PruneText();
}

uint32_t Canvas::Blended(uint32_t c, uint alpha) const
{
	return WithAlpha(c, std::min(alpha, OPAQUE_ALPHA));
}

void Canvas::FillRect(int x0, int y0, int x1, int y1, uint32_t c)
{
	if (x1 < x0 || y1 < y0) return;
	_map_draw.FillRect(x0, y0, x1, y1, c);
}

void Canvas::BlendRect(int x0, int y0, int x1, int y1, uint32_t c, uint alpha)
{
	if (x1 < x0 || y1 < y0) return;
	_map_draw.FillRect(x0, y0, x1, y1, this->Blended(c, alpha));
}

void Canvas::Frame(const Rect &r, int width, uint32_t c, uint alpha)
{
	this->BlendRect(r.left, r.top, r.right, r.top + width - 1, c, alpha);
	this->BlendRect(r.left, r.bottom - width + 1, r.right, r.bottom, c, alpha);
	this->BlendRect(r.left, r.top, r.left + width - 1, r.bottom, c, alpha);
	this->BlendRect(r.right - width + 1, r.top, r.right, r.bottom, c, alpha);
}

ScreenPoint ScreenPointOf(const WorldPoint &point)
{
	ExactPoint at = _camera.ExactScreenOf(point);
	return {static_cast<float>(at.x), static_cast<float>(at.y)};
}

std::array<ScreenPoint, 4> ScreenQuadOf(const std::array<WorldPoint, 4> &corners)
{
	std::array<ScreenPoint, 4> screen;
	std::ranges::transform(corners, screen.begin(), ScreenPointOf);
	return screen;
}

bool OnScreen(std::span<const ScreenPoint> outline)
{
	auto [left, right] = std::ranges::minmax(outline, {}, &ScreenPoint::x);
	auto [top, bottom] = std::ranges::minmax(outline, {}, &ScreenPoint::y);
	return right.x > 0.0f && bottom.y > 0.0f && left.x < _camera.Width() && top.y < _camera.Height();
}

/* Twice the outline's signed area, positive when it runs clockwise on screen. */
float Winding(std::span<const ScreenPoint> outline)
{
	float sum = 0.0f;
	for (size_t i = 0; i < outline.size(); i++) {
		const ScreenPoint &from = outline[i];
		const ScreenPoint &to = outline[(i + 1) % outline.size()];
		sum += from.x * to.y - to.x * from.y;
	}
	return sum;
}

static ScreenPoint InwardNormal(const ScreenPoint &from, const ScreenPoint &to, float winding)
{
	float dx = to.x - from.x;
	float dy = to.y - from.y;
	float length = std::copysign(std::hypot(dx, dy), winding);
	if (length == 0.0f) return {0.0f, 0.0f};
	return {-dy / length, dx / length};
}

/* Each corner moves in along the mitre of its two sides, so the inner outline stays the width away from every side. */
static std::vector<ScreenPoint> Inset(std::span<const ScreenPoint> outline, float width)
{
	float winding = Winding(outline);
	size_t count = outline.size();
	std::vector<ScreenPoint> inner(count);
	for (size_t i = 0; i < count; i++) {
		const ScreenPoint &corner = outline[i];
		ScreenPoint before = InwardNormal(outline[(i + count - 1) % count], corner, winding);
		ScreenPoint after = InwardNormal(corner, outline[(i + 1) % count], winding);
		float reach = width / (1.0f + before.x * after.x + before.y * after.y);
		inner[i] = {corner.x + (before.x + after.x) * reach, corner.y + (before.y + after.y) * reach};
	}
	return inner;
}

void Canvas::FillWorldQuad(const std::array<WorldPoint, 4> &corners, uint32_t c, uint alpha)
{
	_map_draw.FillQuad(ScreenQuadOf(corners), this->Blended(c, alpha));
}

/* Each side is a band reaching in from the outline, so the bands meet at the corners without overlapping. */
void Canvas::FrameWorldRing(std::span<const WorldPoint> ring, int width, uint32_t c, uint alpha)
{
	std::vector<ScreenPoint> outer(ring.size());
	std::ranges::transform(ring, outer.begin(), ScreenPointOf);
	std::vector<ScreenPoint> inner = Inset(outer, static_cast<float>(width));
	uint32_t argb = this->Blended(c, alpha);
	for (size_t i = 0; i < outer.size(); i++) {
		size_t next = (i + 1) % outer.size();
		std::array<ScreenPoint, 4> band = {outer[i], outer[next], inner[next], inner[i]};
		if (OnScreen(band)) _map_draw.FillQuad(band, argb);
	}
}

void Canvas::ThickLine(int x0, int y0, int x1, int y1, int width, uint32_t c)
{
	_map_draw.Line(x0, y0, x1, y1, std::max(width, 1), c);
}

/* Shape fills prefer an atlas quad so the silhouettes are already on the
 * sprite pipeline; sizes too small to sample cleanly and frames without an
 * atlas fall back to the geometric primitives. */
void Canvas::FillCircle(int cx, int cy, int r, uint32_t c)
{
	r = std::max(r, 1);
	if (r >= MIN_ATLAS_RADIUS && MiniAtlasQuad(MiniSprite::Disc, cx - r, cy - r, cx + r, cy + r, c)) return;
	_map_draw.FillCircle(cx, cy, r, c);
}

void Canvas::FillDiamond(int cx, int cy, int r, uint32_t c)
{
	r = std::max(r, 1);
	if (r >= MIN_ATLAS_RADIUS && MiniAtlasQuad(MiniSprite::Diamond, cx - r, cy - r, cx + r, cy + r, c)) return;
	_map_draw.FillDiamond(cx, cy, r, c);
}

void Canvas::FillTriangle(int cx, int cy, int r, uint32_t c)
{
	r = std::max(r, 1);
	if (r >= MIN_ATLAS_RADIUS && MiniAtlasQuad(MiniSprite::Triangle, cx - r, cy - r, cx + r, cy + r, c)) return;
	_map_draw.FillTriangle(cx, cy, r, c);
}

/* Strings are laid out by the native font code into a small offscreen buffer
 * once, cached as a white-on-transparent texture and drawn as a tinted quad,
 * so any TrueType fallback font covers non-Latin names. */
const CanvasText *Canvas::Text(std::string_view text)
{
	if (text.empty()) return nullptr;

	auto it = this->texts.find(std::string(text));
	if (it == this->texts.end()) {
		std::optional<CanvasText> rendered = this->Render(text);
		if (!rendered.has_value()) return nullptr;
		it = this->texts.emplace(std::string(text), *rendered).first;
	}
	it->second.last_use = this->frame;
	return &it->second;
}

void Canvas::DrawText(const CanvasText &text, int x, int y, uint32_t tint)
{
	Rect area = {x - text.pad, y - text.pad, x + text.w + text.pad - 1, y + text.h + text.pad - 1};
	_map_draw.Image(text.tex, area, FULL_UV, tint);
}

void Canvas::DrawText(std::string_view text, int x, int y, uint32_t tint)
{
	if (const CanvasText *rendered = this->Text(text); rendered != nullptr) this->DrawText(*rendered, x, y, tint);
}

std::optional<CanvasText> Canvas::Render(std::string_view text) const
{
	Dimension dim = GetStringBoundingBox(text);
	int w = static_cast<int>(dim.width);
	int h = static_cast<int>(dim.height);
	if (w <= 0 || h <= 0) return std::nullopt;

	/* TrueType glyph bitmaps can overhang the layout box; the padded
	 * canvas keeps those pixels instead of clipping them away. */
	int pad = GetCharacterHeight(FS_NORMAL) / 4 + 1;
	int bw = w + 2 * pad;
	int bh = h + 2 * pad;
	std::vector<uint32_t> buf(static_cast<size_t>(bw) * bh, 0xFF000000U);
	DrawPixelInfo dpi;
	dpi.dst_ptr = buf.data();
	dpi.left = 0;
	dpi.top = 0;
	dpi.width = bw;
	dpi.height = bh;
	dpi.pitch = bw;
	dpi.zoom = ZoomLevel::Min;
	{
		AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);
		AutoRestoreBackup anim_backup(_screen_disable_anim, true);
		DrawString(pad, pad + w - 1, pad, text, TC_WHITE, SA_LEFT | SA_FORCE);
	}
	for (uint32_t &px : buf) px = WithAlpha(GLYPH_WHITE, std::max({Red(px), Green(px), Blue(px)}));
	return CanvasText{_textures.Add(buf, Dimension(bw, bh)), w, h, pad, 0};
}

void Canvas::PruneText()
{
	if ((this->frame & TEXT_PRUNE_INTERVAL_MASK) != 0) return;
	for (auto it = this->texts.begin(); it != this->texts.end();) {
		if (this->frame - it->second.last_use > TEXT_IDLE_FRAMES) {
			_textures.Remove(it->second.tex);
			it = this->texts.erase(it);
		} else {
			++it;
		}
	}
}
