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
#include <utility>

#include "../../map_func.h"
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

bool IsRollingLand(int tx, int ty)
{
	return IsMapTile(tx, ty) && IsOpenLand(tx, ty) && (_world_tiles.NetworkAt(TileXY(tx, ty)).style & NETWORK_BRIDGE_BIT) == 0 && !IsRiverBank(tx, ty);
}

Landform::Landform(const TileSpan &tiles) :
	rolling(tiles.tx0 - LANDFORM_MARGIN, tiles.ty0 - LANDFORM_MARGIN, tiles.tx1 - tiles.tx0 + 1 + 2 * LANDFORM_MARGIN, tiles.ty1 - tiles.ty0 + 1 + 2 * LANDFORM_MARGIN),
	corners(tiles.tx0 - LANDFORM_MARGIN, tiles.ty0 - LANDFORM_MARGIN, tiles.tx1 - tiles.tx0 + 2 + 2 * LANDFORM_MARGIN, tiles.ty1 - tiles.ty0 + 2 + 2 * LANDFORM_MARGIN),
	bed(tiles),
	banks(tiles),
	reliefs(tiles.tx0 - LANDFORM_MARGIN, tiles.ty0 - LANDFORM_MARGIN, tiles.tx1 - tiles.tx0 + 1 + 2 * LANDFORM_MARGIN, tiles.ty1 - tiles.ty0 + 1 + 2 * LANDFORM_MARGIN)
{
}

Landform::Landform(Landform &&other) noexcept : rolling(std::move(other.rolling)), corners(std::move(other.corners)), bed(std::move(other.bed)), banks(std::move(other.banks)), reliefs(other.reliefs.Emptied())
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

HillReliefPtr Landform::ReliefAt(int tx, int ty) const
{
	return this->reliefs.At(tx, ty, [this](int x, int y) -> HillReliefPtr { return this->Bends(x, y) ? std::make_shared<const HillRelief>(*this, x, y) : nullptr; });
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
	for (const CornerTile &meeting : TilesAtCorner(cx, cy)) {
		if (this->Rolls(meeting.tx, meeting.ty)) return meeting.Level();
	}
	return LowestCornerAt(cx, cy);
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

enum class GroundKind : uint8_t {
	Flat,
	Shore,
	Hill,
};

static GroundKind GroundKindAt(const Landform &land, int tx, int ty)
{
	if (LiesAlongShore(land.Bed(), tx, ty)) return GroundKind::Shore;
	return land.ReliefAt(tx, ty) != nullptr ? GroundKind::Hill : GroundKind::Flat;
}

bool LaidGround::RollsAt(const Landform &land, int tx, int ty)
{
	return GroundKindAt(land, tx, ty) == GroundKind::Hill;
}

LaidGround::LaidGround(const Landform &land, int tx, int ty) : tx(tx), ty(ty)
{
	switch (GroundKindAt(land, tx, ty)) {
		case GroundKind::Shore:
			this->relief.emplace<ShoreRelief>(land.Bed(), tx, ty, land.Bed().SettledCorners(tx, ty), land.BanksAround(tx, ty));
			break;
		case GroundKind::Hill:
			this->relief.emplace<HillReliefPtr>(land.ReliefAt(tx, ty));
			break;
		case GroundKind::Flat:
			break;
	}
}

double LaidGround::Level(double x, double y) const
{
	const TileRelief *relief = this->Relief();
	return relief != nullptr ? relief->Level(x, y) : BedLevel(this->tx, this->ty, x, y);
}

bool LaidGround::Banked() const
{
	const ShoreRelief *shore = std::get_if<ShoreRelief>(&this->relief);
	return shore != nullptr && shore->Banked();
}

const TileRelief *LaidGround::Relief() const
{
	if (const HillReliefPtr *hill = std::get_if<HillReliefPtr>(&this->relief); hill != nullptr) return hill->get();
	return std::get_if<ShoreRelief>(&this->relief);
}

std::unique_ptr<DrawnSurface> RolledSpan::Surface(int tx, int ty) const
{
	HillReliefPtr relief = this->land->ReliefAt(tx, ty);
	if (relief == nullptr) return std::make_unique<TileGround>(tx, ty);
	return std::make_unique<RolledSurface>(this->land, std::move(relief));
}
