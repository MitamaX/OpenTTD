/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mini_atlas.cpp Builds the mini UI sprite atlas at run time and draws tinted sub-rectangle quads from it. */

#include "stdafx.h"

#include <cmath>
#include <vector>

#include "mini_atlas.h"
#include "video/raylib_wrap.h"

#include "safeguards.h"

/* Cells keep a wide transparent gutter so mipmap levels never blend
 * neighbouring sprites into each other. */
static const int ATLAS_CELL = 64;
static const int ATLAS_GUTTER = 8;
static const int ATLAS_COLS = 4;
static const int ATLAS_ROWS = ((int)MiniSprite::End + ATLAS_COLS - 1) / ATLAS_COLS;

static int _atlas_tex = 0;

/* Shapes live in a unit square; x grows right, y grows down. */
static bool SpriteHit(MiniSprite sprite, double x, double y)
{
	double dx = x - 0.5;
	double dy = y - 0.5;
	switch (sprite) {
		case MiniSprite::Disc: return dx * dx + dy * dy <= 0.25;
		case MiniSprite::Diamond: return std::abs(dx) + std::abs(dy) <= 0.5;
		case MiniSprite::Triangle: return std::abs(dx) <= y * 0.5;
		default: return false;
	}
}

void MiniAtlasEnsure()
{
	if (_atlas_tex != 0) return;

	int w = ATLAS_COLS * ATLAS_CELL;
	int h = ATLAS_ROWS * ATLAS_CELL;
	int content = ATLAS_CELL - 2 * ATLAS_GUTTER;
	std::vector<uint32_t> px((size_t)w * h, 0x00FFFFFFU);

	for (int i = 0; i < (int)MiniSprite::End; i++) {
		int ox = (i % ATLAS_COLS) * ATLAS_CELL + ATLAS_GUTTER;
		int oy = (i / ATLAS_COLS) * ATLAS_CELL + ATLAS_GUTTER;
		for (int y = 0; y < content; y++) {
			for (int x = 0; x < content; x++) {
				int hits = 0;
				for (int sy = 0; sy < 4; sy++) {
					for (int sx = 0; sx < 4; sx++) {
						double u = (x + (sx + 0.5) / 4.0) / content;
						double v = (y + (sy + 0.5) / 4.0) / content;
						if (SpriteHit((MiniSprite)i, u, v)) hits++;
					}
				}
				uint32_t a = hits * 255 / 16;
				px[(size_t)(oy + y) * w + ox + x] = (a << 24) | 0x00FFFFFFU;
			}
		}
	}

	_atlas_tex = RlwCreateAtlasTexture(px.data(), w, h);
}

/* The driver unloads every texture on shutdown; forgetting the id here makes
 * the next frame rebuild the atlas instead of drawing with a dead handle. */
void MiniAtlasReset()
{
	_atlas_tex = 0;
}

static bool AtlasSprite(MiniSprite sprite, int x0, int y0, int x1, int y1, int angle_deg, uint32_t argb)
{
	if (_atlas_tex == 0 || sprite >= MiniSprite::End || x1 < x0 || y1 < y0) return false;
	int content = ATLAS_CELL - 2 * ATLAS_GUTTER;
	int sx = ((int)sprite % ATLAS_COLS) * ATLAS_CELL + ATLAS_GUTTER;
	int sy = ((int)sprite / ATLAS_COLS) * ATLAS_CELL + ATLAS_GUTTER;
	RlwCmdSprite(_atlas_tex, sx, sy, content, content, x0, y0, x1, y1, angle_deg, argb);
	return true;
}

bool MiniAtlasQuad(MiniSprite sprite, int x0, int y0, int x1, int y1, uint32_t argb)
{
	return AtlasSprite(sprite, x0, y0, x1, y1, 0, argb);
}

bool MiniAtlasQuadRot(MiniSprite sprite, int cx, int cy, int r, int angle_deg, uint32_t argb)
{
	return AtlasSprite(sprite, cx - r, cy - r, cx + r, cy + r, angle_deg, argb);
}
