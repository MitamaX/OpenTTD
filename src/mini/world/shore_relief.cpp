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
/* A river's bed starts falling away just outside the water's outline and reaches the water's full depth this far inside it, in the water field's own measure. */
static constexpr double CHANNEL_LIP = 0.03;
static constexpr double CHANNEL_FALL = 0.15;
static constexpr double SLOPE_PROBE = 0.1;
static constexpr int SPLINE_REACH = 1;

bool IsBareCoast(int tx, int ty)
{
	if (!OnMap(tx, ty) || WaterFormOf(tx, ty) != WaterForm::Shore) return false;
	TileIndex tile = TileXY(tx, ty);
	GroundTexel ground = _world_tiles.GroundAt(tile);
	NetworkTexel network = _world_tiles.NetworkAt(tile);
	bool spanned = (network.style & NETWORK_BRIDGE_BIT) != 0;
	return ground.material == GroundMaterial::Shore && ground.flora == 0 && network.track == 0 && network.road == 0 && network.tram == 0 && !spanned;
}

/* The cubic through p1 and p2 whose slopes there point from the points either side. */
static double CatmullRom(double p0, double p1, double p2, double p3, double t)
{
	return p1 + 0.5 * t * (p2 - p0 + t * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3 + t * (3.0 * (p1 - p2) + p3 - p0)));
}

/* How far a point lies from a tile's square. */
static double DistanceToTile(double x, double y, int tx, int ty)
{
	double dx = std::max({tx - x, 0.0, x - (tx + 1)});
	double dy = std::max({ty - y, 0.0, y - (ty + 1)});
	return std::hypot(dx, dy);
}

TileRelief::TileRelief(int tx, int ty, const std::array<double, 4> &corners, double straightening_reach) : tx(tx), ty(ty), corners(corners), straightening_reach(straightening_reach)
{
}

double TileRelief::Level(double x, double y) const
{
	return std::lerp(this->Faceted(x, y), this->Curved(x, y), this->Freedom(x, y));
}

Vec3 TileRelief::Normal(double x, double y) const
{
	double rise = LevelRise();
	double slope_x = (this->Level(x + SLOPE_PROBE, y) - this->Level(x - SLOPE_PROBE, y)) * rise / (2.0 * SLOPE_PROBE);
	double slope_y = (this->Level(x, y + SLOPE_PROBE) - this->Level(x, y - SLOPE_PROBE)) * rise / (2.0 * SLOPE_PROBE);
	return Normalised({-slope_x, -slope_y, 1.0});
}

/* The tile's corners spread over it, straight along each side as every neighbour's edge runs. Corners go north, west, east, south. */
double TileRelief::Faceted(double x, double y) const
{
	if (!this->held.has_value()) {
		std::array<double, 4> held;
		for (size_t i = 0; i < held.size(); i++) {
			double cx = this->tx + static_cast<double>(i % 2);
			double cy = this->ty + static_cast<double>(i / 2);
			held[i] = this->Pinned(cx, cy) ? this->corners[i] : this->Held(cx, cy, this->corners[i]);
		}
		this->held = held;
	}
	const std::array<double, 4> &corner = *this->held;
	double fx = x - this->tx;
	double fy = y - this->ty;
	double north = std::lerp(corner[0], corner[1], fx);
	double south = std::lerp(corner[2], corner[3], fx);
	return std::lerp(north, south, fy);
}

/* How far the ground may curve at a point: not at all on the edge of a tile that keeps its facets, and freely from the straightening reach away from every one. */
double TileRelief::Freedom(double x, double y) const
{
	int cx = static_cast<int>(std::floor(x));
	int cy = static_cast<int>(std::floor(y));
	double freedom = 1.0;
	for (int ny = cy - SPLINE_REACH; ny <= cy + SPLINE_REACH; ny++) {
		for (int nx = cx - SPLINE_REACH; nx <= cx + SPLINE_REACH; nx++) {
			if (this->Curving(nx, ny)) continue;
			freedom = std::min(freedom, SmoothStep(0.0, this->straightening_reach, DistanceToTile(x, y, nx, ny)));
		}
	}
	return freedom;
}

/* Whether a tile near this one curves, asked of each tile once. */
bool TileRelief::Curving(int nx, int ny) const
{
	if (!this->curving.has_value()) {
		std::array<bool, CURVING_SIDE * CURVING_SIDE> curving;
		for (int j = 0; j < CURVING_SIDE; j++) {
			for (int i = 0; i < CURVING_SIDE; i++) curving[j * CURVING_SIDE + i] = this->Curves(this->tx + i - CURVING_REACH, this->ty + j - CURVING_REACH);
		}
		this->curving = curving;
	}
	return (*this->curving)[(ny - this->ty + CURVING_REACH) * CURVING_SIDE + nx - this->tx + CURVING_REACH];
}

ShoreRelief::ShoreRelief(const Seabed &bed, int tx, int ty, const std::array<double, 4> &corners) : TileRelief(tx, ty, corners, SHORE_STRAIGHTENING_REACH), bed(bed)
{
}

/* The spline runs through every corner of the bed, so it agrees with the tile's own corners wherever it is free to show. */
double ShoreRelief::Curved(double x, double y) const
{
	int cx = static_cast<int>(std::floor(x));
	int cy = static_cast<int>(std::floor(y));
	std::array<double, 4> rows;
	for (int j = 0; j < 4; j++) {
		auto at = [&](int i) { return this->bed.Level(2 * (cx + i - 1), 2 * (cy + j - 1)); };
		rows[j] = CatmullRom(at(0), at(1), at(2), at(3), x - cx);
	}
	return CatmullRom(rows[0], rows[1], rows[2], rows[3], y - cy);
}

ChannelRelief::ChannelRelief(const Seabed &bed, int tx, int ty, const std::array<double, 4> &corners) :
	TileRelief(tx, ty, corners, CHANNEL_STRAIGHTENING_REACH), bed(bed), ground(tx, ty), surface(SurfaceLevelOf(tx, ty))
{
}

/* Only riverside ground at the river's level meeting this tile's own without a step curves with it, so the two follow the same outline down to the same bed. */
bool ChannelRelief::Curves(int nx, int ny) const
{
	if (!IsRiverside(nx, ny) || SurfaceLevelOf(nx, ny) != this->surface) return false;
	TileGround other(nx, ny);
	for (int cy = std::max(ny, this->ty); cy <= std::min(ny, this->ty) + 1; cy++) {
		for (int cx = std::max(nx, this->tx); cx <= std::min(nx, this->tx) + 1; cx++) {
			if (other.Level(cx, cy) != this->ground.Level(cx, cy)) return false;
		}
	}
	return true;
}

double ChannelRelief::Curved(double x, double y) const
{
	double inside = SmoothStep(WATERLINE - CHANNEL_LIP, WATERLINE + CHANNEL_FALL, SheetWater(x, y, this->surface));
	return std::min(this->ground.Level(x, y), this->surface - this->Depth(x, y) * inside);
}

/* How deep the water stands at a point once well inside its outline: the depth under the middles of the tiles around, spread between them. */
double ChannelRelief::Depth(double x, double y) const
{
	double qx = x - HALF_TILE;
	double qy = y - HALF_TILE;
	int cx = static_cast<int>(std::floor(qx));
	int cy = static_cast<int>(std::floor(qy));
	double fx = qx - cx;
	double fy = qy - cy;
	double north = std::lerp(this->CentreDepth(cx, cy), this->CentreDepth(cx + 1, cy), fx);
	double south = std::lerp(this->CentreDepth(cx, cy + 1), this->CentreDepth(cx + 1, cy + 1), fx);
	return std::lerp(north, south, fy);
}

double ChannelRelief::CentreDepth(int cx, int cy) const
{
	if (WaterFormOf(cx, cy) != WaterForm::Open || SurfaceLevelOf(cx, cy) != this->surface) return 0.0;
	return this->surface - this->bed.Level(2 * cx + 1, 2 * cy + 1);
}
