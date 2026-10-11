/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rockfall.cpp Stones strewn over rocky ground, laid out block by block from the map's ground and shown only near the eye. */

#include "../../stdafx.h"
#include "rockfall.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "../../map_func.h"
#include "../../settings_type.h"
#include "../core/camera.h"
#include "../core/lattice_noise.h"
#include "../core/seed.h"
#include "../core/tones.h"
#include "../map/tile_shapes.h"
#include "bare_ground.h"
#include "hill_relief.h"
#include "rock_models.h"
#include "vehicle_models.h"

#include "../../safeguards.h"

static constexpr int SURVEY_MARGIN = 1;
static constexpr int STONE_CELLS = 3;
static constexpr double STONE_JITTER = 0.3;
static constexpr double ROCKY_STONES = 0.75;
static constexpr double GATHER_FLOOR = 0.15;
static constexpr SeedRange GATHER_EDGES = {0.3, 0.7};
static constexpr double ROCK_GATHERS_PER_TILE = 0.6;
static constexpr double MOST_CELL_SHARE = 0.45;
static constexpr SeedRange ROCK_WARMTH = {0.0, 0.6};
static constexpr SeedRange MOSS_SHARE = {0.35, 0.7};
static constexpr uint32_t ROCK_SALT = 0x2C9E51A7U;
static constexpr uint32_t GATHER_SALT = 0x6A1D0F3BU;
static constexpr uint32_t FAMILY_SALT = 0x0B5E77C1U;

struct RockPick {
	RockModel model;
	double weight;
};

static constexpr std::array<RockPick, 5> ROCKY_PICKS = {{
	{RockModel::Boulder, 0.3},
	{RockModel::Slab, 0.2},
	{RockModel::Split, 0.15},
	{RockModel::Block, 0.15},
	{RockModel::Rubble, 0.2},
}};

struct BareTile {
	double level;
	MapVector climb;
	GroundTexel ground;
	bool holds;
};

struct RockLook {
	uint32_t body;
	uint32_t cap;
};

static bool HoldsStones(int tx, int ty, const GroundTexel &ground)
{
	switch (ground.material) {
		case GroundMaterial::Grass:
		case GroundMaterial::Rough:
		case GroundMaterial::Rocks:
		case GroundMaterial::Snow:
		case GroundMaterial::Desert: return TreeCount(ground.flora) == 0 && IsRollingLand(tx, ty);
		default: return false;
	}
}

class RockSurvey {
public:
	explicit RockSurvey(const TileSpan &tiles) :
		land(tiles), tiles(tiles.tx0 - SURVEY_MARGIN, tiles.ty0 - SURVEY_MARGIN, tiles.tx1 - tiles.tx0 + 1 + 2 * SURVEY_MARGIN, tiles.ty1 - tiles.ty0 + 1 + 2 * SURVEY_MARGIN)
	{
	}

	const Landform &Land() const { return this->land; }
	BareTile At(int tx, int ty) const { return this->tiles.At(tx, ty, [this](int x, int y) { return this->Read(x, y); }); }

private:
	BareTile Read(int tx, int ty) const
	{
		if (!OnMap(tx, ty)) return {};
		GroundTexel ground = _world_tiles.GroundAt(TileXY(tx, ty));
		if (!HoldsStones(tx, ty, ground)) return {0.0, {}, ground, false};
		MapVector middle = {tx + 0.5, ty + 0.5};
		return {this->land.LevelAt(middle.x, middle.y), this->land.Climb(middle.x, middle.y), ground, true};
	}

	Landform land;
	LazyLattice<BareTile> tiles;
};

static double AnyBearing(SeedDice &dice)
{
	return dice.Between(0.0, 2.0 * std::numbers::pi);
}

template <size_t N>
static RockModel Picked(const std::array<RockPick, N> &picks, double share)
{
	for (const RockPick &pick : picks) {
		if (share < pick.weight) return pick.model;
		share -= pick.weight;
	}
	return picks.back().model;
}

static double Gathering(const MapVector &at)
{
	return std::lerp(GATHER_FLOOR, 1.0, SmoothStep(GATHER_EDGES.low, GATHER_EDGES.high, LatticeNoise(at.x, at.y, ROCK_GATHERS_PER_TILE, GATHER_SALT)));
}

static uint MixShare(double share)
{
	return static_cast<uint>(std::lround(std::clamp(share, 0.0, 1.0) * CHANNEL_MAX));
}

static RockLook LookOf(uint32_t family, const BareTile &tile, SeedDice &dice)
{
	SeedDice kin(Hash32(FAMILY_SALT ^ family));
	uint32_t rock = Mix(ROCK_TONE, ROCK_WARM_TONE, MixShare(kin.Between(ROCK_WARMTH)));
	uint32_t body = ScaledRgb(rock, 1.0 + LAYER_VARIETY * (kin.Share() - 0.5));
	double moss = dice.Between(MOSS_SHARE) * (1.0 - AltitudeShare(tile.level));
	return {body, Mix(body, LITTER_MOSS_TONE, MixShare(moss))};
}

static VehicleInstance Seated(const LaidGround &ground, const BareTile &tile, const MapVector &at, double yaw, double radius, const RockFit &fit, const RockLook &look)
{
	MapVector grade = tile.climb * LevelRise();
	MapVector heading = {std::cos(yaw), std::sin(yaw)};
	double along = Dot(grade, heading);
	double across = Dot(grade, {-heading.y, heading.x});
	double sink = fit.reach * radius * Length(grade) * (1.0 - fit.follow);
	Vec3 place = {at.x, at.y, ground.Level(at) * LevelRise() - sink};
	return StrewnCopy(place, {yaw, std::atan(along * fit.follow), std::atan(across * fit.follow)}, radius, InstanceColour(look.body), InstanceColour(look.cap));
}

static MapVector Inset(const MapVector &at, int tx, int ty, double margin)
{
	return {std::clamp(at.x, tx + margin, tx + 1.0 - margin), std::clamp(at.y, ty + margin, ty + 1.0 - margin)};
}

static void StrewStones(const LaidGround &ground, const BareTile &tile, int tx, int ty, ScatterCopies &copies)
{
	uint32_t base = TileXY(tx, ty).base();
	SeedDice dice(Hash32(ROCK_SALT ^ base));
	RockLook look = LookOf(base, tile, dice);
	bool hemmed = !ClearAround(tx, ty, 1);
	for (int cell = 0; cell < STONE_CELLS * STONE_CELLS; cell++) {
		MapVector at = {tx + (cell % STONE_CELLS + 0.5 + dice.Between(-STONE_JITTER, STONE_JITTER)) / STONE_CELLS, ty + (cell / STONE_CELLS + 0.5 + dice.Between(-STONE_JITTER, STONE_JITTER)) / STONE_CELLS};
		double chance = dice.Share();
		RockModel model = Picked(ROCKY_PICKS, dice.Share());
		RockFit fit = FitOf(model);
		double radius = std::min(dice.Between(fit.radius), MOST_CELL_SHARE / STONE_CELLS);
		double yaw = AnyBearing(dice);
		if (chance >= ROCKY_STONES * Gathering(at)) continue;
		if (hemmed) at = Inset(at, tx, ty, fit.reach * radius);
		copies[RockModelIndex(model, RockFinish::Chiselled)].push_back(Seated(ground, tile, at, yaw, radius, fit, look));
	}
}

Rockfall::Rockfall() : BlockScatter(ROCK_MODELS, LANDFORM_REACH, OPEN_LAND_READS | ChangeKind::Flora | ChangeKind::Relief, ROCK_BLOCK_MARGIN)
{
}

void Rockfall::Strew(const TileSpan &tiles, ScatterCopies &copies) const
{
	if (_settings_game.game_creation.landscape == LandscapeType::Toyland) return;
	RockSurvey survey(tiles);
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			BareTile tile = survey.At(tx, ty);
			if (!tile.holds || tile.ground.material != GroundMaterial::Rocks) continue;
			StrewStones(LaidGround(survey.Land(), tx, ty), tile, tx, ty, copies);
		}
	}
}
