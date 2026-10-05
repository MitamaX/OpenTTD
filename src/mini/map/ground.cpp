/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ground.cpp The colour and art of bare ground, by height, slope and cover. */

#include "../../stdafx.h"
#include "ground.h"

#include "../../clear_map.h"
#include "../../core/math_func.hpp"
#include "../../landscape.h"
#include "../../slope_func.h"
#include "../core/canvas.h"
#include "../core/tones.h"
#include "../core/tuning.h"
#include "zoom_detail.h"

#include "../../safeguards.h"

uint32_t GroundColour(TileIndex tile, int h)
{
	h = Clamp(h, 0, 15);
	switch (GetTileType(tile)) {
		case MP_CLEAR:
			switch (GetClearGround(tile)) {
				case CLEAR_FIELDS: return COL_FIELDS;
				case CLEAR_ROCKS: return COL_ROCKS;
				case CLEAR_SNOW: return COL_SNOW;
				case CLEAR_DESERT: return COL_DESERT;
				default: return _height_ramp[h];
			}
		default:
			return _height_ramp[h];
	}
}

MiniSprite GroundSlot(TileIndex tile)
{
	if (GetTileType(tile) == MP_CLEAR) {
		switch (GetClearGround(tile)) {
			case CLEAR_FIELDS: return MiniSprite::Field;
			case CLEAR_ROCKS: return MiniSprite::Rock;
			case CLEAR_SNOW: return MiniSprite::Snow;
			case CLEAR_DESERT: return MiniSprite::Desert;
			default: break;
		}
	}
	return MiniSprite::Grass;
}

static uint32_t RampLerp(double h)
{
	h = Clamp(h, 0.0, 15.0);
	int i = (int)h;
	if (i >= 15) return _height_ramp[15];
	return Mix(_height_ramp[i], _height_ramp[i + 1], (uint)((h - i) * 255.0));
}

static bool IsSpecialGround(TileIndex tile)
{
	if (GetTileType(tile) != MP_CLEAR) return false;
	switch (GetClearGround(tile)) {
		case CLEAR_FIELDS:
		case CLEAR_ROCKS:
		case CLEAR_SNOW:
		case CLEAR_DESERT:
			return true;
		default:
			return false;
	}
}

uint32_t GroundOverviewColour(TileIndex tile, Slope s, int hbase)
{
	double avg = (GetSlopeZInCorner(s, CORNER_N) + GetSlopeZInCorner(s, CORNER_W) + GetSlopeZInCorner(s, CORNER_E) + GetSlopeZInCorner(s, CORNER_S)) / 4.0;
	if (IsSpecialGround(tile)) return Mix(GroundColour(tile, hbase), COL_SHADOW, std::min(255, (int)(_tuning.relief_strength * avg)));
	return RampLerp(hbase + avg);
}

/* Sloped ground is one gradient quad: the corner colours sample the height
 * ramp at the corner levels and the GPU interpolates between them, so the
 * top of a slope lands on exactly the colour of the next level and
 * gradients run tile to tile. */
void DrawGround(TileIndex tile, int x0, int y0, int x1, int y1, int ppt)
{
	auto [s, hbase] = GetTileSlopeZ(tile);

	/* Ground art is a luminance texture tinted by the ramp colour, so height
	 * bands and hillshading survive the swap to real tiles. The overview tier
	 * stays on flat colours: per-tile quads are slow at that tile count and
	 * their seams read as a grid. */
	MiniSprite slot = GroundSlot(tile);
	if (ppt >= INFRASTRUCTURE_PPT && MiniAtlasHasArt(slot)) {
		uint32_t c = s == SLOPE_FLAT ? GroundColour(tile, hbase) : GroundOverviewColour(tile, s, hbase);
		if (MiniAtlasQuad(slot, x0, y0, x1, y1, _canvas.Tone(c))) return;
	}

	if (s == SLOPE_FLAT) {
		_canvas.FillRect(x0, y0, x1, y1, GroundColour(tile, hbase));
		return;
	}

	if (ppt < INFRASTRUCTURE_PPT) {
		_canvas.FillRect(x0, y0, x1, y1, GroundOverviewColour(tile, s, hbase));
		return;
	}

	bool special = IsSpecialGround(tile);
	uint32_t flat = GroundColour(tile, hbase);
	auto corner = [&](Corner cn) {
		int z = GetSlopeZInCorner(s, cn);
		return _canvas.Tone(special ? Mix(flat, COL_SHADOW, std::min(255, _tuning.relief_strength * z)) : RampLerp(hbase + z));
	};
	_map_draw.FillGradient(x0, y0, x1, y1, corner(CORNER_N), corner(CORNER_E), corner(CORNER_W), corner(CORNER_S));
}
