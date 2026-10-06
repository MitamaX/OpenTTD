/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file industry_forms.cpp Industry tiles as building forms: seeds, joins with neighbouring tiles, liveries and construction. */

#include "../../stdafx.h"
#include "industry_forms.h"

#include "../../industry.h"
#include "../../industry_map.h"
#include "../../industrytype.h"
#include "../../settings_type.h"
#include "../../tile_map.h"
#include "../core/tones.h"
#include "industry_looks.h"
#include "site_shapes.h"

#include "../../safeguards.h"

static constexpr std::array<float, INDUSTRY_COMPLETED> CONSTRUCTION_RISE = {0.3f, 0.55f, 0.8f};
static constexpr float COMPLETED_RISE = 1.0f;
static constexpr uint MAP_COLOUR_SHARE = 72;
static constexpr uint CARGO_SHARE = 140;
static constexpr uint TOY_TRIM_SHARE = 96;
static constexpr uint JITTER_FIRST = 26;

/* Shared by every tile of one industry, so its joined roofs keep one colour. */
static uint32_t IndustrySeed(TileIndex tile)
{
	return Hash32(GetIndustryIndex(tile).base());
}

static uint32_t TileSeed(TileIndex tile)
{
	return Hash32(tile.base() ^ IndustrySeed(tile));
}

static SiteLook LookOf(TileIndex tile)
{
	return IndustryTileLook(tile, TileSeed(tile));
}

static DiagDirections JoinedParts(TileIndex tile, SiteShape shape)
{
	IndustryID industry = GetIndustryIndex(tile);
	return JoinedSides(tile, [industry, shape](TileIndex next) {
		return IsTileType(next, MP_INDUSTRY) && GetIndustryIndex(next) == industry && LookOf(next).shape == shape;
	});
}

static SiteLot IndustryLot(TileIndex tile, SiteShape shape)
{
	uint8_t stage = GetIndustryConstructionStage(tile);
	bool building = stage < INDUSTRY_COMPLETED;
	return {tile, TileSeed(tile), JoinedParts(tile, shape), building ? CONSTRUCTION_RISE[stage] : COMPLETED_RISE, building};
}

static uint32_t BulkTint(const IndustrySpec &spec)
{
	CargoType cargo = spec.IsProcessingIndustry() ? spec.accepts_cargo[0] : spec.produced_cargo[0];
	return IsValidCargoType(cargo) ? Mix(BULK_TINT, CargoRgb(cargo), CARGO_SHARE) : BULK_TINT;
}

static SiteLivery IndustryLivery(TileIndex tile, const SiteLook &look)
{
	const Industry *industry = Industry::GetByTile(tile);
	const IndustrySpec *spec = GetIndustrySpec(industry->type);
	uint32_t seed = IndustrySeed(tile);
	SiteLivery livery = LiveryFor(look.finish, SiteRoof(look), seed, _company_rgb[industry->random_colour]);
	livery.roof = Mix(livery.roof, PaletteRgb(spec->map_colour), MAP_COLOUR_SHARE);
	livery.bulk = BulkTint(*spec);
	if (_settings_game.game_creation.landscape == LandscapeType::Toyland) {
		livery.wall = Mix(COL_PAPER, livery.trim, TOY_TRIM_SHARE);
		livery.roof = livery.trim;
	}
	livery.wall = TintJitter(livery.wall, seed, JITTER_FIRST);
	livery.roof = TintJitter(livery.roof, seed, JITTER_FIRST);
	return livery;
}

std::optional<BuildingForm> IndustryForm(TileIndex tile)
{
	SiteLook look = LookOf(tile);
	BuildingForm form = SiteForm(tile);
	BuildSite(form, IndustryLot(tile, look.shape), look, IndustryLivery(tile, look));
	if (form.Solids().empty()) return std::nullopt;
	return form;
}

std::optional<FloraPatch> IndustryFlora(TileIndex tile)
{
	SiteLook look = LookOf(tile);
	if (look.shape != SiteShape::Grove) return std::nullopt;
	return GroveFlora(look);
}
