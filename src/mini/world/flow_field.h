/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file flow_field.h Which way and how fast the water runs on every tile: rivers down toward the sea, canals gently, and a river on out past its mouth. */

#ifndef MINI_WORLD_FLOW_FIELD_H
#define MINI_WORLD_FLOW_FIELD_H

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "../../core/geometry_type.hpp"
#include "../../direction_type.h"
#include "../../tile_type.h"

/* A texel channel holds still water at this value, and full pace this far above or below it. */
inline constexpr double FLOW_TEXEL_STILL = 128.0;
inline constexpr double FLOW_TEXEL_SPAN = 127.0;

/* A tile's texel: the water's run along the map's X and along its Y, each a share of full pace. */
using FlowTexel = std::array<uint8_t, 2>;

class Drainage;

/* A change near running water drains again only the water joined to it, and fans the sea out again only past the mouths that water ran out at or runs out at now. */
class FlowField {
public:
	void Survey(Dimension map);
	std::optional<Rect> Resurvey(std::span<const Rect> changed);
	const void *Texels() const { return this->texels.data(); }

private:
	void Refan(const Drainage &drainage, const Rect &window, std::optional<Rect> &moved);
	void Put(TileIndex tile, const FlowTexel &texel, std::optional<Rect> &moved);

	std::vector<FlowTexel> texels;
	std::vector<DiagDirection> downstream;
};

#endif /* MINI_WORLD_FLOW_FIELD_H */
