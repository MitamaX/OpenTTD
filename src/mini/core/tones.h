/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tones.h The colours the mini UI paints the map with, as 0xAARRGGBB. */

#ifndef MINI_CORE_TONES_H
#define MINI_CORE_TONES_H

#include "../../cargo_type.h"
#include "../../gfx_type.h"
#include "../gpu/draw_list.h"

constexpr uint32_t Mix(uint32_t dst, uint32_t src, uint alpha)
{
	uint inv = CHANNEL_MAX - alpha;
	uint32_t rb = ((dst & 0xFF00FFU) * inv + (src & 0xFF00FFU) * alpha) >> 8;
	uint32_t g = ((dst & 0x00FF00U) * inv + (src & 0x00FF00U) * alpha) >> 8;
	return 0xFF000000U | (rb & 0xFF00FFU) | (g & 0x00FF00U);
}

/* Ink draws outlines, paper draws highlights; everything else is a fill. */
inline constexpr uint32_t COL_INK = 0xFF14181CU;
inline constexpr uint32_t COL_PAPER = 0xFFEDF2F7U;
inline constexpr uint32_t COL_SHADOW = 0xFF000000U;

/* Every bordered block darkens its fill by the same cut. */
constexpr uint32_t Darken(uint32_t c)
{
	return Mix(c, COL_SHADOW, 82);
}

inline constexpr uint32_t COL_VOID = 0xFF0A0A0AU;
inline constexpr uint32_t COL_WATER = 0xFF2F6EA5U;
inline constexpr uint32_t COL_SNOW = 0xFFF0F4F7U;
inline constexpr uint32_t COL_DESERT = 0xFFE0CA8CU;
inline constexpr uint32_t COL_ROCKS = 0xFF8E979EU;
inline constexpr uint32_t COL_FIELDS = 0xFFD4B23AU;
inline constexpr uint32_t COL_TREE = 0xFF2F4A2AU;

inline constexpr uint32_t COL_RAIL = 0xFF33383DU;
inline constexpr uint32_t COL_ROAD = 0xFF61686EU;
inline constexpr uint32_t COL_BRIDGE = 0xFF9AA0A6U;
inline constexpr uint32_t COL_TUNNEL = 0xFF1E2124U;
inline constexpr uint32_t COL_CATENARY = 0xFFE8C94AU;
inline constexpr uint32_t COL_BALLAST = 0xFF6E6A63U;
inline constexpr uint32_t COL_CONCRETE = 0xFFA3A199U;
inline constexpr uint32_t COL_STEEL = 0xFFB4B8BCU;
inline constexpr uint32_t COL_ASPHALT = 0xFF45484CU;
inline constexpr uint32_t COL_WIRE = 0xFF2A2D30U;
inline constexpr uint32_t COL_RAIL_ACCENT = COL_PAPER;
inline constexpr uint32_t COL_ROAD_ACCENT = COL_CATENARY;

inline constexpr uint32_t COL_HOUSE = 0xFF9C8A76U;
inline constexpr uint32_t COL_IND = 0xFFD07A4AU;
inline constexpr uint32_t COL_OBJ = 0xFFB0B4B8U;

inline constexpr uint32_t COL_ST_RAIL = 0xFF4A6FA5U;
inline constexpr uint32_t COL_ST_RAIL_B = Darken(COL_ST_RAIL);
inline constexpr uint32_t COL_ST_AIR = 0xFF8E6FB8U;
inline constexpr uint32_t COL_ST_ROAD = 0xFF7FA8C9U;
inline constexpr uint32_t COL_ST_ROAD_B = Darken(COL_ST_ROAD);
inline constexpr uint32_t COL_ST_DOCK = 0xFF9A7FA8U;
inline constexpr uint32_t COL_ST_BUOY = 0xFFD8C86AU;

inline constexpr uint32_t COL_GO = 0xFF3FCB6AU;
inline constexpr uint32_t COL_STOP = 0xFFE04B4BU;
inline constexpr uint32_t COL_BP = 0xFF7FD1FFU;
inline constexpr uint32_t COL_BP_RM = 0xFFFF6B6BU;
/* A tile the game would refuse. Neither blueprint blue nor removal red, so a
 * hole in a long drag reads as a hole rather than as intent. */
inline constexpr uint32_t COL_BP_NO = 0xFF8C8578U;

/* All-green ramp like the old top-down renderer settled on: one step
 * darker per height level, hue constant so slopes match their neighbours. */
inline constexpr uint32_t _height_ramp[16] = {
	0xFF9CCB74U, 0xFF91C16CU, 0xFF86B765U, 0xFF7CAD5EU,
	0xFF71A357U, 0xFF679950U, 0xFF5D8F49U, 0xFF538443U,
	0xFF4A7A3DU, 0xFF417037U, 0xFF386631U, 0xFF305C2BU,
	0xFF285226U, 0xFF214921U, 0xFF1A3F1CU, 0xFF143618U,
};

inline constexpr uint32_t _company_rgb[16] = {
	0xFF1F3A93U, 0xFF9CCC65U, 0xFFEC8FB0U, 0xFFF2D24BU,
	0xFFD64541U, 0xFF4FC3F7U, 0xFF66BB6AU, 0xFF2E7D32U,
	0xFF3D6DCCU, 0xFFEFE5C0U, 0xFF9E8FA8U, 0xFF8E6FB8U,
	0xFFF29C4AU, 0xFF8D6E63U, 0xFF9E9E9EU, 0xFFF5F5F5U,
};

uint32_t PaletteRgb(PixelColour p);
uint32_t CargoRgb(CargoType ct);

#endif /* MINI_CORE_TONES_H */
