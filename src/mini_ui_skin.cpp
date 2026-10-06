/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mini_ui_skin.cpp Flat geometric glyphs replacing widget control sprites while the mini UI is active. */

#include "stdafx.h"
#include "gfx_func.h"
#include "mini_ui_skin.h"
#include "palette_func.h"
#include "table/sprites.h"
#include "zoom_func.h"

#include <cmath>

#include "safeguards.h"

static const uint32_t _skin_tones[] = {
	MINI_CH_PANEL, MINI_CH_EDGE, MINI_CH_SUNKEN, MINI_CH_TILE,
	MINI_CH_ACTIVE, MINI_CH_TEXT, MINI_CH_DIM, MINI_CH_ACCENT,
};

/* Chrome tones are authored as RGB but widgets paint through palette indices.
 * The palette's neutral ramp steps by sixteen, so nearest-colour matching
 * collapses the edge onto the panel and the outline disappears; the greys are
 * pinned to ramp steps instead and only the tinted tones are matched. */
PixelColour MiniUiSkinTone(uint32_t argb)
{
	switch (argb) {
		case MINI_CH_EDGE: return GREY_SCALE(1);
		case MINI_CH_SUNKEN: return GREY_SCALE(1);
		case MINI_CH_PANEL: return GREY_SCALE(2);
		case MINI_CH_TILE: return GREY_SCALE(3);
		case MINI_CH_DIM: return GREY_SCALE(7);
		default: break;
	}

	static uint8_t resolved[lengthof(_skin_tones)];
	static bool ready = false;
	if (!ready) {
		for (size_t i = 0; i < lengthof(_skin_tones); i++) {
			uint32_t t = _skin_tones[i];
			resolved[i] = GetNearestColourIndex((uint8_t)(t >> 16), (uint8_t)(t >> 8), (uint8_t)t);
		}
		ready = true;
	}
	for (size_t i = 0; i < lengthof(_skin_tones); i++) {
		if (_skin_tones[i] == argb) return PixelColour{resolved[i]};
	}
	return PixelColour{GetNearestColourIndex((uint8_t)(argb >> 16), (uint8_t)(argb >> 8), (uint8_t)argb)};
}

/* Buttons take the tile tone so their outline reads; the window face behind
 * them is painted separately with the panel tone. */
PixelColour MiniUiSkinFrameFill(bool lowered, bool darkened)
{
	if (!lowered) return MiniUiSkinTone(MINI_CH_TILE);
	return MiniUiSkinTone(darkened ? MINI_CH_SUNKEN : MINI_CH_ACTIVE);
}

/* A sunken field is already the darkest tone the ramp has, and the official
 * code fills some of them with plain black on top, so a dark outline would
 * make the whole field one flat block. Those get a light rim instead. */
PixelColour MiniUiSkinFrameBorder(bool lowered, bool darkened)
{
	return (lowered && darkened) ? MiniUiSkinTone(MINI_CH_TILE) : MiniUiSkinTone(MINI_CH_EDGE);
}

/* String colours were picked against the light official palette; on the dark
 * skin the dim ones are lifted to a readable level with their hue kept. */
PixelColour MiniUiSkinTextColour(PixelColour colour)
{
	static const uint LIFT_BELOW = 110;
	static const uint LIFT_TO = 168;

	if (!MiniUiActive()) return colour;

	static uint8_t lifted[256];
	static bool known[256];
	if (!known[colour.p]) {
		const Colour &c = _cur_palette.palette[colour.p];
		uint lum = (c.r * 30 + c.g * 59 + c.b * 11) / 100;
		if (lum >= LIFT_BELOW) {
			lifted[colour.p] = colour.p;
		} else if (lum < 8) {
			lifted[colour.p] = MiniUiSkinTone(MINI_CH_TEXT).p;
		} else {
			lifted[colour.p] = GetNearestColourIndex(
					(uint8_t)std::min<uint>(255, c.r * LIFT_TO / lum),
					(uint8_t)std::min<uint>(255, c.g * LIFT_TO / lum),
					(uint8_t)std::min<uint>(255, c.b * LIFT_TO / lum));
		}
		known[colour.p] = true;
	}
	return PixelColour{lifted[colour.p]};
}

static PixelColour GlyphInk(Colours)
{
	return MiniUiSkinTone(MINI_CH_TEXT);
}

static Rect GlyphBox(const Rect &r, int pad)
{
	int space = std::min(r.Width(), r.Height());
	pad = std::min(pad, space / 4);
	int side = std::max(3, space - 2 * pad);
	int x = CentreBounds(r.left, r.right, side);
	int y = CentreBounds(r.top, r.bottom, side);
	return {x, y, x + side - 1, y + side - 1};
}

/** Filled triangle pointing along (dir_x, dir_y); exactly one of the two is non-zero. */
static void DrawTriangleGlyph(const Rect &b, int dir_x, int dir_y, PixelColour ink)
{
	if (dir_y != 0) {
		int h = b.Height();
		int cx = (b.left + b.right) / 2;
		for (int i = 0; i < h; i++) {
			int y = dir_y < 0 ? b.top + i : b.bottom - i;
			int half = h > 1 ? i * (b.Width() / 2) / (h - 1) : 0;
			GfxFillRect(cx - half, y, cx + half, y, ink);
		}
	} else {
		int w = b.Width();
		int cy = (b.top + b.bottom) / 2;
		for (int i = 0; i < w; i++) {
			int x = dir_x < 0 ? b.left + i : b.right - i;
			int half = w > 1 ? i * (b.Height() / 2) / (w - 1) : 0;
			GfxFillRect(x, cy - half, x, cy + half, ink);
		}
	}
}

static void DrawChevronGlyph(const Rect &b, bool up, PixelColour ink, int stroke)
{
	int cx = (b.left + b.right) / 2;
	int q = std::max(1, b.Height() / 4);
	int cy = (b.top + b.bottom) / 2;
	int tip_y = up ? cy - q : cy + q;
	int end_y = up ? cy + q : cy - q;
	GfxDrawLine(b.left, end_y, cx, tip_y, ink, stroke);
	GfxDrawLine(cx, tip_y, b.right, end_y, ink, stroke);
}

static void DrawDiscGlyph(const Rect &b, bool filled, PixelColour ink, int stroke)
{
	int rad = std::min(b.Width(), b.Height()) / 2;
	int cx = (b.left + b.right) / 2;
	int cy = (b.top + b.bottom) / 2;
	int hole = filled ? 0 : std::max(0, rad - stroke);
	for (int dy = -rad; dy <= rad; dy++) {
		int outer = (int)std::sqrt((double)(rad * rad - dy * dy));
		int inner = std::abs(dy) < hole ? (int)std::sqrt((double)(hole * hole - dy * dy)) : -1;
		if (inner < 0) {
			GfxFillRect(cx - outer, cy + dy, cx + outer, cy + dy, ink);
		} else {
			GfxFillRect(cx - outer, cy + dy, cx - inner - 1, cy + dy, ink);
			GfxFillRect(cx + inner + 1, cy + dy, cx + outer, cy + dy, ink);
		}
	}
}

static void DrawSquareGlyph(const Rect &b, PixelColour ink, int stroke)
{
	GfxFillRect(b.left, b.top, b.right, b.top + stroke - 1, ink);
	GfxFillRect(b.left, b.bottom - stroke + 1, b.right, b.bottom, ink);
	GfxFillRect(b.left, b.top, b.left + stroke - 1, b.bottom, ink);
	GfxFillRect(b.right - stroke + 1, b.top, b.right, b.bottom, ink);
}

static void DrawDiamondGlyph(const Rect &b, PixelColour ink)
{
	int cy = (b.top + b.bottom) / 2;
	DrawTriangleGlyph({b.left, b.top, b.right, cy}, 0, -1, ink);
	DrawTriangleGlyph({b.left, cy, b.right, b.bottom}, 0, 1, ink);
}

static void DrawPenGlyph(const Rect &b, PixelColour ink, int stroke)
{
	int q = std::max(2, b.Width() / 3);
	GfxDrawLine(b.left + q, b.bottom - q, b.right, b.top, ink, stroke + 1);
	DrawTriangleGlyph({b.left, b.bottom - q, b.left + q, b.bottom}, -1, 0, ink);
}

static void DrawCrosshairGlyph(const Rect &b, PixelColour ink, int stroke)
{
	int cx = (b.left + b.right) / 2;
	int cy = (b.top + b.bottom) / 2;
	GfxDrawLine(cx, b.top, cx, b.bottom, ink, stroke);
	GfxDrawLine(b.left, cy, b.right, cy, ink, stroke);
	DrawDiscGlyph(b.Shrink(b.Width() / 4), false, ink, stroke);
}

bool MiniUiDrawControlGlyph(const Rect &r, Colours colour, SpriteID sprite)
{
	if (!MiniUiActive()) return false;

	const PixelColour ink = GlyphInk(colour);
	const int stroke = std::max(1, ScaleGUITrad(1));
	const Rect b = GlyphBox(r, ScaleGUITrad(3));

	switch (sprite) {
		case SPR_ARROW_UP:    DrawTriangleGlyph(b, 0, -1, ink); return true;
		case SPR_ARROW_DOWN:  DrawTriangleGlyph(b, 0, 1, ink); return true;
		case SPR_ARROW_LEFT:  DrawTriangleGlyph(b, -1, 0, ink); return true;
		case SPR_ARROW_RIGHT: DrawTriangleGlyph(b, 1, 0, ink); return true;
		case SPR_WINDOW_UNSHADE: DrawChevronGlyph(b, true, ink, stroke); return true;
		case SPR_WINDOW_SHADE:   DrawChevronGlyph(b, false, ink, stroke); return true;
		case SPR_PIN_UP:   DrawDiscGlyph(b, true, ink, stroke); return true;
		case SPR_PIN_DOWN: DrawDiscGlyph(b, false, ink, stroke); return true;
		case SPR_WINDOW_DEFSIZE: DrawSquareGlyph(b, ink, stroke); return true;
		case SPR_WINDOW_DEBUG:   DrawDiamondGlyph(b, ink); return true;
		case SPR_BOX_EMPTY: DrawSquareGlyph(b, ink, stroke); return true;
		case SPR_BOX_CHECKED: {
			DrawSquareGlyph(b, ink, stroke);
			Rect inner = b.Shrink(2 * stroke + 1);
			if (inner.Width() > 0 && inner.Height() > 0) GfxFillRect(inner, ink);
			return true;
		}
		case SPR_BLOT: DrawDiscGlyph(b, true, MiniUiSkinTone(MINI_CH_ACCENT), stroke); return true;
		case SPR_RENAME: DrawPenGlyph(b, ink, stroke); return true;
		case SPR_GOTO_LOCATION: DrawCrosshairGlyph(b, ink, stroke); return true;
		default: return false;
	}
}

bool MiniUiDrawCoverageGlyph(const Rect &r, Colours colour)
{
	if (!MiniUiActive()) return false;

	const PixelColour ink = GlyphInk(colour);
	const int stroke = std::max(1, ScaleGUITrad(1));
	const Rect b = GlyphBox(r, ScaleGUITrad(3));
	DrawDiscGlyph(b, false, ink, stroke);
	DrawDiscGlyph(b.Shrink(std::max(2, b.Width() * 3 / 8)), true, ink, stroke);
	return true;
}

bool MiniUiDrawCloseGlyph(const Rect &r, Colours colour)
{
	if (!MiniUiActive()) return false;

	const PixelColour ink = GlyphInk(colour);
	const int stroke = std::max(1, ScaleGUITrad(1));
	const Rect b = GlyphBox(r, ScaleGUITrad(4));
	GfxDrawLine(b.left, b.top, b.right, b.bottom, ink, stroke);
	GfxDrawLine(b.left, b.bottom, b.right, b.top, ink, stroke);
	return true;
}
