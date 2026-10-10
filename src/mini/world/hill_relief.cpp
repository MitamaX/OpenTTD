/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file hill_relief.cpp The ground of the land as its meshes and everything standing on it read it. */

#include "../../stdafx.h"
#include "hill_relief.h"

#include <cmath>

#include "../map/tile_shapes.h"
#include "seabed.h"
#include "tile_relief.h"

#include "../../safeguards.h"

Landform::Landform(const TileSpan &tiles) : tiles(tiles)
{
}

double Landform::LevelAt(double x, double y) const
{
	int tx = static_cast<int>(std::floor(x));
	int ty = static_cast<int>(std::floor(y));
	return OnMap(tx, ty) ? LaidGround(*this, tx, ty).Level(x, y) : GroundLevel(x, y);
}

MapVector Landform::Climb(double x, double y) const
{
	return ClimbOf([this](double px, double py) { return this->LevelAt(px, py); }, x, y);
}

LaidGround::LaidGround(const Landform &land, int tx, int ty) : land(land), tx(tx), ty(ty)
{
}

double LaidGround::Level(double x, double y) const
{
	return BedLevel(this->tx, this->ty, x, y);
}
