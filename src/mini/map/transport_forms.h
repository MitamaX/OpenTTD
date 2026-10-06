/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file transport_forms.h Depots, tunnel portals and the buildings of stations that are not rail stations, as building forms. */

#ifndef MINI_MAP_TRANSPORT_FORMS_H
#define MINI_MAP_TRANSPORT_FORMS_H

#include <optional>

#include "../../tile_type.h"
#include "building_form.h"

std::optional<BuildingForm> DepotForm(TileIndex tile);
std::optional<BuildingForm> StationForm(TileIndex tile);
std::optional<BuildingForm> TunnelForm(TileIndex tile);

#endif /* MINI_MAP_TRANSPORT_FORMS_H */
