/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file industry_forms.h Industry tiles as building forms, and the trees of plantations and forests. */

#ifndef MINI_MAP_INDUSTRY_FORMS_H
#define MINI_MAP_INDUSTRY_FORMS_H

#include <optional>

#include "../../tile_type.h"
#include "building_form.h"

std::optional<BuildingForm> IndustryForm(TileIndex tile);
std::optional<FloraPatch> IndustryFlora(TileIndex tile);

#endif /* MINI_MAP_INDUSTRY_FORMS_H */
