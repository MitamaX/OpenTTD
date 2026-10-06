/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file house_kinds.h What a town house is to the mini map: its kind, storeys, era, seed and the colour the game paints it. */

#ifndef MINI_MAP_HOUSE_KINDS_H
#define MINI_MAP_HOUSE_KINDS_H

#include <optional>

#include "../../house_type.h"
#include "../../tile_type.h"
#include "../core/seed.h"
#include "building_form.h"

enum class HouseKind : uint8_t {
	Cottage, House, Terrace, Flats, Shops, Office, Tower, GlassTower, Hotel,
	Church, Theatre, Cinema, Mall, Warehouse, Stadium, Park, Statue, Fountain, Igloo, Tepee, Teapot, PiggyBank, End,
};

enum class Era : uint8_t { Traditional, Interwar, Modern, End };

struct KindRule {
	uint8_t pps;
	uint8_t min;
	uint8_t max;
	WindowGrid grid;
	HouseKind next;
};

struct SeedField {
	uint first;
	uint count;

	constexpr uint32_t Of(uint32_t seed) const { return SeedBits(seed, this->first, this->count); }
	constexpr float ShareOf(uint32_t seed) const { return SeedShare(seed, this->first, this->count); }
};

inline constexpr SeedField WALL_MATERIAL_SLOT = {0, 2};
inline constexpr SeedField WALL_COLOUR = {2, 4};
inline constexpr SeedField ROOF_MATERIAL_SLOT = {6, 2};
inline constexpr SeedField ROOF_COLOUR = {8, 2};
inline constexpr SeedField ROOF_SHAPE_SLOT = {10, 2};
inline constexpr SeedField RIDGE_FALLBACK = {12, 1};
inline constexpr SeedField PLAN_VARIANT = {13, 2};
inline constexpr SeedField STOREY_VARIATION = {15, 2};
inline constexpr SeedField SETBACK_JITTER = {17, 3};
inline constexpr SeedField CHIMNEY = {20, 1};
inline constexpr uint TINT_JITTER_FIRST = 26;
inline constexpr SeedField END_CHOICE = {30, 1};
inline constexpr SeedField SPIRE_MATERIAL = {31, 1};
/* Accent colours share bits with the end and spire choices; no kind reads both. */
inline constexpr SeedField ACCENT_COLOUR = {30, 2};
inline constexpr uint32_t GLASS_SALT = 5;

struct RemapTint {
	uint32_t argb;
	std::optional<Material> wall;
};

HouseID VisualModel(HouseID id);
HouseKind KindOfHouse(HouseID north_id, double pop_per_tile);
std::optional<KindRule> RuleOf(HouseKind kind);
int StoreysOf(const KindRule &rule, double pop_per_tile, uint32_t seed);
Era EraOf(const HouseSpec &spec);
uint32_t HouseSeed(TileIndex north, HouseID north_id);
std::optional<RemapTint> GameRemapTint(TileIndex north, HouseID north_id);

#endif /* MINI_MAP_HOUSE_KINDS_H */
