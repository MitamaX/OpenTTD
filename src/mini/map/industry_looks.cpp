/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file industry_looks.cpp The look of each original industry tile, and of NewGRF tiles by the tile they stand in for. */

#include "../../stdafx.h"
#include "industry_looks.h"

#include <algorithm>

#include "../../industry.h"
#include "../../industry_map.h"
#include "../../industrytype.h"

#include "../../safeguards.h"

static constexpr std::array COAL_MINE = {
	HeadframeLook(4.0f), HeadframeLook(4.0f), ShedLook(2.5f), BlockLook(2.0f, Finish::Brick), HeapLook(), HeapLook(), HeapLook(),
};
static constexpr std::array POWER_STATION = {
	BlockLook(4.0f, Finish::Concrete), BandedStackLook(9.0f), ShedLook(2.5f), PlantLook(),
};
static constexpr std::array SAWMILL = {
	ShedLook(2.5f), ShedLook(2.5f), BlockLook(2.0f, Finish::Brick), StacksLook(Pile::Logs), StacksLook(Pile::Logs),
};
static constexpr std::array FOREST = {
	GroveLook(TreeKind::Conifer), FelledGroveLook(TreeKind::Conifer),
};
static constexpr std::array OIL_REFINERY = {
	TanksLook(2.5f, TankCount::Four, Finish::Painted), ColumnLook(7.5f), StackLook(10.0f), ColumnLook(6.0f, ColumnKind::Pair),
	BlockLook(4.0f, Finish::Concrete), BlockLook(1.5f, Finish::Concrete),
};
static constexpr std::array OIL_RIG = {
	DeckLook(DeckModule::Bare), DeckLook(DeckModule::Crane), DeckLook(DeckModule::Derrick), DeckLook(DeckModule::Quarters), DeckLook(DeckModule::Tanks),
};
static constexpr std::array OIL_WELLS = {
	PumpjackLook(), PumpjackLook(), PumpjackLook(), PumpjackLook(),
};
static constexpr std::array FARM = {
	ShedLook(1.5f, Finish::Tile), ShedLook(2.25f, Finish::Painted), ShedLook(2.25f, Finish::Painted), ShedLook(1.25f), SilosLook(4.0f, 2), StacksLook(Pile::Bales),
};
static constexpr std::array FACTORY = {
	SawtoothLook(), SawtoothLook(), SawtoothLook(), SawtoothLook(),
};
static constexpr std::array PRINTING_WORKS = {
	BlockLook(3.0f, Finish::Brick), BlockLook(3.0f, Finish::Brick), BlockLook(3.0f, Finish::Brick), BlockLook(3.0f, Finish::Brick),
};
static constexpr std::array COPPER_MINE = {
	HeadframeLook(6.0f), HeadframeLook(6.0f), StackLook(5.0f), BlockLook(2.0f, Finish::Concrete), HeapLook(),
};
static constexpr std::array STEEL_MILL = {
	ColumnLook(5.5f, ColumnKind::Furnace), ShedLook(3.0f), StackLook(7.0f), HeapLook(), ShedLook(3.0f), ShedLook(3.0f),
};
static constexpr std::array BANK = {
	BlockLook(3.0f, Finish::Stone), BlockLook(3.0f, Finish::Stone),
};
static constexpr std::array FOOD_PROCESSING_PLANT = {
	BlockLook(2.0f, Finish::Concrete), SilosLook(3.5f, 3), ShedLook(2.0f), TanksLook(2.0f, TankCount::Two, Finish::Metal),
};
static constexpr std::array PAPER_MILL = {
	BlockLook(3.0f, Finish::Brick), StackLook(7.0f), ShedLook(2.5f), StacksLook(Pile::Logs),
	TanksLook(2.0f, TankCount::Two), ColumnLook(4.0f), BlockLook(2.0f), ShedLook(2.5f),
};
static constexpr std::array GOLD_MINE = {
	BlockLook(2.0f), PitLook(), ShedLook(2.0f), ShedLook(2.0f), HeapLook(), HeapLook(), HeapLook(), HeadframeLook(4.5f),
	PitLook(), PitLook(), PitLook(), PitLook(), PitLook(), PitLook(), PitLook(), PitLook(), HeadframeLook(4.5f),
};
static constexpr std::array DIAMOND_MINE = {
	HeadframeLook(4.0f), ShedLook(2.0f), BlockLook(2.0f), ShedLook(2.0f), PitLook(), PitLook(), BlockLook(1.5f), PitLook(), PitLook(),
};
static constexpr std::array IRON_ORE_MINE = {
	ShedLook(2.0f), HeapLook(), HeapLook(), BlockLook(1.5f), PitLook(), PitLook(), PitLook(), PitLook(),
	PitLook(), PitLook(), PitLook(), PitLook(), HeapLook(), HeapLook(), HeapLook(), ShedLook(2.0f),
};
static constexpr std::array FRUIT_PLANTATION = {
	GroveLook(TreeKind::Broadleaf),
};
static constexpr std::array RUBBER_PLANTATION = {
	GroveLook(TreeKind::Jungle),
};
static constexpr std::array WATER_SUPPLY = {
	TanksLook(2.5f, TankCount::One, Finish::Painted), BlockLook(1.5f),
};
static constexpr std::array WATER_TOWER = {
	WaterTowerLook(),
};
static constexpr std::array LUMBER_MILL = {
	ShedLook(2.5f), StacksLook(Pile::Logs), StacksLook(Pile::Logs), BlockLook(1.5f),
};
static constexpr std::array TOY_GROVE = {
	GroveLook(TreeKind::Toy), FelledGroveLook(TreeKind::Toy),
};
static constexpr std::array SWEET_FACTORY = {
	TanksLook(2.0f, TankCount::Two), BlockLook(3.0f), BlockLook(3.0f), BlockLook(3.0f),
};
static constexpr std::array COLA_WELLS = {
	PumpjackLook(),
};
static constexpr std::array TOY_SHOP = {
	StacksLook(Pile::Crates), BlockLook(2.5f), BlockLook(2.5f), BlockLook(2.5f),
};
static constexpr std::array TOY_FACTORY = {
	SawtoothLook(), SawtoothLook(), SawtoothLook(), SawtoothLook(), SawtoothLook(), StacksLook(Pile::Crates),
};
static constexpr std::array PLASTIC_FOUNTAINS = {
	FountainLook(), FountainLook(), FountainLook(), FountainLook(), FountainLook(), FountainLook(), FountainLook(), FountainLook(),
};
static constexpr std::array FIZZY_DRINK_FACTORY = {
	TanksLook(2.0f, TankCount::Four), BlockLook(2.5f), BlockLook(2.5f), BlockLook(2.5f),
};
static constexpr std::array BUBBLE_GENERATOR = {
	ColumnLook(3.0f), ColumnLook(3.0f), ShedLook(2.0f), PitLook(),
};
static constexpr std::array TOFFEE_QUARRY = {
	PitLook(), ShedLook(1.5f), HeapLook(),
};
static constexpr std::array SUGAR_MINE = {
	HeapLook(), HeapLook(), PitLook(), PitLook(), PitLook(), ShedLook(2.0f), BlockLook(2.0f), ColumnLook(3.0f),
};

template <size_t... N>
static constexpr std::array<SiteLook, (N + ...)> Concatenated(const std::array<SiteLook, N> &... industries)
{
	std::array<SiteLook, (N + ...)> looks{};
	auto next = looks.begin();
	((next = std::ranges::copy(industries, next).out), ...);
	return looks;
}

static constexpr auto ORIGINAL_TILE_LOOKS = Concatenated(
	COAL_MINE, POWER_STATION, SAWMILL, FOREST, OIL_REFINERY, OIL_RIG, OIL_WELLS, FARM, FACTORY, PRINTING_WORKS,
	COPPER_MINE, STEEL_MILL, BANK, FOOD_PROCESSING_PLANT, PAPER_MILL, GOLD_MINE, BANK, DIAMOND_MINE, IRON_ORE_MINE,
	FRUIT_PLANTATION, RUBBER_PLANTATION, WATER_SUPPLY, WATER_TOWER, FACTORY, LUMBER_MILL, TOY_GROVE, SWEET_FACTORY,
	TOY_GROVE, COLA_WELLS, TOY_SHOP, TOY_FACTORY, PLASTIC_FOUNTAINS, FIZZY_DRINK_FACTORY, BUBBLE_GENERATOR, TOFFEE_QUARRY, SUGAR_MINE);
static_assert(ORIGINAL_TILE_LOOKS.size() == NEW_INDUSTRYTILEOFFSET);

static constexpr uint32_t FALLBACK_SALT = 13;
static constexpr SiteLook RIG_ORIGIN = DeckLook(DeckModule::Derrick);
static constexpr SiteLook RIG_DECK = DeckLook(DeckModule::Bare);
static constexpr SiteLook MINE_ORIGIN = HeadframeLook(4.0f);
static constexpr std::array MINE_YARD = {ShedLook(2.0f), HeapLook(), HeapLook(), HeapLook()};
static constexpr std::array FARM_YARD = {ShedLook(2.25f), SilosLook(3.5f, 3)};
static constexpr SiteLook WORKS_ORIGIN = StackLook(7.0f);
static constexpr SiteLook WORKS_HALL = SawtoothLook();
static constexpr SiteLook SINK_BLOCK = BlockLook(3.0f);

/* A tile no original stands in for takes a look from what its industry does; the industry's north tile carries the landmark. */
static SiteLook SpecFallback(TileIndex tile, uint32_t seed)
{
	const Industry *industry = Industry::GetByTile(tile);
	const IndustrySpec *spec = GetIndustrySpec(industry->type);
	bool origin = tile == industry->location.tile;
	uint32_t pick = SubSeed(seed, FALLBACK_SALT);
	if (spec->behaviour.Test(IndustryBehaviour::BuiltOnWater)) return origin ? RIG_ORIGIN : RIG_DECK;
	if (spec->life_type.Test(IndustryLifeType::Extractive)) return origin ? MINE_ORIGIN : SeedPick(MINE_YARD, pick);
	if (spec->life_type.Test(IndustryLifeType::Organic)) return SeedPick(FARM_YARD, pick);
	if (spec->life_type.Test(IndustryLifeType::Processing)) return origin ? WORKS_ORIGIN : WORKS_HALL;
	return SINK_BLOCK;
}

SiteLook IndustryTileLook(TileIndex tile, uint32_t seed)
{
	IndustryGfx gfx = GetIndustryGfx(tile);
	if (gfx >= NEW_INDUSTRYTILEOFFSET) gfx = GetIndustryTileSpec(gfx)->grf_prop.subst_id;
	if (gfx < NEW_INDUSTRYTILEOFFSET) return ORIGINAL_TILE_LOOKS[gfx];
	return SpecFallback(tile, seed);
}
