/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tile_pick.cpp The map tile a build tool points at. */

#include "../../stdafx.h"
#include "tile_pick.h"

#include "../../core/math_func.hpp"
#include "../../gfx_func.h"
#include "../../map_func.h"

#include "../../safeguards.h"

static TileIndex ClampedTileAt(TilePoint at, int low, int edge)
{
	int tx = Clamp<int>(static_cast<int>(std::floor(at.first)), low, Map::SizeX() - edge);
	int ty = Clamp<int>(static_cast<int>(std::floor(at.second)), low, Map::SizeY() - edge);
	return TileXY(tx, ty);
}

TilePoint CursorPoint()
{
	return _camera.MapAt(_cursor.pos.x, _cursor.pos.y);
}

std::optional<TileIndex> TileUnder(TilePoint at)
{
	int tx = static_cast<int>(std::floor(at.first));
	int ty = static_cast<int>(std::floor(at.second));
	if (tx < 0 || ty < 0 || tx >= static_cast<int>(Map::SizeX()) || ty >= static_cast<int>(Map::SizeY())) return std::nullopt;
	return TileXY(tx, ty);
}

TileIndex WholeTileAt(TilePoint at)
{
	return ClampedTileAt(at, 0, 1);
}

TileIndex PathTileAt(TilePoint at)
{
	return ClampedTileAt(at, 0, 2);
}

TileIndex SiteTileAt(TilePoint at)
{
	return ClampedTileAt(at, 1, 2);
}
