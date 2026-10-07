/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file shore_field.cpp How far out from the shore every tile of open water lies, measured straight across the water rather than tile by tile. */

#include "../../stdafx.h"
#include "shore_field.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "seabed.h"

#include "../../safeguards.h"

static constexpr double UNREACHED = 1e12;
static constexpr double LAND_EDGE = 0.5;
static constexpr double TEXEL_SCALE = 255.0;

/* The squared distance from every point of a line to the nearest of its sites, each site weighed by its own squared distance already found:
 * the lower envelope of the parabolas rooted at the sites, after Felzenszwalb and Huttenlocher. */
static void SquaredDistances(std::vector<double> &line)
{
	size_t count = line.size();
	std::vector<size_t> roots(count);
	std::vector<double> bounds(count + 1);
	auto crossing = [&](size_t a, size_t b) {
		double da = static_cast<double>(a);
		double db = static_cast<double>(b);
		return ((line[b] + db * db) - (line[a] + da * da)) / (2.0 * (db - da));
	};

	size_t hull = 0;
	roots[0] = 0;
	bounds[0] = -std::numeric_limits<double>::infinity();
	bounds[1] = std::numeric_limits<double>::infinity();
	for (size_t q = 1; q < count; q++) {
		double meet = crossing(roots[hull], q);
		while (meet <= bounds[hull]) meet = crossing(roots[--hull], q);
		roots[++hull] = q;
		bounds[hull] = meet;
		bounds[hull + 1] = std::numeric_limits<double>::infinity();
	}

	std::vector<double> found(count);
	hull = 0;
	for (size_t q = 0; q < count; q++) {
		while (bounds[hull + 1] < static_cast<double>(q)) hull++;
		double offset = static_cast<double>(q) - static_cast<double>(roots[hull]);
		found[q] = offset * offset + line[roots[hull]];
	}
	line = std::move(found);
}

/* Each tile's squared distance to the nearest land tile's middle, by rows and then by columns; less half a tile, that is the distance to the land's edge. */
void ShoreField::Survey(Dimension map)
{
	size_t width = map.width;
	size_t height = map.height;
	std::vector<double> squared(width * height);
	for (size_t y = 0; y < height; y++) {
		for (size_t x = 0; x < width; x++) {
			bool land = WaterFormOf(static_cast<int>(x), static_cast<int>(y)) != WaterForm::Open;
			squared[y * width + x] = land ? 0.0 : UNREACHED;
		}
	}

	std::vector<double> line(width);
	for (size_t y = 0; y < height; y++) {
		std::copy_n(squared.begin() + static_cast<ptrdiff_t>(y * width), width, line.begin());
		SquaredDistances(line);
		std::ranges::copy(line, squared.begin() + static_cast<ptrdiff_t>(y * width));
	}
	line.resize(height);
	for (size_t x = 0; x < width; x++) {
		for (size_t y = 0; y < height; y++) line[y] = squared[y * width + x];
		SquaredDistances(line);
		for (size_t y = 0; y < height; y++) squared[y * width + x] = line[y];
	}

	this->texels.resize(width * height);
	std::ranges::transform(squared, this->texels.begin(), [](double distance_squared) {
		double offshore = std::clamp(std::sqrt(distance_squared) - LAND_EDGE, 0.0, static_cast<double>(SHELF_TILES));
		return static_cast<uint8_t>(std::lround(offshore / SHELF_TILES * TEXEL_SCALE));
	});
}
