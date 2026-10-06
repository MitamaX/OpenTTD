/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file overview.h A picture of the whole map, one pixel per sampled tile, with vehicles and cargo flow drawn over it. */

#ifndef MINI_MAP_OVERVIEW_H
#define MINI_MAP_OVERVIEW_H

#include <span>
#include <vector>

#include "../../core/geometry_type.hpp"
#include "../../tile_type.h"
#include "../core/camera.h"

enum class OverviewMode : uint8_t {
	Contour,
	Vehicles,
	Industries,
	Routes,
	Flow,
	Vegetation,
	Owner,
};

class Overview {
public:
	void Paint(int width, int height, OverviewMode mode);

	std::span<const uint32_t> Pixels() const { return this->pixels; }
	int Width() const { return this->width; }
	int Height() const { return this->height; }

	ExactPoint ExactPixelOf(double tile_x, double tile_y) const;
	Point PixelOf(double tile_x, double tile_y) const;
	TileIndex TileAt(int x, int y) const;

private:
	void PaintTiles(OverviewMode mode);
	void PaintVehicles();
	void PaintFlow();
	void Dot(Point at, int radius, uint32_t colour);
	void Line(Point from, Point to, int thickness, uint32_t colour);

	std::vector<uint32_t> pixels;
	int width = 0;
	int height = 0;
};

#endif /* MINI_MAP_OVERVIEW_H */
