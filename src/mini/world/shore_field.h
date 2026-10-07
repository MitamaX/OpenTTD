/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file shore_field.h How far out from the shore every tile of open water lies, measured straight across the water rather than tile by tile. */

#ifndef MINI_WORLD_SHORE_FIELD_H
#define MINI_WORLD_SHORE_FIELD_H

#include <cstdint>
#include <vector>

#include "../../core/geometry_type.hpp"

/* One texel per tile: the distance from its middle to the edge of the nearest tile that is not open water, a share of the shelf's breadth. */
class ShoreField {
public:
	void Survey(Dimension map);
	const uint8_t *Texels() const { return this->texels.data(); }

private:
	std::vector<uint8_t> texels;
};

#endif /* MINI_WORLD_SHORE_FIELD_H */
