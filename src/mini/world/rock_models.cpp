/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rock_models.cpp Stones lying on open land: boulders, slabs, blocks, wedges, split boulders and heaps of rubble, hewn into facets and painted like a vehicle's paintwork. */

#include "../../stdafx.h"
#include "rock_models.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <numeric>
#include <ranges>

#include "../core/lattice_noise.h"
#include "../model/model_shapes.h"
#include "vehicle_models.h"

#include "../../safeguards.h"

static constexpr int STONE_SUBDIVISIONS = 1;
static constexpr int LUMP_SUBDIVISIONS = 0;
static constexpr SeedRange CUT_RISE = {-0.3, 1.1};
static constexpr double CAP_FACING = 0.6;
static constexpr double SKIRT_SHARE = 0.08;
static constexpr double ROCK_FOOT_SHADE = 0.55;
static constexpr double ROCK_TOP_SHADE = 1.0;
static constexpr double FACET_SPREAD = 0.04;
static constexpr double MOTTLE_SPREAD = 0.16;
static constexpr double MOTTLES_PER_UNIT = 1.6;
static constexpr MapVector MOTTLE_SHEAR = {0.6, -0.4};
static constexpr double ROCK_FOOT_OPENNESS = 0.55;
static constexpr double SPLIT_GAP = 0.08;
static constexpr int RUBBLE_LUMPS = 6;
static constexpr SeedRange RUBBLE_LUMP_RADIUS = {0.3, 0.45};
static constexpr int RUBBLE_LUMP_CUTS = 2;

struct StoneRecipe {
	Vec3 span;
	int cuts;
	SeedRange cut_reach;
	double top;
	double tilt;
	double rounding;
	double wobble;
	double bury;
	double follow;
	SeedRange radius;
	uint32_t seed;
};

static constexpr StoneRecipe BOULDER = {{1.0, 0.85, 0.72}, 5, {0.6, 0.85}, 1.0, 0.0, 0.25, 0.06, 0.38, 0.6, {0.05, 0.15}, 0xB0A1D3E5U};
static constexpr StoneRecipe SLAB = {{1.25, 0.9, 0.4}, 4, {0.7, 0.9}, 0.55, 0.0, 0.15, 0.04, 0.2, 0.9, {0.07, 0.16}, 0x51AB0F27U};
static constexpr StoneRecipe BLOCK = {{0.85, 0.8, 0.8}, 6, {0.55, 0.7}, 0.75, 0.0, 0.0, 0.05, 0.3, 0.6, {0.04, 0.1}, 0xB10C4A93U};
static constexpr StoneRecipe WEDGE = {{1.1, 0.75, 0.7}, 5, {0.55, 0.8}, 0.7, 0.45, 0.0, 0.05, 0.3, 0.5, {0.05, 0.12}, 0x3ED6E5C1U};
static constexpr StoneRecipe LEDGE = {{1.7, 0.8, 0.8}, 8, {0.45, 0.65}, 0.55, 0.2, 0.0, 0.07, 0.25, 0.5, {0.1, 0.17}, 0x1EDC3A57U};
static constexpr StoneRecipe BUTTRESS = {{1.1, 0.9, 1.1}, 7, {0.45, 0.65}, 0.65, 0.3, 0.0, 0.07, 0.25, 0.4, {0.1, 0.17}, 0xB077E55DU};

static constexpr StoneRecipe Recast(StoneRecipe recipe, SeedRange radius, double follow, uint32_t seed)
{
	recipe.radius = radius;
	recipe.follow = follow;
	recipe.seed = seed;
	return recipe;
}

static constexpr std::array<StoneRecipe, ROCK_MODELS> STONE_RECIPES = {{
	BOULDER,
	SLAB,
	BLOCK,
	WEDGE,
	Recast(BOULDER, {0.08, 0.15}, BOULDER.follow, 0x5B117A0DU),
	Recast(BOULDER, {0.06, 0.12}, 0.8, 0x7B1E5C3AU),
	LEDGE,
	BUTTRESS,
}};

static Vec3 Stretched(const Vec3 &at, const Vec3 &span)
{
	return {at.x * span.x, at.y * span.y, at.z * span.z};
}

static double ReachAlong(const Vec3 &span, const Vec3 &direction)
{
	return Length(Stretched(direction, span));
}

static void Cut(ModelMesh &stone, const Vec3 &normal, double reach)
{
	stone.Reshape([&](const Vec3 &at) {
		double beyond = Dot(at, normal) - reach;
		return beyond > 0.0 ? at - normal * beyond : at;
	});
}

static Vec3 Heading(double bearing, double rise)
{
	return {std::cos(bearing) * std::cos(rise), std::sin(bearing) * std::cos(rise), std::sin(rise)};
}

static void Cleave(ModelMesh &stone, const Vec3 &span, const SeedRange &reach, int cuts, SeedDice &dice)
{
	for (int cut = 0; cut < cuts; cut++) {
		Vec3 normal = Heading(dice.Bearing(), dice.Between(CUT_RISE));
		Cut(stone, normal, dice.Between(reach) * ReachAlong(span, normal));
	}
}

static ModelMesh Lump(int subdivisions, const Vec3 &span, const StoneRecipe &recipe, int cuts, SeedDice &dice)
{
	ModelMesh lump = Icosphere(subdivisions).Displace(recipe.wobble, dice.Next());
	lump.Reshape([&](const Vec3 &at) { return Stretched(at, span); });
	Cleave(lump, span, recipe.cut_reach, cuts, dice);
	return lump;
}

static ModelMesh Hewn(const StoneRecipe &recipe, SeedDice &dice)
{
	ModelMesh stone = Lump(STONE_SUBDIVISIONS, recipe.span, recipe, recipe.cuts, dice);
	if (recipe.top < 1.0) {
		Vec3 up = Heading(dice.Bearing(), std::numbers::pi / 2.0 - recipe.tilt);
		Cut(stone, up, recipe.top * ReachAlong(recipe.span, up));
	}
	return stone;
}

static ModelMesh Split(const StoneRecipe &recipe, SeedDice &dice)
{
	ModelMesh whole = Hewn(recipe, dice);
	Vec3 across = Heading(dice.Bearing(), 0.0);
	ModelMesh halves;
	for (double side : {-1.0, 1.0}) {
		ModelMesh half = whole;
		Cut(half, across * -side, 0.0);
		halves.Append(half.Move(across * (side * SPLIT_GAP / 2.0)));
	}
	return halves;
}

static ModelMesh Rubble(const StoneRecipe &recipe, SeedDice &dice)
{
	ModelMesh heap;
	for (int lump = 0; lump < RUBBLE_LUMPS; lump++) {
		double radius = dice.Between(RUBBLE_LUMP_RADIUS);
		Vec3 out = Heading(dice.Bearing(), 0.0) * dice.Between(0.0, 1.0 - radius);
		Vec3 span = Stretched({radius, radius, radius}, {1.0, 1.0, recipe.span.z});
		heap.Append(Lump(LUMP_SUBDIVISIONS, span, recipe, RUBBLE_LUMP_CUTS, dice).Move(out));
	}
	return heap;
}

static ModelMesh Shaped(RockModel model, const StoneRecipe &recipe)
{
	SeedDice dice(recipe.seed);
	switch (model) {
		case RockModel::Split: return Split(recipe, dice);
		case RockModel::Rubble: return Rubble(recipe, dice);
		default: return Hewn(recipe, dice);
	}
}

static std::pair<double, double> HeightsOf(const ModelMesh &stone)
{
	auto [low, high] = std::ranges::minmax(stone.vertices | std::views::transform(&ModelVertex::z));
	return {low, high};
}

static ModelMesh &Settled(ModelMesh &stone, double bury)
{
	auto [low, high] = HeightsOf(stone);
	return stone.Move({0.0, 0.0, -std::lerp(low, high, bury)});
}

static double Spread(double share, double spread)
{
	return 1.0 + spread * (2.0 * share - 1.0);
}

static ModelMesh &Chiselled(ModelMesh &stone, const StoneRecipe &recipe)
{
	auto [foot, top] = HeightsOf(stone);
	stone.FacetPlanes();
	if (recipe.rounding > 0.0) stone.Round({0.0, 0.0, std::midpoint(foot, top)}, recipe.rounding);
	stone.PaintFacetsBy([&](const Vec3 &centre, const Vec3 &normal, size_t first) {
		double rise = std::clamp(centre.z / top, 0.0, 1.0);
		double facet = Spread(SeedShare(SubSeed(recipe.seed, static_cast<uint32_t>(first)), 0, SeedDice::SHARE_BITS), FACET_SPREAD);
		double mottle = Spread(LatticeNoise(centre.x + centre.z * MOTTLE_SHEAR.x, centre.y + centre.z * MOTTLE_SHEAR.y, MOTTLES_PER_UNIT, recipe.seed), MOTTLE_SPREAD);
		bool capped = normal.z > CAP_FACING || centre.z < SKIRT_SHARE * top;
		return PaintworkTone(capped ? Paintwork::Secondary : Paintwork::Primary, std::lerp(ROCK_FOOT_SHADE, ROCK_TOP_SHADE, rise) * facet * mottle);
	});
	return stone.Occlude(UpwardOpenness{0.0, top, ROCK_FOOT_OPENNESS, 1.0});
}

RockFit FitOf(RockModel model)
{
	const StoneRecipe &recipe = STONE_RECIPES[to_underlying(model)];
	return {{recipe.span.x, recipe.span.y}, recipe.follow, recipe.radius};
}

std::vector<ModelMesh> BuildRockModels()
{
	std::vector<ModelMesh> models(ROCK_MODELS);
	for (size_t kind = 0; kind < ROCK_MODELS; kind++) {
		const StoneRecipe &recipe = STONE_RECIPES[kind];
		ModelMesh stone = Shaped(static_cast<RockModel>(kind), recipe);
		models[kind] = std::move(Chiselled(Settled(stone, recipe.bury), recipe));
	}
	return models;
}
