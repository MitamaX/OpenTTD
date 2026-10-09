/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file water_sheet.cpp The level a tile's sheet of water stands at over every point of the tile. */

#include "../../stdafx.h"
#include "water_sheet.h"

#include "../../map_func.h"
#include "seabed.h"

#include "../../safeguards.h"

WaterSheet::WaterSheet(int tx, int ty) : tx(tx), ty(ty), surface(_world_tiles.SurfaceAt(TileXY(tx, ty))), low(SurfaceLevelOf(tx, ty)), shape(ShapeOf(tx, ty))
{
}

WaterSheet::Shape WaterSheet::ShapeOf(int tx, int ty)
{
	switch (WaterFormOf(tx, ty)) {
		case WaterForm::Open:
		case WaterForm::Shore: return Shape::Level;
		default: return Shape::Plane;
	}
}

double WaterSheet::Level(double x, double y) const
{
	return this->shape == Shape::Plane ? FacetLevel(this->surface, x - this->tx, y - this->ty) : this->low;
}

int WaterSheet::Divisions() const
{
	return 1;
}
