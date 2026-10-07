/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sea_frame.cpp The flat open sea around the map out to the horizon, laid so the meshes inside meet it without cracks. */

#include "../../stdafx.h"
#include "sea_frame.h"

#include "../../safeguards.h"

/* One side of the frame, going round the map: the margin's corner it starts from, the way it runs, how many tiles of the map it passes and the way out from it. */
struct FrameSide {
	MapVector from;
	MapVector along;
	int tiles;
	MapVector outward;
};

/* Each side fans out from its rim to the middle of its far edge, and closes on the far corners it shares with the sides beside it. */
SeaFrame::SeaFrame(Dimension map, double margin, double reach)
{
	double width = map.width;
	double height = map.height;
	int columns = static_cast<int>(map.width);
	int rows = static_cast<int>(map.height);
	const std::array<FrameSide, 4> sides = {{
		{{-margin, -margin}, {1.0, 0.0}, columns, {0.0, -1.0}},
		{{width + margin, -margin}, {0.0, 1.0}, rows, {1.0, 0.0}},
		{{width + margin, height + margin}, {-1.0, 0.0}, columns, {0.0, 1.0}},
		{{-margin, height + margin}, {0.0, -1.0}, rows, {-1.0, 0.0}},
	}};

	auto add = [&](const MapVector &point) {
		this->points.push_back(point);
		return static_cast<uint32_t>(this->points.size() - 1);
	};
	for (const FrameSide &side : sides) {
		auto at = [&](double run) { return side.from + side.along * run; };
		std::vector<uint32_t> rim = {add(at(0.0))};
		for (int tile = 0; tile <= side.tiles; tile++) {
			if (margin > 0.0 || tile > 0) rim.push_back(add(at(margin + tile)));
		}
		if (margin > 0.0) rim.push_back(add(at(side.tiles + 2.0 * margin)));

		MapVector out = side.outward * reach;
		uint32_t far_start = add(at(-reach) + out);
		uint32_t far_end = add(at(side.tiles + 2.0 * margin + reach) + out);
		uint32_t far_middle = add(at((side.tiles + 2.0 * margin) * 0.5) + out);
		for (size_t i = 0; i + 1 < rim.size(); i++) this->triangles.push_back({rim[i], rim[i + 1], far_middle});
		this->triangles.push_back({rim.front(), far_middle, far_start});
		this->triangles.push_back({rim.back(), far_end, far_middle});
	}
}
