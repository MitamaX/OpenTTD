/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file airfield_marks.h What an airport tile is in the game's own airport layouts, and the markings its ground is painted with. */

#ifndef MINI_MAP_AIRFIELD_MARKS_H
#define MINI_MAP_AIRFIELD_MARKS_H

#include <array>
#include <optional>
#include <string_view>

#include "../../core/enum_type.hpp"
#include "../../station_map.h"

enum class AirfieldMark : uint8_t {
	None,
	Apron,
	Taxiway,
	Runway,
	Stand,
	Helipad,
	Grass,
	End,
};

inline constexpr std::array<std::string_view, to_underlying(AirfieldMark::End)> AIRFIELD_MARK_NAMES = {
	"NONE", "APRON", "TAXIWAY", "RUNWAY", "STAND", "HELIPAD", "GRASS",
};

/* The game's own airport tile a tile is drawn as, a NewGRF tile standing in for the one it substitutes; none for a NewGRF tile substituting nothing. */
std::optional<StationGfx> AirportStandIn(TileIndex tile);

AirfieldMark AirfieldMarkOf(TileIndex tile);

#endif /* MINI_MAP_AIRFIELD_MARKS_H */
