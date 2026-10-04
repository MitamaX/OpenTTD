/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file canvas.cpp Shapes and text the mini UI records into the raylib command buffer. */

#include "../../stdafx.h"
#include "canvas.h"

#include "../../core/backup_type.hpp"
#include "../../core/math_func.hpp"
#include "../../gfx_func.h"
#include "../../video/raylib_wrap.h"
#include "tuning.h"

#include "../../safeguards.h"

static constexpr uint64_t TEXT_PRUNE_INTERVAL_MASK = 0xFF;
static constexpr uint64_t TEXT_IDLE_FRAMES = 600;
static constexpr int MIN_ATLAS_RADIUS = 2;

Canvas _canvas;

void Canvas::BeginFrame()
{
	this->frame++;
	this->PruneText();
}

/* Overlay mode draws the base map as darkened greyscale: luminance is kept
 * so terrain still reads, while repainted layer content gets full colour.
 * filter_alpha sets how far the background sinks. */
uint32_t Canvas::Greyed(uint32_t c) const
{
	uint lum = (77 * ((c >> 16) & 0xFFU) + 151 * ((c >> 8) & 0xFFU) + 28 * (c & 0xFFU)) >> 8;
	uint g = 12 + lum * (255 - static_cast<uint>(_tuning.filter_alpha)) / 255;
	return (c & 0xFF000000U) | (g << 16) | (g << 8) | g;
}

uint32_t Canvas::Tone(uint32_t c) const
{
	return this->grey ? this->Greyed(c) : c;
}

void Canvas::FillRect(int x0, int y0, int x1, int y1, uint32_t c)
{
	if (x1 < x0 || y1 < y0) return;
	RlwCmdRect(x0, y0, x1, y1, this->Tone(c));
}

void Canvas::BlendRect(int x0, int y0, int x1, int y1, uint32_t c, uint alpha)
{
	if (x1 < x0 || y1 < y0) return;
	RlwCmdRect(x0, y0, x1, y1, (this->Tone(c) & 0x00FFFFFFU) | (static_cast<uint32_t>(Clamp<uint>(alpha, 0, 255)) << 24));
}

void Canvas::Frame(const Rect &r, int width, uint32_t c, uint alpha)
{
	this->BlendRect(r.left, r.top, r.right, r.top + width - 1, c, alpha);
	this->BlendRect(r.left, r.bottom - width + 1, r.right, r.bottom, c, alpha);
	this->BlendRect(r.left, r.top, r.left + width - 1, r.bottom, c, alpha);
	this->BlendRect(r.right - width + 1, r.top, r.right, r.bottom, c, alpha);
}

void Canvas::ThickLine(int x0, int y0, int x1, int y1, int width, uint32_t c)
{
	RlwCmdLine(x0, y0, x1, y1, std::max(width, 1), this->Tone(c));
}

/* Shape fills prefer an atlas quad so the silhouettes are already on the
 * sprite pipeline; sizes too small to sample cleanly and frames without an
 * atlas fall back to the geometric primitives. */
void Canvas::FillCircle(int cx, int cy, int r, uint32_t c)
{
	r = std::max(r, 1);
	uint32_t col = this->Tone(c);
	if (r >= MIN_ATLAS_RADIUS && MiniAtlasQuad(MiniSprite::Disc, cx - r, cy - r, cx + r, cy + r, col)) return;
	RlwCmdCircle(cx, cy, r, col);
}

void Canvas::FillDiamond(int cx, int cy, int r, uint32_t c)
{
	r = std::max(r, 1);
	uint32_t col = this->Tone(c);
	if (r >= MIN_ATLAS_RADIUS && MiniAtlasQuad(MiniSprite::Diamond, cx - r, cy - r, cx + r, cy + r, col)) return;
	RlwCmdDiamond(cx, cy, r, col);
}

void Canvas::FillTriangle(int cx, int cy, int r, uint32_t c)
{
	r = std::max(r, 1);
	uint32_t col = this->Tone(c);
	if (r >= MIN_ATLAS_RADIUS && MiniAtlasQuad(MiniSprite::Triangle, cx - r, cy - r, cx + r, cy + r, col)) return;
	RlwCmdTriangle(cx, cy, r, col);
}

/* Rotated silhouette: ships and aircraft point along their heading. Without
 * an atlas the shape falls back unrotated. */
void Canvas::FillShapeRot(MiniSprite s, int cx, int cy, int r, int angle, uint32_t c)
{
	r = std::max(r, 1);
	uint32_t col = this->Tone(c);
	if (r >= MIN_ATLAS_RADIUS && MiniAtlasQuadRot(s, cx, cy, r, angle, col)) return;
	switch (s) {
		case MiniSprite::Triangle:
		case MiniSprite::Aircraft:
			RlwCmdTriangle(cx, cy, r, col);
			break;
		case MiniSprite::Diamond:
		case MiniSprite::Ship:
			RlwCmdDiamond(cx, cy, r, col);
			break;
		default:
			RlwCmdCircle(cx, cy, r, col);
			break;
	}
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
	RlwCmdTexQuad(text.tex, x - text.pad, y - text.pad, tint);
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
	for (uint32_t &px : buf) {
		uint32_t a = std::max({(px >> 16) & 0xFF, (px >> 8) & 0xFF, px & 0xFF});
		px = (a << 24) | 0x00FFFFFFU;
	}
	return CanvasText{RlwCreateTexture(buf.data(), bw, bh), w, h, pad, 0};
}

void Canvas::PruneText()
{
	if ((this->frame & TEXT_PRUNE_INTERVAL_MASK) != 0) return;
	for (auto it = this->texts.begin(); it != this->texts.end();) {
		if (this->frame - it->second.last_use > TEXT_IDLE_FRAMES) {
			RlwFreeTexture(it->second.tex);
			it = this->texts.erase(it);
		} else {
			++it;
		}
	}
}
