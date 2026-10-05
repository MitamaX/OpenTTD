/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file window_links.h Ways from one mini window into another, or onto the map. */

#ifndef MINI_WINDOWS_WINDOW_LINKS_H
#define MINI_WINDOWS_WINDOW_LINKS_H

#include "../../industry_type.h"
#include "../../news_type.h"
#include "../../station_type.h"
#include "../../tile_type.h"
#include "../../town_type.h"
#include "../../vehicle_type.h"

void OpenVehicleWindow(VehicleID vehicle);
void OpenStationWindow(StationID station);
void OpenTownWindow(TownID town);
void OpenIndustryWindow(IndustryID industry);
void OpenCompanyWindow();
void FollowNews(const NewsReference &ref);
void ScrollToTile(TileIndex tile);

#endif /* MINI_WINDOWS_WINDOW_LINKS_H */
