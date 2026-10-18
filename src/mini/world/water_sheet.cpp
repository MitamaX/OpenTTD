/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file water_sheet.cpp The level a tile's sheet of water stands at over every point of the tile. */

#include "../../stdafx.h"
#include "water_sheet.h"

#include <algorithm>

#include "../../map_func.h"
#include "../map/tile_shapes.h"
#include "seabed.h"

#include "../../safeguards.h"

static bool LiesLevel(int tx, int ty)
{
	WaterForm form = WaterFormOf(tx, ty);
	return form == WaterForm::Open || form == WaterForm::Shore;
}

static std::optional<TileIndex> RapidsFollowed(int tx, int ty)
{
	if (IsRapids(tx, ty)) return TileXY(tx, ty);
	std::optional<TileIndex> river = RiverOfBank(tx, ty);
	if (river.has_value() && IsRapids(TileX(*river), TileY(*river))) return river;
	return FallBesideCoast(tx, ty);
}

static double TopLevelOf(int tx, int ty)
{
	return _world_tiles.SurfaceAt(TileXY(tx, ty)).Highest();
}

static double FallShare(double u, double foot, double lip)
{
	if (u < 0.0) return u * foot;
	if (u > 1.0) return 1.0 + (u - 1.0) * lip;
	double square = u * u;
	double cube = square * u;
	return 3.0 * square - 2.0 * cube + (cube - 2.0 * square + u) * foot + (cube - square) * lip;
}

WaterSheet::WaterSheet(int tx, int ty) : tx(tx), ty(ty), surface(_world_tiles.SurfaceAt(TileXY(tx, ty))), low(SurfaceLevelOf(tx, ty))
{
	std::optional<TileIndex> rapids = RapidsFollowed(tx, ty);
	if (rapids.has_value()) this->fall = FallEndsOf(TileX(*rapids), TileY(*rapids));
	this->lies_level = !this->fall.has_value() && LiesLevel(tx, ty);
}

WaterSheet::FallEnds WaterSheet::FallEndsOf(int tx, int ty)
{
	TileIndexDiffC down = FallStep(tx, ty);
	int below_x = tx + down.x;
	int below_y = ty + down.y;
	int above_x = tx - down.x;
	int above_y = ty - down.y;
	bool runs_on_below = IsRapids(below_x, below_y) && TopLevelOf(below_x, below_y) == SurfaceLevelOf(tx, ty);
	bool runs_on_above = IsRapids(above_x, above_y) && SurfaceLevelOf(above_x, above_y) == TopLevelOf(tx, ty);
	return {runs_on_below ? 1.0 : 0.0, runs_on_above ? 1.0 : 0.0};
}

double WaterSheet::Level(double x, double y) const
{
	if (this->lies_level) return this->low;
	double plane = FacetLevel(this->surface, x - this->tx, y - this->ty);
	if (!this->fall.has_value()) return plane;
	return this->low + FallShare(plane - this->low, this->fall->foot, this->fall->lip);
}

int WaterSheet::Divisions() const
{
	bool beside_fall = AnySideTile(this->tx, this->ty, [](int nx, int ny) { return RapidsFollowed(nx, ny).has_value(); });
	return this->fall.has_value() || beside_fall ? CHANNEL_DIVISIONS : 1;
}
