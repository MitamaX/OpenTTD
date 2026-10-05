/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mini_atlas.cpp Builds the mini UI sprite atlas at run time and draws tinted sub-rectangle quads from it. */

#include "stdafx.h"

#include <cmath>
#include <utility>
#include <vector>

#include "fileio_func.h"
#include "mini_atlas.h"
#include "mini/core/canvas.h"
#include "mini/gpu/art_image.h"

#include "safeguards.h"

/* Cells keep a wide transparent gutter so mipmap levels never blend
 * neighbouring sprites into each other. */
static const int ATLAS_CELL = 64;
static const int ATLAS_GUTTER = 8;
static const int ATLAS_COLS = 4;
static const int ATLAS_ROWS = ((int)MiniSprite::End + ATLAS_COLS - 1) / ATLAS_COLS;
static const int ATLAS_WIDTH = ATLAS_COLS * ATLAS_CELL;
static const int ATLAS_HEIGHT = ATLAS_ROWS * ATLAS_CELL;
static const int ATLAS_CONTENT = ATLAS_CELL - 2 * ATLAS_GUTTER;

static TextureId _atlas_tex = NO_TEXTURE;
static bool _has_art[(size_t)MiniSprite::End];
static TextureId _tile_tex[(size_t)MiniSprite::End];

/* Ground kinds also get a standalone repeat-wrapped texture, so merged runs
 * of equal tiles can draw as one quad without per-tile seams. */
static bool IsGroundSlot(MiniSprite s)
{
	return s >= MiniSprite::Grass && s <= MiniSprite::Water;
}

/* Art file base names inside mini_art, one per MiniSprite slot. */
static const char *_slot_names[] = {
	"disc", "diamond", "triangle", "tree", "road_vehicle", "ship", "aircraft",
	"grass", "field", "rock", "snow", "desert", "water",
	"house", "industry", "station", "object", "depot", "tunnel",
};
static_assert(lengthof(_slot_names) == (size_t)MiniSprite::End);

/* Shapes live in a unit square; x grows right, y grows down. */
static bool SpriteHit(MiniSprite sprite, double x, double y)
{
	double dx = x - 0.5;
	double dy = y - 0.5;
	switch (sprite) {
		case MiniSprite::Disc:
		case MiniSprite::Tree:
		case MiniSprite::RoadVeh:
			return dx * dx + dy * dy <= 0.25;
		case MiniSprite::Diamond:
		case MiniSprite::Ship:
			return std::abs(dx) + std::abs(dy) <= 0.5;
		case MiniSprite::Triangle:
		case MiniSprite::Aircraft:
			return std::abs(dx) <= y * 0.5;
		default: return true;
	}
}

void MiniAtlasEnsure()
{
	if (_atlas_tex != NO_TEXTURE) return;

	std::vector<uint32_t> px((size_t)ATLAS_WIDTH * ATLAS_HEIGHT, 0x00FFFFFFU);
	std::vector<uint32_t> art((size_t)ATLAS_CONTENT * ATLAS_CONTENT);

	for (int i = 0; i < (int)MiniSprite::End; i++) {
		int ox = (i % ATLAS_COLS) * ATLAS_CELL + ATLAS_GUTTER;
		int oy = (i / ATLAS_COLS) * ATLAS_CELL + ATLAS_GUTTER;
		std::string path = _personal_dir + "mini_art/" + _slot_names[i] + ".png";
		_has_art[i] = LoadArtImage(path, art, ATLAS_CONTENT, ATLAS_CONTENT);
		if (_has_art[i]) {
			for (int y = 0; y < ATLAS_CONTENT; y++) {
				std::copy_n(&art[(size_t)y * ATLAS_CONTENT], ATLAS_CONTENT, &px[(size_t)(oy + y) * ATLAS_WIDTH + ox]);
			}
			if (IsGroundSlot((MiniSprite)i)) _tile_tex[i] = _textures.Add(art, Dimension(ATLAS_CONTENT, ATLAS_CONTENT), TextureFilter::Mipmapped, TextureWrap::Repeat);
			continue;
		}
		for (int y = 0; y < ATLAS_CONTENT; y++) {
			for (int x = 0; x < ATLAS_CONTENT; x++) {
				int hits = 0;
				for (int sy = 0; sy < 4; sy++) {
					for (int sx = 0; sx < 4; sx++) {
						double u = (x + (sx + 0.5) / 4.0) / ATLAS_CONTENT;
						double v = (y + (sy + 0.5) / 4.0) / ATLAS_CONTENT;
						if (SpriteHit((MiniSprite)i, u, v)) hits++;
					}
				}
				uint32_t a = hits * 255 / 16;
				px[(size_t)(oy + y) * ATLAS_WIDTH + ox + x] = (a << 24) | 0x00FFFFFFU;
			}
		}
	}

	_atlas_tex = _textures.Add(px, Dimension(ATLAS_WIDTH, ATLAS_HEIGHT), TextureFilter::Mipmapped);
}

/* Frees the textures so the next frame rebuilds them and re-reads art files. */
void MiniAtlasReload()
{
	_textures.Remove(std::exchange(_atlas_tex, NO_TEXTURE));
	for (TextureId &t : _tile_tex) _textures.Remove(std::exchange(t, NO_TEXTURE));
}

bool MiniAtlasHasArt(MiniSprite sprite)
{
	return sprite < MiniSprite::End && _has_art[(size_t)sprite];
}

static bool AtlasSprite(MiniSprite sprite, int x0, int y0, int x1, int y1, int angle_deg, uint32_t argb)
{
	if (_atlas_tex == NO_TEXTURE || sprite >= MiniSprite::End || x1 < x0 || y1 < y0) return false;
	float left = ((int)sprite % ATLAS_COLS) * ATLAS_CELL + ATLAS_GUTTER;
	float top = ((int)sprite / ATLAS_COLS) * ATLAS_CELL + ATLAS_GUTTER;
	UvRect cell = {left / ATLAS_WIDTH, top / ATLAS_HEIGHT, (left + ATLAS_CONTENT) / ATLAS_WIDTH, (top + ATLAS_CONTENT) / ATLAS_HEIGHT};
	_map_draw.Image(_atlas_tex, {x0, y0, x1, y1}, cell, angle_deg, argb);
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

/* One quad for a vertical run of equal tiles; the repeat wrap keeps the
 * pattern continuous, so no per-tile draw calls and no seams. */
bool MiniAtlasTileRun(MiniSprite sprite, int x0, int y0, int x1, int y1, int run_tiles, uint32_t argb)
{
	if (sprite >= MiniSprite::End || x1 < x0 || y1 < y0) return false;
	TextureId tex = _tile_tex[(size_t)sprite];
	if (tex == NO_TEXTURE) return false;
	_map_draw.Image(tex, {x0, y0, x1, y1}, {0.0f, 0.0f, 1.0f, static_cast<float>(std::max(run_tiles, 1))}, 0, argb);
	return true;
}
