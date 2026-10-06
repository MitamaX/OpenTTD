/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file house_kinds.cpp Sorting town houses into kinds, storeys and eras, and reading the colour the game paints them. */

#include "../../stdafx.h"
#include "house_kinds.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "../../house.h"
#include "../../map_func.h"
#include "../../sprite.h"
#include "../../tile_map.h"
#include "../../town.h"
#include "../../table/sprites.h"
#include "../../table/strings.h"

#include "../../safeguards.h"

static constexpr size_t RULED_KINDS = to_underlying(HouseKind::Hotel) + 1;
static constexpr double PROMOTION_MARGIN = 0.5;
static constexpr int MIN_VARIED_STOREY_RANGE = 3;
static constexpr std::array<int, 4> STOREY_VARIATIONS = {-1, 0, 0, 1};
static constexpr TimerGameCalendar::Year INTERWAR_START{1930};
static constexpr TimerGameCalendar::Year MODERN_START{1960};
static constexpr uint TOWN_DRAW_HOUSE_SHIFT = 4;
static constexpr uint TOWN_DRAW_VIEW_SHIFT = 2;

static constexpr std::array<KindRule, RULED_KINDS> KIND_RULES = {{
	{12, 1, 2, WindowGrid::Cottage, HouseKind::House},
	{9, 1, 2, WindowGrid::Cottage, HouseKind::Terrace},
	{12, 2, 3, WindowGrid::Terrace, HouseKind::Flats},
	{16, 2, 8, WindowGrid::Apartment, HouseKind::Office},
	{20, 2, 8, WindowGrid::Shopfront, HouseKind::Office},
	{16, 3, 12, WindowGrid::Office, HouseKind::Tower},
	{14, 8, 16, WindowGrid::Office, HouseKind::End},
	{9, 10, 18, WindowGrid::Curtain, HouseKind::End},
	{10, 4, 10, WindowGrid::Apartment, HouseKind::Tower},
}};

struct PaletteTint {
	PaletteID pal;
	uint32_t argb;
};

static constexpr std::array<PaletteTint, 19> PALETTE_TINTS = {{
	{PALETTE_TO_STRUCT_WHITE, 0xFFE4E1D8U},
	{PALETTE_TO_STRUCT_CONCRETE, 0xFFA9A69EU},
	{PALETTE_TO_STRUCT_BROWN, 0xFF8A6A50U},
	{PALETTE_TO_STRUCT_RED, 0xFFA4503CU},
	{PALETTE_TO_STRUCT_BLUE, 0xFF5E7FA0U},
	{PALETTE_TO_RED, 0xFFB5463CU},
	{PALETTE_TO_BLUE, 0xFF4E78B8U},
	{PALETTE_TO_ORANGE, 0xFFD98A3DU},
	{PALETTE_TO_GREEN, 0xFF5B9A55U},
	{PALETTE_TO_DARK_GREEN, 0xFF3F6A4CU},
	{PALETTE_TO_GREY, 0xFF8E9296U},
	{PALETTE_TO_CREAM, 0xFFE6D8B4U},
	{PALETTE_TO_BROWN, 0xFF7A563EU},
	{PALETTE_TO_PINK, 0xFFE3A1B4U},
	{PALETTE_TO_YELLOW, 0xFFE2C65AU},
	{PALETTE_TO_PALE_GREEN, 0xFFA9D49AU},
	{PALETTE_TO_MAUVE, 0xFFB497B8U},
	{PALETTE_CHURCH_RED, 0xFFA5523FU},
	{PALETTE_CHURCH_CREAM, 0xFFDCCFAEU},
}};

static HouseKind KindOfName(StringID name)
{
	switch (name) {
		case STR_TOWN_BUILDING_NAME_COTTAGES_1:
		case STR_TOWN_BUILDING_NAME_OLD_HOUSES_1: return HouseKind::Cottage;
		case STR_TOWN_BUILDING_NAME_TOWN_HOUSES_1: return HouseKind::Terrace;
		case STR_TOWN_BUILDING_NAME_SMALL_BLOCK_OF_FLATS_1:
		case STR_TOWN_BUILDING_NAME_FLATS_1: return HouseKind::Flats;
		case STR_TOWN_BUILDING_NAME_SHOPS_AND_OFFICES_1:
		case STR_TOWN_BUILDING_NAME_SHOPS_AND_OFFICES_2:
		case STR_TOWN_BUILDING_NAME_SHOPS_AND_OFFICES_3: return HouseKind::Shops;
		case STR_TOWN_BUILDING_NAME_OFFICE_BLOCK_1:
		case STR_TOWN_BUILDING_NAME_OFFICE_BLOCK_2:
		case STR_TOWN_BUILDING_NAME_OFFICE_BLOCK_3:
		case STR_TOWN_BUILDING_NAME_OFFICES_1: return HouseKind::Office;
		case STR_TOWN_BUILDING_NAME_TALL_OFFICE_BLOCK_1:
		case STR_TOWN_BUILDING_NAME_TALL_OFFICE_BLOCK_2:
		case STR_TOWN_BUILDING_NAME_LARGE_OFFICE_BLOCK_1: return HouseKind::Tower;
		case STR_TOWN_BUILDING_NAME_MODERN_OFFICE_BUILDING_1: return HouseKind::GlassTower;
		case STR_TOWN_BUILDING_NAME_HOTEL_1: return HouseKind::Hotel;
		case STR_TOWN_BUILDING_NAME_CHURCH_1: return HouseKind::Church;
		case STR_TOWN_BUILDING_NAME_THEATER_1: return HouseKind::Theatre;
		case STR_TOWN_BUILDING_NAME_CINEMA_1: return HouseKind::Cinema;
		case STR_TOWN_BUILDING_NAME_SHOPPING_MALL_1: return HouseKind::Mall;
		case STR_TOWN_BUILDING_NAME_WAREHOUSE_1: return HouseKind::Warehouse;
		case STR_TOWN_BUILDING_NAME_STADIUM_1:
		case STR_TOWN_BUILDING_NAME_STADIUM_2: return HouseKind::Stadium;
		case STR_TOWN_BUILDING_NAME_PARK_1: return HouseKind::Park;
		case STR_TOWN_BUILDING_NAME_STATUE_1: return HouseKind::Statue;
		case STR_TOWN_BUILDING_NAME_FOUNTAIN_1: return HouseKind::Fountain;
		case STR_TOWN_BUILDING_NAME_IGLOO_1: return HouseKind::Igloo;
		case STR_TOWN_BUILDING_NAME_TEPEES_1: return HouseKind::Tepee;
		case STR_TOWN_BUILDING_NAME_TEAPOT_HOUSE_1: return HouseKind::Teapot;
		case STR_TOWN_BUILDING_NAME_PIGGY_BANK_1: return HouseKind::PiggyBank;
		default: return HouseKind::House;
	}
}

static bool Outgrows(const KindRule &rule, double pop_per_tile)
{
	return rule.next != HouseKind::End && pop_per_tile > rule.pps * (rule.max + PROMOTION_MARGIN);
}

static HouseKind Promoted(HouseKind kind, double pop_per_tile)
{
	for (std::optional<KindRule> rule = RuleOf(kind); rule.has_value() && Outgrows(*rule, pop_per_tile); rule = RuleOf(kind)) {
		kind = rule->next;
	}
	return kind;
}

HouseID VisualModel(HouseID id)
{
	return id < NEW_HOUSE_OFFSET ? id : HouseSpec::Get(id)->grf_prop.subst_id;
}

HouseKind KindOfHouse(HouseID north_id, double pop_per_tile)
{
	BuildingFlags flags = HouseSpec::Get(north_id)->building_flags;
	if (flags.Test(BuildingFlag::IsStadium)) return HouseKind::Stadium;
	if (flags.Test(BuildingFlag::IsChurch)) return HouseKind::Church;
	return Promoted(KindOfName(HouseSpec::Get(VisualModel(north_id))->building_name), pop_per_tile);
}

std::optional<KindRule> RuleOf(HouseKind kind)
{
	if (to_underlying(kind) >= KIND_RULES.size()) return std::nullopt;
	return KIND_RULES[to_underlying(kind)];
}

int StoreysOf(const KindRule &rule, double pop_per_tile, uint32_t seed)
{
	int variation = rule.max - rule.min >= MIN_VARIED_STOREY_RANGE ? SeedPick(STOREY_VARIATIONS, STOREY_VARIATION.Of(seed)) : 0;
	return std::clamp<int>(static_cast<int>(std::lround(pop_per_tile / rule.pps)) + variation, rule.min, rule.max);
}

Era EraOf(const HouseSpec &spec)
{
	if (spec.min_year < INTERWAR_START) return Era::Traditional;
	if (spec.min_year < MODERN_START) return Era::Interwar;
	return Era::Modern;
}

uint32_t HouseSeed(TileIndex north, HouseID north_id)
{
	return Hash32(north.base() ^ Hash32(north_id));
}

std::optional<RemapTint> GameRemapTint(TileIndex north, HouseID north_id)
{
	if (north_id >= NEW_HOUSE_OFFSET && HouseSpec::Get(north_id)->grf_prop.HasSpriteGroups()) return std::nullopt;

	uint view = TileHash2Bit(TileX(north) * TILE_SIZE, TileY(north) * TILE_SIZE);
	size_t entry = VisualModel(north_id) << TOWN_DRAW_HOUSE_SHIFT | view << TOWN_DRAW_VIEW_SHIFT | TOWN_HOUSE_COMPLETED;
	PaletteID pal = GetTownDrawTileData()[entry].building.pal;

	auto found = std::ranges::find(PALETTE_TINTS, pal, &PaletteTint::pal);
	if (found == PALETTE_TINTS.end()) return std::nullopt;

	std::optional<Material> wall;
	if (pal == PALETTE_CHURCH_RED) wall = Material::Brick;
	return RemapTint{found->argb, wall};
}
