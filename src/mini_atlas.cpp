/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mini_atlas.cpp Builds the map atlas at run time and draws tinted sprite quads from it. */

#include "stdafx.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "fileio_func.h"
#include "mini_atlas.h"
#include "mini/art/material_atlas.h"
#include "mini/core/canvas.h"
#include "mini/gpu/art_image.h"
#include "mini/gpu/texture_store.h"

#include "safeguards.h"

/* One cell past the sprites is solid white, the texel untextured map shapes sample. */
static constexpr int SOLID_CELL = to_underlying(MiniSprite::End);
static constexpr int SPRITE_TEXELS = SPRITE_GRID.content_w;
static constexpr int SAMPLES_PER_AXIS = 4;
static constexpr int SAMPLES_PER_TEXEL = SAMPLES_PER_AXIS * SAMPLES_PER_AXIS;
static constexpr double SAMPLE_CENTRE = 0.5;
static constexpr uint32_t CLEAR_WHITE = 0x00FFFFFFU;
static constexpr uint32_t OPAQUE_WHITE = 0xFFFFFFFFU;

static_assert(SOLID_CELL < SPRITE_GRID.columns * SPRITE_GRID.rows);
static_assert(SPRITE_GRID.content_w == SPRITE_GRID.content_h);

static TextureId _atlas_tex = NO_TEXTURE;

/* Art file base names inside mini_art, one per MiniSprite slot. */
static const char *_slot_names[] = {
	"disc", "diamond", "triangle", "road_vehicle", "ship", "aircraft",
};
static_assert(lengthof(_slot_names) == to_underlying(MiniSprite::End));

static size_t TexelIndex(int x, int y)
{
	return static_cast<size_t>(y) * ATLAS_WIDTH + x;
}

/* Shapes live in a unit square; x grows right, y grows down. */
static bool SpriteHit(MiniSprite sprite, double x, double y)
{
	double dx = x - 0.5;
	double dy = y - 0.5;
	switch (sprite) {
		case MiniSprite::Disc:
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

/* A texel is as opaque as the share of its samples the shape covers. */
static uint32_t PlaceholderTexel(MiniSprite sprite, int x, int y)
{
	uint hits = 0;
	for (int sy = 0; sy < SAMPLES_PER_AXIS; sy++) {
		for (int sx = 0; sx < SAMPLES_PER_AXIS; sx++) {
			double u = (x + (sx + SAMPLE_CENTRE) / SAMPLES_PER_AXIS) / SPRITE_TEXELS;
			double v = (y + (sy + SAMPLE_CENTRE) / SAMPLES_PER_AXIS) / SPRITE_TEXELS;
			if (SpriteHit(sprite, u, v)) hits++;
		}
	}
	return WithAlpha(OPAQUE_WHITE, hits * CHANNEL_MAX / SAMPLES_PER_TEXEL);
}

static void PaintPlaceholder(std::span<uint32_t> px, Point origin, MiniSprite sprite)
{
	for (int y = 0; y < SPRITE_TEXELS; y++) {
		for (int x = 0; x < SPRITE_TEXELS; x++) px[TexelIndex(origin.x + x, origin.y + y)] = PlaceholderTexel(sprite, x, y);
	}
}

static void CopyArt(std::span<uint32_t> px, Point origin, std::span<const uint32_t> art)
{
	for (int y = 0; y < SPRITE_TEXELS; y++) std::ranges::copy(art.subspan(static_cast<size_t>(y) * SPRITE_TEXELS, SPRITE_TEXELS), px.begin() + TexelIndex(origin.x, origin.y + y));
}

/* A sprite without an art file of its own stands in as a white shape. */
static void PlaceSprite(std::span<uint32_t> px, MiniSprite sprite, std::span<uint32_t> art)
{
	std::string path = _personal_dir + "mini_art/" + _slot_names[to_underlying(sprite)] + ".png";
	Point origin = ContentOrigin(SPRITE_GRID, to_underlying(sprite));
	if (LoadArtImage(path, art, SPRITE_TEXELS, SPRITE_TEXELS)) {
		CopyArt(px, origin, art);
	} else {
		PaintPlaceholder(px, origin, sprite);
	}
}

/* The gutter is white too, so every mipmap level still samples white at the cell's centre. */
static void PaintSolidCell(std::span<uint32_t> px)
{
	Point content = ContentOrigin(SPRITE_GRID, SOLID_CELL);
	for (int y = 0; y < SPRITE_GRID.pitch_y; y++) {
		std::fill_n(px.begin() + TexelIndex(content.x - ATLAS_GUTTER, content.y - ATLAS_GUTTER + y), SPRITE_GRID.pitch_x, OPAQUE_WHITE);
	}
}

void MiniAtlasEnsure()
{
	if (_atlas_tex != NO_TEXTURE) return;

	std::vector<uint32_t> px(static_cast<size_t>(ATLAS_WIDTH) * ATLAS_HEIGHT, CLEAR_WHITE);
	std::vector<uint32_t> art(static_cast<size_t>(SPRITE_TEXELS) * SPRITE_TEXELS);
	for (int sprite = 0; sprite < to_underlying(MiniSprite::End); sprite++) PlaceSprite(px, static_cast<MiniSprite>(sprite), art);
	PaintSolidCell(px);
	PaintMaterialCells(px);

	_atlas_tex = _textures.Add(px, Dimension(ATLAS_WIDTH, ATLAS_HEIGHT), TextureFilter::Mipmapped, TextureWrap::Clamp, MAX_MIP_LEVEL);
	_map_draw.SetSolid(MiniAtlasSolid());
}

/* Frees the texture so the next frame rebuilds it and re-reads the art files; the materials come back from their cache. */
void MiniAtlasReload()
{
	_map_draw.SetSolid({});
	_textures.Remove(std::exchange(_atlas_tex, NO_TEXTURE));
}

SolidTexel MiniAtlasSolid()
{
	UvRect cell = ContentUv(SPRITE_GRID, SOLID_CELL);
	return {_atlas_tex, std::midpoint(cell.left, cell.right), std::midpoint(cell.top, cell.bottom)};
}

static bool Drawable(MiniSprite sprite)
{
	return _atlas_tex != NO_TEXTURE && sprite < MiniSprite::End;
}

static bool AtlasSprite(MiniSprite sprite, int x0, int y0, int x1, int y1, float angle_deg, uint32_t argb)
{
	if (!Drawable(sprite) || x1 < x0 || y1 < y0) return false;
	_map_draw.Image(_atlas_tex, {x0, y0, x1, y1}, ContentUv(SPRITE_GRID, to_underlying(sprite)), angle_deg, argb);
	return true;
}

bool MiniAtlasQuad(MiniSprite sprite, int x0, int y0, int x1, int y1, uint32_t argb)
{
	return AtlasSprite(sprite, x0, y0, x1, y1, 0.0f, argb);
}

bool MiniAtlasQuadRot(MiniSprite sprite, int cx, int cy, int r, float angle_deg, uint32_t argb)
{
	return AtlasSprite(sprite, cx - r, cy - r, cx + r, cy + r, angle_deg, argb);
}
