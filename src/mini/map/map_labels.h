/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_labels.h Name plates over towns, stations, industries and signs. */

#ifndef MINI_MAP_MAP_LABELS_H
#define MINI_MAP_MAP_LABELS_H

#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "../../core/geometry_type.hpp"
#include "../../gfx_type.h"
#include "../../industry_type.h"
#include "../../signs_type.h"
#include "../../station_type.h"
#include "../../town_type.h"

/* Listed in click priority: where plates overlap, the earlier kind wins. */
using LabelTarget = std::variant<SignID, StationID, IndustryID, TownID>;

class MapLabels {
public:
	void Paint(int ppt);
	std::optional<LabelTarget> HitAt(int x, int y) const;

private:
	struct Plate {
		Rect area;
		LabelTarget target;
	};

	void Place(Point at, std::string_view str, uint32_t fill, bool transparent, TextColour tc, LabelTarget target);
	bool Visible(Point at) const;

	std::vector<Plate> plates;
};

extern MapLabels _map_labels;

#endif /* MINI_MAP_MAP_LABELS_H */
