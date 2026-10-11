/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file shore_relief.cpp The ground along shores curving smoothly from tile to tile instead of in saw teeth: bare coasts through the corners around them, river beds along the water's outline. */

#include "../../stdafx.h"
#include "shore_relief.h"

#include <algorithm>
#include <cmath>

#include "../../map_func.h"
#include "../map/tile_shapes.h"
#include "../map/world_tiles.h"
#include "water_outline.h"

#include "../../safeguards.h"

static constexpr double SHORE_STRAIGHTENING_REACH = 0.5;
static constexpr double CHANNEL_STRAIGHTENING_REACH = 0.2;
/* A river's bed starts falling away this far outside the water's outline, in the water field's own measure. */
static constexpr double CHANNEL_LIP = 0.03;
static constexpr double RAPIDS_DEPTH = 0.3;
static constexpr int BANK_PROBES_PER_SIDE = 4;

bool IsBareCoast(int tx, int ty)
{
	if (!OnMap(tx, ty) || WaterFormOf(tx, ty) != WaterForm::Shore) return false;
	TileIndex tile = TileXY(tx, ty);
	GroundTexel ground = _world_tiles.GroundAt(tile);
	NetworkTexel network = _world_tiles.NetworkAt(tile);
	bool spanned = (network.style & NETWORK_BRIDGE_BIT) != 0;
	return ground.material == GroundMaterial::Shore && ground.flora == 0 && network.track == 0 && network.road == 0 && network.tram == 0 && !spanned;
}

bool LiesAlongShore(const Seabed &bed, int tx, int ty)
{
	return IsBareCoast(tx, ty) || bed.Shallow(tx, ty);
}

ShoreRelief::ShoreRelief(const Seabed &bed, int tx, int ty, const std::array<double, 4> &corners, const BankSite &banks) :
	TileRelief(tx, ty, corners, SHORE_STRAIGHTENING_REACH), bed(bed), banks(banks)
{
}

Vec3 ShoreRelief::Normal(double x, double y, const Vec3 &faceted) const
{
	MapVector climb = this->LevelClimb(x, y);
	return Normalised(Lerp(this->BentNormal(climb, x, y, faceted), ClimbNormal(climb), this->Freedom(x, y)));
}

bool ShoreRelief::Banked() const
{
	if (!this->banked.has_value()) this->banked = !this->banks.Empty() && this->LiftsAnywhere();
	return *this->banked;
}

bool ShoreRelief::LiftsAnywhere() const
{
	for (int j = 0; j <= BANK_PROBES_PER_SIDE; j++) {
		for (int i = 0; i <= BANK_PROBES_PER_SIDE; i++) {
			double x = this->tx + static_cast<double>(i) / BANK_PROBES_PER_SIDE;
			double y = this->ty + static_cast<double>(j) / BANK_PROBES_PER_SIDE;
			if (this->Lift(x, y, this->Ground(x, y)) > 0.0) return true;
		}
	}
	return false;
}

/* The spline runs through every corner of the bed, so it agrees with the tile's own corners wherever it is free to show. */
double ShoreRelief::Curved(double x, double y) const
{
	LatticeCell cell = CellOf(2.0 * x, 2.0 * y);
	return Bicubic([this](int hx, int hy) { return this->bed.Level(hx, hy); }, cell, CatmullRomWeights(cell.u), CatmullRomWeights(cell.v));
}

ChannelRelief::ChannelRelief(const Seabed &bed, int tx, int ty, const std::array<double, 4> &corners) :
	TileRelief(tx, ty, corners, CHANNEL_STRAIGHTENING_REACH), bed(bed), ground(tx, ty), sheet(tx, ty)
{
}

/* Whether a riverside neighbour meets this tile's ground and water without a step, so the two curve as one. */
bool ChannelRelief::Curves(int nx, int ny) const
{
	if (!IsRiverside(nx, ny)) return false;
	TileGround neighbour_ground(nx, ny);
	WaterSheet neighbour_sheet(nx, ny);
	for (int hy = 2 * std::max(ny, this->ty); hy <= 2 * (std::min(ny, this->ty) + 1); hy++) {
		for (int hx = 2 * std::max(nx, this->tx); hx <= 2 * (std::min(nx, this->tx) + 1); hx++) {
			double x = hx * HALF_TILE;
			double y = hy * HALF_TILE;
			if (neighbour_ground.Level(x, y) != this->ground.Level(x, y) || neighbour_sheet.Level(x, y) != this->sheet.Level(x, y)) return false;
		}
	}
	return true;
}

double ChannelRelief::Curved(double x, double y) const
{
	double level = this->sheet.Level(x, y);
	double inside = SmoothStep(WATERLINE - CHANNEL_LIP, WATERLINE + CHANNEL_FALL, SheetWater(x, y, level));
	double ground = this->ground.Level(x, y);
	return std::lerp(ground, std::min(ground, level - this->Depth(x, y, level)), inside);
}

/* How deep the water stands at a point once well inside its outline: the depth under the middles of the tiles around, spread between them. */
double ChannelRelief::Depth(double x, double y, double level) const
{
	LatticeCell cell = CellOf(x - HALF_TILE, y - HALF_TILE);
	return Bicubic([&](int cx, int cy) { return this->CentreDepth(cx, cy, level); }, cell, BsplineWeights(cell.u), BsplineWeights(cell.v));
}

double ChannelRelief::CentreDepth(int cx, int cy, double level) const
{
	return this->SunkUnder(cx, cy) * Joining(cx, cy, level);
}

double ChannelRelief::SunkUnder(int cx, int cy) const
{
	if (IsRapids(cx, cy)) return RAPIDS_DEPTH;
	if (WaterFormOf(cx, cy) != WaterForm::Open) return 0.0;
	return SurfaceLevelOf(cx, cy) - this->bed.Level(2 * cx + 1, 2 * cy + 1);
}
