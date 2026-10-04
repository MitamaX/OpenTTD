/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tile_pick.h The map tile a build tool points at. */

#ifndef MINI_TOOLS_TILE_PICK_H
#define MINI_TOOLS_TILE_PICK_H

#include <optional>

#include "../../tile_type.h"
#include "../core/camera.h"

TilePoint CursorPoint();
std::optional<TileIndex> TileUnder(TilePoint at);
TileIndex WholeTileAt(TilePoint at);
TileIndex PathTileAt(TilePoint at);
TileIndex SiteTileAt(TilePoint at);

#endif /* MINI_TOOLS_TILE_PICK_H */
