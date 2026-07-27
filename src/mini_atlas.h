/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mini_atlas.h Sprite atlas for the mini UI; white placeholder shapes stand in until real art lands. */

#ifndef MINI_ATLAS_H
#define MINI_ATLAS_H

#include <cstdint>

enum class MiniSprite : uint8_t {
	Disc,
	Diamond,
	Triangle,
	End,
};

void MiniAtlasEnsure();
void MiniAtlasReset();
bool MiniAtlasQuad(MiniSprite sprite, int x0, int y0, int x1, int y1, uint32_t argb);

#endif /* MINI_ATLAS_H */
