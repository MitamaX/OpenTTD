/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file house_forms.h Town houses as building forms and the trees of town parks. */

#ifndef MINI_MAP_HOUSE_FORMS_H
#define MINI_MAP_HOUSE_FORMS_H

#include <optional>

#include "../../tile_type.h"
#include "building_form.h"

std::optional<BuildingForm> HouseForm(TileIndex tile);
std::optional<FloraPatch> HouseFlora(TileIndex tile);

#endif /* MINI_MAP_HOUSE_FORMS_H */
