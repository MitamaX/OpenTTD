/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file farmsteads.cpp The countryside's own life away from the towns: farmsteads in their meadows, hay bales on the stubble, fences and stone walls along the pastures. */

#include "../../stdafx.h"
#include "farmsteads.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#include "../../map_func.h"
#include "../../settings_type.h"
#include "../core/seed.h"
#include "../map/tile_shapes.h"

#include "../../safeguards.h"

static constexpr int TOWN_DISTANCE = 4;
static constexpr int BUILDING_DISTANCE = 2;
static constexpr int YARD_CLEARANCE = 1;
static constexpr int FARMSTEAD_SPACING = 2;
static constexpr int FIELD_NEIGHBOURHOOD = 2;
static constexpr uint32_t FARMSTEAD_BY_FIELDS_ODDS = 5;
static constexpr uint32_t LONE_FARMSTEAD_ODDS = 24;
static constexpr uint32_t BALED_FIELD_ODDS = 3;
static constexpr uint32_t BOUNDARY_ODDS = 5;
static constexpr int BOUNDARY_RUN_TILES = 4;
static constexpr double STONE_SHARE = 0.6;
static constexpr int MOST_BALES = 7;
static constexpr int YARD_BALES = 4;
static constexpr double YARD_REACH = 0.32;
static constexpr double BALE_ROW_GAP = 0.12;
static constexpr double SILO_SHARE = 0.5;
static constexpr double QUARTER_TURN = M_PI / 2.0;
static constexpr uint32_t FARMSTEAD_SALT = 0x46A3D1E5U;
static constexpr uint32_t BALE_SALT = 0x2BA1E5C7U;
static constexpr uint32_t BOUNDARY_SALT = 0x3D7A11F9U;

/* Crops ripe or cut to stubble, which bales are left on. */
static constexpr std::array<bool, 6> BALED_CROPS = {false, false, true, true, true, false};

static constexpr std::array<uint32_t, 4> HOUSE_WALLS = {0xE4DAC2, 0xEDEAE2, 0x9A5C45, 0xB3AA98};
static constexpr std::array<uint32_t, 3> HOUSE_ROOFS = {0xA04A30, 0x515862, 0x8F7B50};
static constexpr std::array<uint32_t, 3> BARN_WALLS = {0x8C3B2D, 0x6D5A45, 0x56603F};
static constexpr std::array<uint32_t, 2> BARN_ROOFS = {0x7B7F82, 0x45403A};
static constexpr std::array<uint32_t, 3> HAY = {0xC8AD5E, 0xB49950, 0xA68E57};
static constexpr std::array<uint32_t, 2> SILO_WALLS = {0xB9BDBF, 0x9AA6AC};

/* Where a piece stands in the frame of a farmstead's yard, which turns with the yard. */
struct YardSpot {
	FarmPiece piece;
	MapVector at;
};

static constexpr std::array<YardSpot, 3> YARD = {{
	{FarmPiece::House, {-0.17, 0.22}},
	{FarmPiece::Barn, {0.09, -0.17}},
	{FarmPiece::Silo, {0.36, 0.2}},
}};

template <size_t N>
static uint32_t Pick(const std::array<uint32_t, N> &tones, SeedDice &dice)
{
	return tones[dice.Below(N)];
}

template <typename Piece>
static void Place(ScatterCopies &copies, Piece piece, double x, double y, double turn, uint32_t primary, uint32_t secondary = 0)
{
	float z = static_cast<float>(GroundLevel(x, y) * LevelRise());
	copies[to_underlying(piece)].push_back({static_cast<float>(x), static_cast<float>(y), z, static_cast<float>(turn), 0.0f, 0.0f, 1.0f, InstanceColour(primary), InstanceColour(secondary), {}});
}

static bool IsFlat(const SurfaceTexel &surface)
{
	return surface.north == surface.west && surface.north == surface.east && surface.north == surface.south;
}

/* Paved streets near mark a town, which the countryside keeps its distance from, and it keeps a little clear of any other building too. */
static bool FarFromTowns(int tx, int ty)
{
	for (int dy = -TOWN_DISTANCE; dy <= TOWN_DISTANCE; dy++) {
		for (int dx = -TOWN_DISTANCE; dx <= TOWN_DISTANCE; dx++) {
			if (!OnMap(tx + dx, ty + dy)) continue;
			GroundTexel ground = _world_tiles.GroundAt(TileXY(tx + dx, ty + dy));
			bool near = std::max(std::abs(dx), std::abs(dy)) <= BUILDING_DISTANCE;
			if (ground.material == GroundMaterial::Paved || (near && (ground.detail & GROUND_BUILT_BIT) != 0)) return false;
		}
	}
	return true;
}

static bool NearFields(int tx, int ty)
{
	for (int dy = -FIELD_NEIGHBOURHOOD; dy <= FIELD_NEIGHBOURHOOD; dy++) {
		for (int dx = -FIELD_NEIGHBOURHOOD; dx <= FIELD_NEIGHBOURHOOD; dx++) {
			if (OnMap(tx + dx, ty + dy) && _world_tiles.GroundAt(TileXY(tx + dx, ty + dy)).material == GroundMaterial::Fields) return true;
		}
	}
	return false;
}

/* A tile left bare: no trees on it, no bridge over it, and nothing else either. */
static bool IsBare(int tx, int ty, const GroundTexel &ground)
{
	TileIndex tile = TileXY(tx, ty);
	return (ground.flora & FLORA_COUNT_MASK) == 0 && (_world_tiles.NetworkAt(tile).style & NETWORK_BRIDGE_BIT) == 0 && IsOpenLand(tx, ty);
}

static bool IsBareGrass(int tx, int ty)
{
	if (!OnMap(tx, ty)) return false;
	GroundTexel ground = _world_tiles.GroundAt(TileXY(tx, ty));
	return ground.material == GroundMaterial::Grass && IsBare(tx, ty, ground);
}

/* How a tile draws for a farmstead: a lower draw wins over the tiles about it; a tile that could not hold one draws nothing. */
static std::optional<uint32_t> FarmsteadDraw(int tx, int ty)
{
	if (!IsBareGrass(tx, ty)) return std::nullopt;
	TileIndex tile = TileXY(tx, ty);
	uint32_t draw = Hash32(FARMSTEAD_SALT ^ tile.base());
	uint32_t odds = NearFields(tx, ty) ? FARMSTEAD_BY_FIELDS_ODDS : LONE_FARMSTEAD_ODDS;
	if (draw % odds != 0 || !IsFlat(_world_tiles.SurfaceAt(tile)) || !ClearAround(tx, ty, YARD_CLEARANCE) || !FarFromTowns(tx, ty)) return std::nullopt;
	return draw;
}

/* A farmstead stands where its tile's draw beats every other tile's near enough for their yards to crowd it. */
static bool HoldsFarmstead(int tx, int ty)
{
	std::optional<uint32_t> own = FarmsteadDraw(tx, ty);
	if (!own.has_value()) return false;
	for (int dy = -FARMSTEAD_SPACING; dy <= FARMSTEAD_SPACING; dy++) {
		for (int dx = -FARMSTEAD_SPACING; dx <= FARMSTEAD_SPACING; dx++) {
			if (dx == 0 && dy == 0) continue;
			std::optional<uint32_t> other = FarmsteadDraw(tx + dx, ty + dy);
			if (other.has_value() && *other < *own) return false;
		}
	}
	return true;
}

/* A farmhouse, a barn, now and then a silo, and a few bales stacked in the yard, the yard turned a quarter at a time. */
static void Farmstead(ScatterCopies &copies, int tx, int ty, SeedDice &dice)
{
	double turn = dice.Below(4) * QUARTER_TURN;
	double cos_turn = std::cos(turn);
	double sin_turn = std::sin(turn);
	MapVector middle = {tx + HALF_TILE, ty + HALF_TILE};
	auto placed = [&](const MapVector &at) { return middle + MapVector{at.x * cos_turn - at.y * sin_turn, at.x * sin_turn + at.y * cos_turn}; };

	bool silo = dice.Share() < SILO_SHARE;
	uint32_t hay = Pick(HAY, dice);
	for (const YardSpot &spot : YARD) {
		if (spot.piece == FarmPiece::Silo && !silo) continue;
		MapVector at = placed(spot.at);
		switch (spot.piece) {
			case FarmPiece::House: Place(copies, spot.piece, at.x, at.y, turn, Pick(HOUSE_WALLS, dice), Pick(HOUSE_ROOFS, dice)); break;
			case FarmPiece::Barn: Place(copies, spot.piece, at.x, at.y, turn, Pick(BARN_WALLS, dice), Pick(BARN_ROOFS, dice)); break;
			default: Place(copies, spot.piece, at.x, at.y, turn, Pick(SILO_WALLS, dice)); break;
		}
	}
	for (int bale = 0; bale < YARD_BALES; bale++) {
		MapVector at = placed({dice.Between(-YARD_REACH, -0.2), dice.Between(-YARD_REACH, YARD_REACH)});
		Place(copies, bale % 2 == 0 ? FarmPiece::RoundBale : FarmPiece::BaleStack, at.x, at.y, dice.Between(0.0, 2.0 * M_PI), hay);
	}
}

/* Round bales left lying along the rows of a cut field. */
static void Bales(ScatterCopies &copies, int tx, int ty, const GroundTexel &field, SeedDice &dice)
{
	bool rows_along_y = (field.variant & 1) == 0;
	uint32_t hay = Pick(HAY, dice);
	int count = 2 + static_cast<int>(dice.Below(MOST_BALES - 1));
	double row = dice.Between(0.15, 0.85);
	for (int bale = 0; bale < count; bale++) {
		double along = dice.Share();
		double across = row + (bale % 2) * BALE_ROW_GAP;
		double x = tx + (rows_along_y ? across : along);
		double y = ty + (rows_along_y ? along : across);
		Place(copies, FarmPiece::RoundBale, x, y, (rows_along_y ? QUARTER_TURN : 0.0) + dice.Between(-0.3, 0.3), hay);
	}
}

/* Whether a run of boundary stands along a line of tile sides, a few tiles at a time, and whether it is laid in stone; the same along the whole run. */
static std::optional<bool> BoundaryRun(int line, int along, Axis axis)
{
	SeedDice dice(Hash32(BOUNDARY_SALT ^ Hash32(static_cast<uint32_t>(line) * 2 + to_underlying(axis)) ^ static_cast<uint32_t>(along / BOUNDARY_RUN_TILES)));
	if (dice.Below(BOUNDARY_ODDS) != 0) return std::nullopt;
	return _settings_game.game_creation.landscape != LandscapeType::Tropic && dice.Share() < STONE_SHARE;
}

/* Dry stone walls and fences run along the level sides of pastures, a few tiles at a stretch, where open land lies on both sides. */
static void Boundaries(ScatterCopies &copies, int tx, int ty, const SurfaceTexel &surface)
{
	for (Axis axis : {AXIS_X, AXIS_Y}) {
		bool along_x = axis == AXIS_X;
		bool level = along_x ? surface.north == surface.west : surface.north == surface.east;
		if (!level || !IsOpenLand(along_x ? tx : tx - 1, along_x ? ty - 1 : ty)) continue;
		std::optional<bool> stone = along_x ? BoundaryRun(ty, tx, axis) : BoundaryRun(tx, ty, axis);
		if (!stone.has_value()) continue;
		double x = along_x ? tx + HALF_TILE : static_cast<double>(tx);
		double y = along_x ? static_cast<double>(ty) : ty + HALF_TILE;
		Place(copies, *stone ? BoundaryPiece::StoneWall : BoundaryPiece::Fence, x, y, along_x ? 0.0 : QUARTER_TURN, 0);
	}
}

Farmsteads::Farmsteads() : BlockScatter(FARM_PIECES, TOWN_DISTANCE + FARMSTEAD_SPACING)
{
}

void Farmsteads::Strew(const TileSpan &tiles, ScatterCopies &copies) const
{
	if (_settings_game.game_creation.landscape == LandscapeType::Toyland) return;
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			TileIndex tile = TileXY(tx, ty);
			GroundTexel ground = _world_tiles.GroundAt(tile);
			bool grass = ground.material == GroundMaterial::Grass;
			bool field = ground.material == GroundMaterial::Fields;
			if ((!grass && !field) || !IsBare(tx, ty, ground)) continue;

			SeedDice dice(Hash32(FARMSTEAD_SALT ^ tile.base()));
			if (field) {
				if (BALED_CROPS[ground.variant % BALED_CROPS.size()] && dice.Below(BALED_FIELD_ODDS) == 0 && FarFromTowns(tx, ty)) {
					SeedDice bales(Hash32(BALE_SALT ^ tile.base()));
					Bales(copies, tx, ty, ground, bales);
				}
				continue;
			}
			if (HoldsFarmstead(tx, ty)) Farmstead(copies, tx, ty, dice);
		}
	}
}

FieldBoundaries::FieldBoundaries() : BlockScatter(BOUNDARY_PIECES, TOWN_DISTANCE + FARMSTEAD_SPACING)
{
}

void FieldBoundaries::Strew(const TileSpan &tiles, ScatterCopies &copies) const
{
	if (_settings_game.game_creation.landscape == LandscapeType::Toyland) return;
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			if (IsBareGrass(tx, ty) && !HoldsFarmstead(tx, ty) && FarFromTowns(tx, ty)) Boundaries(copies, tx, ty, _world_tiles.SurfaceAt(TileXY(tx, ty)));
		}
	}
}
