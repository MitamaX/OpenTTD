/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file water_sheet.h The level a tile's sheet of water stands at over every point of the tile. */

#ifndef MINI_WORLD_WATER_SHEET_H
#define MINI_WORLD_WATER_SHEET_H

#include "../map/world_tiles.h"

class WaterSheet {
public:
	WaterSheet(int tx, int ty);

	double Level(double x, double y) const;
	int Divisions() const;

private:
	enum class Shape : uint8_t {
		Level,
		Plane,
	};

	static Shape ShapeOf(int tx, int ty);

	int tx;
	int ty;
	SurfaceTexel surface;
	double low;
	Shape shape;
};

#endif /* MINI_WORLD_WATER_SHEET_H */
