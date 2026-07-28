/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mini_ui_skin.cpp Flat geometric glyphs replacing widget control sprites while the mini UI is active. */

#include "stdafx.h"
#include "gfx_func.h"
#include "mini_ui.h"
#include "palette_func.h"
#include "table/sprites.h"
#include "zoom_func.h"

#include <cmath>

#include "safeguards.h"

static PixelColour GlyphInk(Colours colour)
{
	return GetColourGradient(colour, SHADE_DARKEST);
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
		case SPR_BLOT: DrawDiscGlyph(b, true, GetColourGradient(colour, SHADE_NORMAL), stroke); return true;
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

bool MiniUiDrawResizeGlyph(const Rect &r, Colours colour, bool at_left)
{
	if (!MiniUiActive()) return false;

	const PixelColour ink = GlyphInk(colour);
	const int stroke = std::max(1, ScaleGUITrad(1));
	const Rect b = r.Shrink(ScaleGUITrad(3));
	int step = std::max(2, std::min(b.Width(), b.Height()) / 3);
	for (int k = 1; k <= 2; k++) {
		int reach = k * step;
		if (at_left) {
			GfxDrawLine(b.left + reach, b.bottom, b.left, b.bottom - reach, ink, stroke);
		} else {
			GfxDrawLine(b.right - reach, b.bottom, b.right, b.bottom - reach, ink, stroke);
		}
	}
	return true;
}
