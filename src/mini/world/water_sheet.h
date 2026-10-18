/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file water_sheet.h The level a tile's sheet of water stands at over every point of the tile. */

#ifndef MINI_WORLD_WATER_SHEET_H
#define MINI_WORLD_WATER_SHEET_H

#include <optional>

#include "../../map_type.h"
#include "../map/world_tiles.h"

inline constexpr int CHANNEL_DIVISIONS = 6;

class WaterSheet {
public:
	WaterSheet(int tx, int ty);

	double Level(double x, double y) const;
	int Divisions() const;

private:
	struct FallEnds {
		double foot;
		double lip;
	};

	static FallEnds FallEndsOf(int tx, int ty);
	double SpreadFromFall(double x, double y) const;

	int tx;
	int ty;
	SurfaceTexel surface;
	double low;
	bool lies_level;
	std::optional<FallEnds> fall;
	std::optional<TileIndexDiffC> fall_side;
};

#endif /* MINI_WORLD_WATER_SHEET_H */
