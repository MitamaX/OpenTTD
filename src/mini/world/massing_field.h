/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file massing_field.h The tallest body standing on every tile as a box, one texel per tile, for glass to find what it mirrors. */

#ifndef MINI_WORLD_MASSING_FIELD_H
#define MINI_WORLD_MASSING_FIELD_H

#include <cstdint>
#include <optional>
#include <vector>

#include "../../core/geometry_type.hpp"
#include "../../tile_type.h"
#include "../map/building_form.h"

inline constexpr uint MASSING_GRID_MASK = 0x000F;
inline constexpr uint MASSING_FAR_X_BIT = 0x0010;
inline constexpr uint MASSING_FAR_Y_BIT = 0x0020;
inline constexpr uint MASSING_RAISED_SHIFT = 6;
inline constexpr uint MASSING_RAISED_MOST = 3;
inline constexpr uint MASSING_INSET_SHIFT = 8;
inline constexpr uint MASSING_INSET_BITS = 2;
inline constexpr uint MASSING_INSET_STEPS = 8;
inline constexpr uint MASSING_INSET_MOST = 3;
inline constexpr uint MASSING_EAVES_SHIFT = 8;
inline constexpr uint MASSING_LOW_BYTE = 0x00FF;
inline constexpr float MASSING_LEAST_COVER = 0.25f;
inline constexpr int MASSING_BLOCK_TILES = 16;

static_assert(MAX_FOOTPRINT_TILES == 2);
static_assert(WINDOW_GRID_NAMES.size() <= MASSING_GRID_MASK + 1);
static_assert(MAX_STRUCTURE_TILES * TEXELS_PER_TILE <= 255);
static_assert(MASSING_INSET_MOST < 1U << MASSING_INSET_BITS);

struct MassingTexel {
	uint16_t heights = 0;
	uint16_t wall = 0;
	uint16_t roof = 0;
	uint16_t traits = 0;

	bool operator==(const MassingTexel &) const = default;
};

MassingTexel MassingTexelOf(TileIndex tile, const std::optional<BuildingForm> &form);

struct MassingBox {
	float low_x;
	float low_y;
	float high_x;
	float high_y;
	float foot;
	float top;
};

class MassingField {
public:
	void Clear(Dimension map);
	void Note(TileIndex tile, const MassingTexel &texel);
	std::vector<Rect> TakeMoved();
	std::optional<MassingBox> BoxAt(int x, int y, double rise) const;
	std::optional<TileIndex> AnchorAt(int x, int y) const;
	const void *Texels() const { return this->texels.data(); }

private:
	const MassingTexel *TexelAt(int x, int y) const;

	Dimension map{};
	std::vector<MassingTexel> texels;
	std::vector<Rect> moved;
};

#endif /* MINI_WORLD_MASSING_FIELD_H */
