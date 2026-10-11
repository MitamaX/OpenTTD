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
#include <span>
#include <vector>

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
#include "scene_view.h"
#include "vehicle_models.h"

#include "../../safeguards.h"

static constexpr int TALUS_REACH = 2;
static constexpr int SURVEY_MARGIN = TALUS_REACH;
static constexpr int STONE_CELLS = 3;
static constexpr double STONE_JITTER = 0.3;
static constexpr double ROCKY_STONES = 0.75;
static constexpr double ROUGH_STONES = 0.08;
static constexpr double SCREE_STONES = 0.3;
static constexpr double OUTCROP_STONES = 0.4;
static constexpr double GATHER_FLOOR = 0.15;
static constexpr SeedRange GATHER_EDGES = {0.3, 0.7};
static constexpr double ROCK_GATHERS_PER_TILE = 0.6;
static constexpr double MOST_CELL_SHARE = 0.45;
static constexpr double SNOWLINE_CAP = 0.8;
static constexpr double SNOW_LINE_REACH = 1.0;
static constexpr double BLANKET_BURY = 0.35;
static constexpr SeedRange CRAG_SLOPES = {0.6, 1.2};
static constexpr double CRAG_LEAST_SHARE = 0.15;
static constexpr int CRAG_BED_STRIDE = 3;
static constexpr double CRAG_INSET = 0.12;
static constexpr double CRAG_RUNS_PER_TILE = 0.7;
static constexpr double CRAG_RUN_SHARE = 0.7;
static constexpr double BUTTRESS_SHARE = 0.35;
static constexpr SeedRange CRAG_GROWTH = {0.8, 1.2};
static constexpr double CRAG_YAW_JITTER = 0.25;
static constexpr double TALUS_DROP = 0.75;
static constexpr double TALUS_STONES = 0.8;
static constexpr double TALUS_LEAN = 0.4;
static constexpr double TALUS_GROWTH = 1.4;
static constexpr uint32_t CRAG_SALT = 0x4F1BD2E9U;
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

static constexpr std::array<RockPick, 4> SCREE_PICKS = {{
	{RockModel::Rubble, 0.45},
	{RockModel::Block, 0.25},
	{RockModel::Wedge, 0.2},
	{RockModel::Boulder, 0.1},
}};

static constexpr std::array<RockPick, 3> OUTCROP_PICKS = {{
	{RockModel::Block, 0.4},
	{RockModel::Slab, 0.3},
	{RockModel::Boulder, 0.3},
}};

static constexpr std::array<RockPick, 4> TALUS_PICKS = {{
	{RockModel::Block, 0.35},
	{RockModel::Rubble, 0.35},
	{RockModel::Boulder, 0.2},
	{RockModel::Split, 0.1},
}};

struct BareTile {
	double level;
	MapVector climb;
	GroundTexel ground;
	bool holds;
	double bare;
	double crag;

	bool Rocky() const { return this->ground.material == GroundMaterial::Rocks || (this->ground.detail & GROUND_ROCKY_BIT) != 0; }
};

struct RockClimate {
	SeedRange warmth;
	uint32_t cap;
	SeedRange cap_share;
};

static constexpr RockClimate TEMPERATE_ROCKS = {{0.0, 0.6}, LITTER_MOSS_TONE, {0.35, 0.7}};
static constexpr std::array<RockClimate, 4> CLIMATE_ROCKS = {{
	TEMPERATE_ROCKS,
	{{0.0, 0.4}, LITTER_MOSS_TONE, {0.2, 0.4}},
	{{0.6, 1.0}, DRY_SAND_TONE, {0.3, 0.6}},
	TEMPERATE_ROCKS,
}};

struct Talus {
	double share;
	MapVector uphill;
};

struct StoneRule {
	double chance;
	std::span<const RockPick> picks;
	bool layered;
	MapVector lean;
	double growth;
};

struct Crag {
	MapVector at;
	double yaw;
	double radius;
	RockModel model;
	int bed;
};

struct RockLook {
	uint32_t body;
	uint32_t cap;
};

static double MiddleLevel(const SurfaceTexel &surface)
{
	return (surface.CornerAt(0, 0) + surface.CornerAt(1, 0) + surface.CornerAt(0, 1) + surface.CornerAt(1, 1)) / 4.0;
}

static MapVector CornerClimb(const SurfaceTexel &surface)
{
	double far_x = surface.CornerAt(1, 0) + surface.CornerAt(1, 1);
	double far_y = surface.CornerAt(0, 1) + surface.CornerAt(1, 1);
	double near_x = surface.CornerAt(0, 0) + surface.CornerAt(0, 1);
	double near_y = surface.CornerAt(0, 0) + surface.CornerAt(1, 0);
	return {(far_x - near_x) / 2.0, (far_y - near_y) / 2.0};
}

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

static double CragShare(double bare, const MapVector &climb)
{
	double share = ExposedShare(bare, ROCK_EXPOSURE) * SmoothStep(CRAG_SLOPES.low, CRAG_SLOPES.high, Length(climb));
	return share < CRAG_LEAST_SHARE ? 0.0 : share;
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
		TileIndex index = TileXY(tx, ty);
		GroundTexel ground = _world_tiles.GroundAt(index);
		SurfaceTexel surface = _world_tiles.SurfaceAt(index);
		double level = MiddleLevel(surface);
		MapVector climb = CornerClimb(surface);
		double bare = BareShare(SteepShare(Length(climb)), AltitudeShare(level), BlanketDepth(ground));
		return {level, climb, ground, HoldsStones(tx, ty, ground), bare, CragShare(bare, climb)};
	}

	Landform land;
	LazyLattice<BareTile> tiles;
};

static RockModel Picked(std::span<const RockPick> picks, double share)
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

static double SnowShare(const BareTile &tile)
{
	if (_settings_game.game_creation.landscape != LandscapeType::Arctic) return 0.0;
	return std::max(BlanketDepth(tile.ground), SNOWLINE_CAP * SmoothStep(0.0, SNOW_LINE_REACH, tile.level - SnowLineLevel()));
}

static RockLook LookOf(uint32_t family, const BareTile &tile, SeedDice &dice)
{
	const RockClimate &climate = CLIMATE_ROCKS[to_underlying(_settings_game.game_creation.landscape) % CLIMATE_ROCKS.size()];
	SeedDice kin(Hash32(FAMILY_SALT ^ family));
	uint32_t rock = Mix(ROCK_TONE, ROCK_WARM_TONE, UnsignedByte(kin.Between(climate.warmth)));
	uint32_t body = ScaledRgb(rock, 1.0 + LAYER_VARIETY * (kin.Share() - 0.5));
	uint32_t cap = Mix(body, climate.cap, UnsignedByte(dice.Between(climate.cap_share) * (1.0 - AltitudeShare(tile.level))));
	return {body, Mix(cap, CAP_SNOW_TONE, UnsignedByte(SnowShare(tile)))};
}

static Talus TalusOf(const RockSurvey &survey, const BareTile &home, int tx, int ty)
{
	Talus talus = {0.0, {}};
	for (int dy = -TALUS_REACH; dy <= TALUS_REACH; dy++) {
		for (int dx = -TALUS_REACH; dx <= TALUS_REACH; dx++) {
			double distance = std::hypot(dx, dy);
			if (distance == 0.0 || distance > TALUS_REACH) continue;
			BareTile source = survey.At(tx + dx, ty + dy);
			if (source.crag <= 0.0 || source.level - home.level < TALUS_DROP) continue;
			double share = source.crag * (1.0 - distance / (TALUS_REACH + 1));
			if (share > talus.share) talus = {share, MapVector{static_cast<double>(dx), static_cast<double>(dy)} * (1.0 / distance)};
		}
	}
	talus.share *= 1.0 - SteepShare(Length(home.climb));
	return talus;
}

static StoneRule RuleOf(const BareTile &tile, const Talus &talus)
{
	bool rocky = tile.Rocky();
	bool rough = tile.ground.material == GroundMaterial::Rough;
	std::array<StoneRule, 5> rules = {{
		{rocky ? ROCKY_STONES : 0.0, ROCKY_PICKS, false, {}, 1.0},
		{rough ? ROUGH_STONES : 0.0, ROCKY_PICKS, false, {}, 1.0},
		{ExposedShare(tile.bare, SCREE_EXPOSURE) * SCREE_STONES, SCREE_PICKS, true, {}, 1.0},
		{ExposedShare(tile.bare, ROCK_EXPOSURE) * OUTCROP_STONES, OUTCROP_PICKS, true, {}, 1.0},
		{talus.share * TALUS_STONES, TALUS_PICKS, true, talus.uphill * (TALUS_LEAN * STONE_CELLS), std::lerp(1.0, TALUS_GROWTH, talus.share)},
	}};
	return *std::ranges::max_element(rules, {}, &StoneRule::chance);
}

static VehicleInstance Seated(const WorldPoint &seat, const BareTile &tile, double yaw, double radius, const RockFit &fit, const RockLook &look)
{
	MapVector grade = tile.climb * LevelRise();
	MapVector heading = {std::cos(yaw), std::sin(yaw)};
	double along = Dot(grade, heading);
	double across = Dot(grade, {-heading.y, heading.x});
	double sink = radius * (Length(MapVector{along * fit.reach.x, across * fit.reach.y}) * (1.0 - fit.follow) + BLANKET_BURY * BlanketDepth(tile.ground));
	Vec3 place = {seat.x, seat.y, seat.level * LevelRise() - sink};
	return StrewnCopy(place, {yaw, std::atan(along * fit.follow), std::atan(across * fit.follow)}, radius, InstanceColour(look.body), InstanceColour(look.cap));
}

static MapVector Inset(const MapVector &at, int tx, int ty, double margin)
{
	return {std::clamp(at.x, tx + margin, tx + 1.0 - margin), std::clamp(at.y, ty + margin, ty + 1.0 - margin)};
}

static bool InsideTile(const MapVector &at, int tx, int ty, double margin)
{
	return at.x >= tx + margin && at.x <= tx + 1.0 - margin && at.y >= ty + margin && at.y <= ty + 1.0 - margin;
}

static int FirstBedFrom(double bedding)
{
	return static_cast<int>(std::ceil(bedding / CRAG_BED_STRIDE)) * CRAG_BED_STRIDE;
}

static std::vector<Crag> CragsOf(const BareTile &tile, int tx, int ty, bool hemmed)
{
	std::vector<Crag> crags;
	double share = tile.crag;
	if (share <= 0.0) return crags;
	double slope = Length(tile.climb);
	MapVector rise = tile.climb * StrataPerLevel() + STRATA_DIP;
	MapVector middle = {tx + 0.5, ty + 0.5};
	double bedding = BeddingOf(middle, tile.level);
	double spread = (std::abs(rise.x) + std::abs(rise.y)) / 2.0;
	SeedDice dice(Hash32(CRAG_SALT ^ TileXY(tx, ty).base()));
	for (int bed = FirstBedFrom(bedding - spread); bed <= bedding + spread; bed += CRAG_BED_STRIDE) {
		MapVector at = middle + rise * ((bed - bedding) / Dot(rise, rise));
		RockModel model = dice.Share() < BUTTRESS_SHARE * SteepShare(slope) ? RockModel::Buttress : RockModel::Ledge;
		RockFit fit = FitOf(model);
		double radius = dice.Between(fit.radius) * std::lerp(CRAG_GROWTH.low, CRAG_GROWTH.high, share);
		double yaw = std::atan2(rise.x, -rise.y) + dice.Between(-CRAG_YAW_JITTER, CRAG_YAW_JITTER);
		if (!InsideTile(at, tx, ty, hemmed ? fit.Farthest() * radius : CRAG_INSET)) continue;
		if (LatticeNoise(at.x, at.y, CRAG_RUNS_PER_TILE, SubSeed(CRAG_SALT, static_cast<uint32_t>(bed))) >= share * CRAG_RUN_SHARE) continue;
		crags.push_back({at, yaw, radius, model, bed});
	}
	return crags;
}

static bool UnderCrag(const MapVector &at, std::span<const Crag> crags)
{
	return std::ranges::any_of(crags, [&](const Crag &crag) { return Length(at - crag.at) < FitOf(crag.model).Farthest() * crag.radius; });
}

static void RaiseCrags(const LaidGround &ground, const BareTile &tile, std::span<const Crag> crags, ScatterCopies &copies)
{
	for (const Crag &crag : crags) {
		SeedDice dice(Hash32(CRAG_SALT ^ static_cast<uint32_t>(crag.bed)));
		RockLook look = LookOf(static_cast<uint32_t>(crag.bed), tile, dice);
		copies[RockModelIndex(crag.model, RockFinish::Chiselled)].push_back(Seated(ground.At(crag.at), tile, crag.yaw, crag.radius, FitOf(crag.model), look));
	}
}

static void StrewStones(const LaidGround &ground, const BareTile &tile, const StoneRule &rule, std::span<const Crag> crags, int tx, int ty, bool hemmed, ScatterCopies &copies)
{
	uint32_t base = TileXY(tx, ty).base();
	SeedDice dice(Hash32(ROCK_SALT ^ base));
	MapVector middle = {tx + 0.5, ty + 0.5};
	for (int cell = 0; cell < STONE_CELLS * STONE_CELLS; cell++) {
		MapVector at = {tx + (cell % STONE_CELLS + 0.5 + dice.Between(-STONE_JITTER, STONE_JITTER)) / STONE_CELLS, ty + (cell / STONE_CELLS + 0.5 + dice.Between(-STONE_JITTER, STONE_JITTER)) / STONE_CELLS};
		double chance = dice.Share();
		RockModel model = Picked(rule.picks, dice.Share());
		RockFit fit = FitOf(model);
		double radius = std::min(dice.Between(fit.radius), MOST_CELL_SHARE / STONE_CELLS) * rule.growth;
		double yaw = dice.Bearing();
		double leaning = 1.0 + Dot(at - middle, rule.lean);
		if (chance >= rule.chance * leaning * Gathering(at) || UnderCrag(at, crags)) continue;
		if (hemmed) at = Inset(at, tx, ty, fit.Farthest() * radius);
		WorldPoint seat = ground.At(at);
		uint32_t family = rule.layered ? static_cast<uint32_t>(static_cast<int>(std::floor(BeddingOf(at, seat.level)))) : base;
		copies[RockModelIndex(model, RockFinish::Chiselled)].push_back(Seated(seat, tile, yaw, radius, fit, LookOf(family, tile, dice)));
	}
}

Rockfall::Rockfall() : BlockScatter(ROCK_MODELS, LANDFORM_REACH + TALUS_REACH, OPEN_LAND_READS | ChangeKind::Flora | ChangeKind::Relief, ROCK_BLOCK_MARGIN)
{
}

void Rockfall::Strew(const TileSpan &tiles, ScatterCopies &copies) const
{
	if (_settings_game.game_creation.landscape == LandscapeType::Toyland) return;
	RockSurvey survey(tiles);
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			BareTile tile = survey.At(tx, ty);
			if (!tile.holds) continue;
			StoneRule rule = RuleOf(tile, TalusOf(survey, tile, tx, ty));
			if (rule.chance <= 0.0 && tile.crag <= 0.0) continue;
			bool hemmed = !ClearAround(tx, ty, 1);
			std::vector<Crag> crags = CragsOf(tile, tx, ty, hemmed);
			LaidGround ground(survey.Land(), tx, ty);
			RaiseCrags(ground, tile, crags, copies);
			StrewStones(ground, tile, rule, crags, tx, ty, hemmed, copies);
		}
	}
}
