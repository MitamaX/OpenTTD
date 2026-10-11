/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file hill_relief.cpp The ground of the land as its meshes and everything standing on it read it. */

#include "../../stdafx.h"
#include "hill_relief.h"

#include <algorithm>
#include <cmath>

#include "../../map_func.h"
#include "../../tile_map.h"
#include "../core/lattice_noise.h"
#include "../map/tile_shapes.h"
#include "../map/world_tiles.h"
#include "block_scatter.h"
#include "seabed.h"

#include "../../safeguards.h"

static constexpr int LANDFORM_MARGIN = 4;
static constexpr double HILL_STRAIGHTENING_REACH = 1.0;
static constexpr double CREST_FULL_BEND = 0.5;
static constexpr double CREST_WAVES_PER_TILE = 0.4;
static constexpr uint32_t CREST_SALT = 0x6C2E57A1U;

static bool IsMapTile(int tx, int ty)
{
	return OnMap(tx, ty) && !IsTileType(TileXY(tx, ty), MP_VOID);
}

static bool IsRollingLand(int tx, int ty)
{
	return IsMapTile(tx, ty) && IsOpenLand(tx, ty) && (_world_tiles.NetworkAt(TileXY(tx, ty)).style & NETWORK_BRIDGE_BIT) == 0 && !IsRiverBank(tx, ty);
}

Landform::Landform(const TileSpan &tiles) :
	rolling(tiles.tx0 - LANDFORM_MARGIN, tiles.ty0 - LANDFORM_MARGIN, tiles.tx1 - tiles.tx0 + 1 + 2 * LANDFORM_MARGIN, tiles.ty1 - tiles.ty0 + 1 + 2 * LANDFORM_MARGIN),
	corners(tiles.tx0 - LANDFORM_MARGIN, tiles.ty0 - LANDFORM_MARGIN, tiles.tx1 - tiles.tx0 + 2 + 2 * LANDFORM_MARGIN, tiles.ty1 - tiles.ty0 + 2 + 2 * LANDFORM_MARGIN)
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

bool Landform::Rolls(int tx, int ty) const
{
	return this->rolling.At(tx, ty, IsRollingLand);
}

bool Landform::RollsRound(int cx, int cy) const
{
	return this->Rolls(cx - 1, cy - 1) && this->Rolls(cx, cy - 1) && this->Rolls(cx - 1, cy) && this->Rolls(cx, cy);
}

bool Landform::Bends(int tx, int ty) const
{
	return this->Rolls(tx, ty) && !this->Planar(tx, ty);
}

std::array<double, 4> Landform::Corners(int tx, int ty) const
{
	return {this->Corner(tx, ty), this->Corner(tx + 1, ty), this->Corner(tx, ty + 1), this->Corner(tx + 1, ty + 1)};
}

static double CrestWave(double x, double y, double bend)
{
	return CREST_WAVE * SmoothStep(0.0, CREST_FULL_BEND, bend) * (2.0 * LatticeNoise(x, y, CREST_WAVES_PER_TILE, CREST_SALT) - 1.0);
}

double Landform::Rolled(double x, double y) const
{
	LatticeCell cell = CellOf(x, y);
	CornerPatch patch = PatchAbout([this](int cx, int cy) { return this->Corner(cx, cy); }, cell);
	CubicWeights across = BsplineWeights(cell.u);
	CubicWeights down = BsplineWeights(cell.v);
	double bend = std::abs(Weighed(patch, BsplineCurvatures(cell.u), down)) + std::abs(Weighed(patch, across, BsplineCurvatures(cell.v)));
	return Weighed(patch, across, down) + CrestWave(x, y, bend);
}

Vec3 Landform::RolledNormal(double x, double y) const
{
	return ClimbNormal(ClimbOf([this](double px, double py) { return this->Rolled(px, py); }, x, y));
}

double Landform::Corner(int cx, int cy) const
{
	return this->corners.At(cx, cy, [this](int x, int y) { return this->MeetingCorner(x, y); });
}

double Landform::MeetingCorner(int cx, int cy) const
{
	std::optional<double> lowest;
	for (int dy = 0; dy <= 1; dy++) {
		for (int dx = 0; dx <= 1; dx++) {
			int tx = cx - 1 + dx;
			int ty = cy - 1 + dy;
			if (!IsMapTile(tx, ty)) continue;
			double level = _world_tiles.SurfaceAt(TileXY(tx, ty)).CornerAt(1 - dx, 1 - dy);
			if (this->Rolls(tx, ty)) return level;
			lowest = std::min(lowest.value_or(level), level);
		}
	}
	return lowest.value_or(GroundLevel(cx, cy));
}

bool Landform::Planar(int tx, int ty) const
{
	int first_x = tx - ROLL_SPREAD;
	int first_y = ty - ROLL_SPREAD;
	double origin = this->Corner(first_x, first_y);
	double across = this->Corner(first_x + 1, first_y) - origin;
	double down = this->Corner(first_x, first_y + 1) - origin;
	for (int cy = first_y; cy <= ty + 1 + ROLL_SPREAD; cy++) {
		for (int cx = first_x; cx <= tx + 1 + ROLL_SPREAD; cx++) {
			if (this->Corner(cx, cy) != origin + (cx - first_x) * across + (cy - first_y) * down) return false;
		}
	}
	return true;
}

HillRelief::HillRelief(const Landform &land, int tx, int ty) : TileRelief(tx, ty, land.Corners(tx, ty), HILL_STRAIGHTENING_REACH), land(land)
{
}

Vec3 HillRelief::Normal(double x, double y, const Vec3 &faceted) const
{
	return Normalised(Lerp(faceted, this->land.RolledNormal(x, y), this->Freedom(x, y)));
}

LaidGround::LaidGround(const Landform &land, int tx, int ty) : tx(tx), ty(ty)
{
	if (land.Bends(tx, ty)) this->relief.emplace(land, tx, ty);
}

double LaidGround::Level(double x, double y) const
{
	return this->relief.has_value() ? this->relief->Level(x, y) : BedLevel(this->tx, this->ty, x, y);
}

std::unique_ptr<DrawnSurface> RolledSpan::Surface(int tx, int ty) const
{
	if (!this->land->Bends(tx, ty)) return std::make_unique<TileGround>(tx, ty);
	return std::make_unique<RolledSurface>(this->land, tx, ty);
}
