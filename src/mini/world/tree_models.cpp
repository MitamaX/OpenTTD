/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tree_models.cpp The low poly trees of every kind, in a few shapes each and at each level of detail, built once in code. */

#include "../../stdafx.h"
#include "tree_models.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <ranges>
#include <numbers>

#include "../core/seed.h"
#include "../model/model_shapes.h"

#include "../../safeguards.h"

static constexpr Vec3 UP = {0.0, 0.0, 1.0};
static constexpr double TAU = 2.0 * std::numbers::pi;
static constexpr double FOOT = -0.06;

static constexpr double LEAF_TRANSLUCENCY = 0.45;
static constexpr double FROND_TRANSLUCENCY = 0.55;
static constexpr double FLESH_TRANSLUCENCY = 0.12;
static constexpr double CANDY_TRANSLUCENCY = 0.05;
static constexpr double FACET_SPREAD = 0.07;
static constexpr double CANOPY_ROUNDING = 0.45;
static constexpr double SHADED_UNDERSIDE = 0.4;
static constexpr double SHADED_FOOT = 0.45;
static constexpr double LIT_TRUNK = 0.8;
static constexpr uint32_t SHAPE_SALT = 0x5EED7EE5U;

static constexpr SeedRange LUMP_SCATTER = {-0.3, 0.3};
static constexpr double MASS_ROUGHNESS = 0.06;

static constexpr SeedRange BROADLEAF_CROWN = {0.24, 0.27};
static constexpr double BROADLEAF_UNDERSIDE = 0.13;
static constexpr double BROADLEAF_TOP = 0.16;
static constexpr SeedRange BROADLEAF_TRUNK = {0.026, 0.016};
static constexpr double BROADLEAF_TRUNK_INSET = 0.04;
static constexpr Vec3 BROADLEAF_HEART = {0.14, 0.14, 0.12};
static constexpr double BROADLEAF_HEART_ROUGHNESS = 0.1;
static constexpr int BROADLEAF_RING_FEWEST = 4;
static constexpr SeedRange BROADLEAF_CREST_SCATTER = {-0.025, 0.025};
static constexpr double BROADLEAF_CREST_RISE = 0.08;
static constexpr Vec3 BROADLEAF_CREST = {0.085, 0.085, 0.075};
static constexpr Vec3 BROADLEAF_MASS = {0.19, 0.19, 0.16};
static constexpr double BROADLEAF_MASS_RISE = 0.01;

static constexpr SeedRange CONIFER_GIRTH = {0.9, 1.1};
static constexpr SeedRange CONIFER_TOP = {0.5, 0.56};
static constexpr SeedRange CONIFER_TRUNK = {0.02, 0.012};
static constexpr double CONIFER_TRUNK_TOP = 0.2;
static constexpr double CONIFER_BOUGHS_BASE = 0.07;
static constexpr double CONIFER_RADIUS = 0.155;
static constexpr int CONIFER_TIERS = 4;
static constexpr double CONIFER_TOP_TIER_REACH = 0.22;
static constexpr double CONIFER_TIER_TAPER = 0.65;
static constexpr double CONIFER_TIER_HEIGHT = 0.21;
static constexpr double CONIFER_TIER_SHRINK = 0.19;
static constexpr LatheRing CONIFER_WAIST = {0.55, 0.45};
static constexpr double CONIFER_ROUGHNESS = 0.008;

static constexpr SeedRange JUNGLE_CROWN = {0.44, 0.5};
static constexpr SeedRange JUNGLE_LEAN = {-0.03, 0.03};
static constexpr double JUNGLE_UNDERSIDE = 0.06;
static constexpr double JUNGLE_TOP = 0.08;
static constexpr SeedRange JUNGLE_TRUNK = {0.024, 0.016};
static constexpr std::array<LatheRing, 4> JUNGLE_BENDS = {{{1.0, 0.0}, {0.85, 0.4}, {0.75, 0.75}, {0.65, 1.0}}};
static constexpr Vec3 JUNGLE_HEART = {0.13, 0.13, 0.06};
static constexpr double JUNGLE_HEART_RISE = 0.03;
static constexpr int JUNGLE_RING_LUMPS = 4;
static constexpr Vec3 JUNGLE_MASS = {0.21, 0.21, 0.085};
static constexpr double JUNGLE_MASS_RISE = 0.02;

static constexpr SeedRange PALM_HEIGHT = {0.42, 0.48};
static constexpr SeedRange PALM_LEAN = {0.04, 0.1};
static constexpr double PALM_UNDERSIDE = 0.12;
static constexpr double PALM_TOP = 0.06;
static constexpr double PALM_TRUNK = 0.02;
static constexpr double PALM_TRUNK_TAPER = 0.3;
static constexpr double PALM_SPREAD = 0.19;
static constexpr int PALM_FRONDS_FEWEST = 7;
static constexpr Vec3 PALM_NUTS = {0.03, 0.03, 0.025};
static constexpr SeedRange FROND_SCATTER = {-0.2, 0.2};
static constexpr SeedRange FROND_LENGTH = {0.17, 0.22};
static constexpr SeedRange FROND_DROOP = {0.16, 0.22};
static constexpr double FROND_WIDTH = 0.045;
static constexpr double FROND_WIDEST = 0.8;
static constexpr double FROND_RIB = 0.004;
static constexpr double FROND_ARCH = 0.06;
static constexpr double FROND_FOLD = 0.25;

static constexpr SeedRange CACTUS_HEIGHT = {0.24, 0.3};
static constexpr double CACTUS_FOOT = -0.04;
static constexpr double CACTUS_RADIUS = 0.034;
static constexpr double CACTUS_DOME = 0.05;
/* The stem's rings as radius and depth below its top; its foot stands in the ground. */
static constexpr std::array<LatheRing, 5> CACTUS_STEM = {{{0.032, 0.0}, {0.035, 0.09}, {0.03, 0.04}, {0.018, 0.01}, {0.0, 0.0}}};
static constexpr double CACTUS_ARM_RADIUS = 0.02;
static constexpr SeedRange CACTUS_ARM_SCATTER = {-0.4, 0.4};
static constexpr SeedRange CACTUS_ARM_FROM = {0.07, 0.13};
static constexpr std::array<SweepPoint, 5> CACTUS_ARM_BENDS = {{
	{{0.0, 0.0, 0.0}, 1.0},
	{{0.045, 0.0, 0.005}, 1.0},
	{{0.065, 0.0, 0.035}, 1.0},
	{{0.068, 0.0, 0.09}, 0.85},
	{{0.066, 0.0, 0.105}, 0.4},
}};
static constexpr Vec3 CACTUS_FLOWER = {0.014, 0.014, 0.01};

static constexpr SeedRange TOY_CROWN = {0.27, 0.31};
static constexpr double TOY_CANDY = 0.11;
static constexpr Vec3 TOY_LOLLIPOP = {TOY_CANDY, TOY_CANDY, TOY_CANDY * 0.9};
static constexpr SeedRange TOY_STICK = {0.013, 0.011};
static constexpr double TOY_STICK_INSET = 0.03;
static constexpr int TOY_GUMDROPS = 3;
static constexpr double TOY_GUMDROP_HEIGHT = 0.1;
static constexpr double TOY_GUMDROP_TAPER = 0.75;
static constexpr double TOY_GUMDROP_STEP = 0.2;
static constexpr double TOY_CUBE_HALF = 0.075;

/* A shape's two colours, picked on screen: its leaves, and its wood or whatever else sets them off. */
struct TreeTones {
	uint32_t leaves;
	uint32_t wood;
};

static constexpr std::array<std::array<TreeTones, TREE_SHAPES>, TREE_KINDS> TONES = {{
	{{{0x5E8C3A, 0x6B4A2F}, {0x4E8034, 0x634329}, {0x6E9A3E, 0x705036}}},
	{{{0x3A6B38, 0x5A3E28}, {0x447334, 0x553A26}, {0x356640, 0x5E432C}}},
	{{{0x3B7A2E, 0x6E5A3F}, {0x2F6E2A, 0x665238}, {0x4C8A30, 0x76603F}}},
	{{{0x5E9A3A, 0x8A7050}, {0x6AA040, 0x7E6649}, {0x548E36, 0x927858}}},
	{{{0x5E8A4A, 0xE86FA8}, {0x6B9A55, 0xF2C94A}, {0x557F45, 0xF4EEE4}}},
	{{{0xF07FB0, 0xF4EEE4}, {0x7FC8F0, 0xF4EEE4}, {0xF2B24A, 0xF4EEE4}}},
}};

/* Where a canopy spans from its underside to its top, so its facets darken toward the trunk. */
struct Crown {
	Vec3 centre;
	double underside;
	double top;

	double Openness(const Vec3 &at) const
	{
		double share = std::clamp((at.z - this->underside) / (this->top - this->underside), 0.0, 1.0);
		return SHADED_UNDERSIDE + (1.0 - SHADED_UNDERSIDE) * share * share * (3.0 - 2.0 * share);
	}
};

/* Lumps set round a crown: how far out, how big and how far up or down each may lie, how flat they are and how rough. */
struct LumpRing {
	SeedRange reach;
	SeedRange radius;
	SeedRange rise;
	double squash;
	double roughness;
};

static constexpr LumpRing BROADLEAF_RING = {{0.085, 0.11}, {0.08, 0.105}, {-0.05, 0.03}, 0.9, 0.12};
static constexpr LumpRing JUNGLE_RING = {{0.09, 0.12}, {0.085, 0.11}, {-0.025, 0.01}, 0.5, 0.14};

/* One shape at one detail, built up part by part. */
class TreeBuilder {
public:
	TreeBuilder(const TreeTones &tones, uint32_t seed) : tones(tones), dice(seed) {}

	SeedDice &Dice() { return this->dice; }

	/* A lump of foliage: a jittered sphere squashed to its radii, turned at random so no two lumps facet alike. */
	ModelMesh Lump(const Vec3 &centre, const Vec3 &radii, int subdivisions, double roughness)
	{
		ModelMesh lump = Icosphere(subdivisions);
		lump.Transform(Mat4::Turn(UP, this->dice.Between(0.0, TAU)) * Mat4::Scaling(radii));
		lump.Displace(roughness * std::min(radii.x, radii.z), this->dice.Next());
		return lump.Transform(Mat4::Translation(centre));
	}

	void Leaves(ModelMesh part, const Crown &crown, double translucency = LEAF_TRANSLUCENCY)
	{
		this->Leaves(part, crown, this->tones.leaves, translucency);
	}

	void Leaves(ModelMesh part, const Crown &crown, uint32_t tone, double translucency)
	{
		part.Facet().Round(crown.centre, CANOPY_ROUNDING).Paint(tone).Vary(FACET_SPREAD, this->dice.Next()).Translucent(translucency);
		this->model.Append(part.Occlude([&](const Vec3 &at) { return crown.Openness(at); }));
	}

	/* Wood darkens toward the ground it stands in. */
	void Wood(ModelMesh part, double top)
	{
		part.Facet().Paint(this->tones.wood).Vary(FACET_SPREAD, this->dice.Next());
		this->model.Append(part.Occlude([&](const Vec3 &at) { return SHADED_FOOT + (LIT_TRUNK - SHADED_FOOT) * std::clamp((at.z - FOOT) / (top - FOOT), 0.0, 1.0); }));
	}

	/* A trunk tapering from its foot radius to its top one. */
	void Trunk(int sides, SeedRange radii, double top)
	{
		this->Wood(Column(sides, radii.low, radii.high, top - FOOT).Transform(Mat4::Translation({0.0, 0.0, FOOT})), top);
	}

	/* Lumps around a crown's middle, each set a little off its even place. */
	ModelMesh Ring(const Vec3 &centre, const LumpRing &ring, int count)
	{
		ModelMesh lumps;
		double turn = this->dice.Between(0.0, TAU);
		for (int lump = 0; lump < count; lump++) {
			double angle = turn + TAU * lump / count + this->dice.Between(LUMP_SCATTER);
			double reach = this->dice.Between(ring.reach);
			double radius = this->dice.Between(ring.radius);
			Vec3 at = centre + Vec3{reach * std::cos(angle), reach * std::sin(angle), this->dice.Between(ring.rise)};
			lumps.Append(this->Lump(at, {radius, radius, radius * ring.squash}, 1, ring.roughness));
		}
		return lumps;
	}

	/* A far tree as one smooth turned solid, wood below the given height and leaves above; too small on screen to show facets. */
	void Spindle(std::initializer_list<LatheRing> profile, int sides, double wood_top, double translucency)
	{
		ModelMesh spindle = Lathe(profile, sides);
		spindle.Transform(Mat4::Turn(UP, this->dice.Between(0.0, TAU)));
		spindle.PaintBy([&](const Vec3 &at) { return at.z < wood_top ? this->tones.wood : this->tones.leaves; }).Translucent(translucency);
		this->model.Append(spindle.Occlude([&](const Vec3 &at) { return at.z < wood_top ? SHADED_FOOT : 1.0; }));
	}

	const TreeTones &Tones() const { return this->tones; }
	ModelMesh Finish() { return std::move(this->model); }

private:
	const TreeTones &tones;
	SeedDice dice;
	ModelMesh model;
};

static void Broadleaf(TreeBuilder &tree, TreeDetail detail)
{
	SeedDice &dice = tree.Dice();
	double crown_z = dice.Between(BROADLEAF_CROWN);
	Crown crown = {{0.0, 0.0, crown_z}, crown_z - BROADLEAF_UNDERSIDE, crown_z + BROADLEAF_TOP};
	double trunk_top = crown_z - BROADLEAF_TRUNK_INSET;
	switch (detail) {
		case TreeDetail::Full: {
			tree.Trunk(6, BROADLEAF_TRUNK, trunk_top);
			ModelMesh canopy = tree.Lump(crown.centre, BROADLEAF_HEART, 1, BROADLEAF_HEART_ROUGHNESS);
			canopy.Append(tree.Ring(crown.centre, BROADLEAF_RING, BROADLEAF_RING_FEWEST + static_cast<int>(dice.Below(2))));
			Vec3 crest = {dice.Between(BROADLEAF_CREST_SCATTER), dice.Between(BROADLEAF_CREST_SCATTER), crown_z + BROADLEAF_CREST_RISE};
			canopy.Append(tree.Lump(crest, BROADLEAF_CREST, 1, BROADLEAF_RING.roughness));
			tree.Leaves(std::move(canopy), crown);
			break;
		}
		case TreeDetail::Simple:
			tree.Trunk(4, BROADLEAF_TRUNK, trunk_top);
			tree.Leaves(tree.Lump(crown.centre + Vec3{0.0, 0.0, BROADLEAF_MASS_RISE}, BROADLEAF_MASS, 0, MASS_ROUGHNESS), crown);
			break;
		default:
			tree.Spindle({{BROADLEAF_TRUNK.low, FOOT}, {BROADLEAF_MASS.x, crown_z}, {0.0, crown.top}}, 5, crown.underside, LEAF_TRANSLUCENCY);
			break;
	}
}

static void Conifer(TreeBuilder &tree, TreeDetail detail)
{
	SeedDice &dice = tree.Dice();
	double girth = dice.Between(CONIFER_GIRTH);
	double top = dice.Between(CONIFER_TOP);
	Crown crown = {{0.0, 0.0, (CONIFER_BOUGHS_BASE + top) * 0.5}, CONIFER_BOUGHS_BASE, top};
	double radius = CONIFER_RADIUS * girth;
	switch (detail) {
		case TreeDetail::Full: {
			tree.Trunk(5, CONIFER_TRUNK, CONIFER_TRUNK_TOP);
			ModelMesh boughs;
			for (int tier = 0; tier < CONIFER_TIERS; tier++) {
				double share = static_cast<double>(tier) / CONIFER_TIERS;
				double base = CONIFER_BOUGHS_BASE + share * (top - CONIFER_TOP_TIER_REACH);
				double tier_radius = radius * (1.0 - share * CONIFER_TIER_TAPER);
				double height = tier == CONIFER_TIERS - 1 ? top - base : CONIFER_TIER_HEIGHT * (1.0 - share * CONIFER_TIER_SHRINK);
				std::array<LatheRing, 3> profile = {{{tier_radius, 0.0}, {tier_radius * CONIFER_WAIST.radius, height * CONIFER_WAIST.z}, {0.0, height}}};
				ModelMesh bough = Lathe(profile, 7);
				bough.Transform(Mat4::Translation({0.0, 0.0, base}) * Mat4::Turn(UP, dice.Between(0.0, TAU)));
				boughs.Append(bough.Displace(CONIFER_ROUGHNESS, dice.Next()));
			}
			tree.Leaves(std::move(boughs), crown);
			break;
		}
		case TreeDetail::Simple:
			tree.Trunk(4, CONIFER_TRUNK, CONIFER_BOUGHS_BASE);
			tree.Leaves(Cone(6, radius, top - CONIFER_BOUGHS_BASE).Transform(Mat4::Translation({0.0, 0.0, CONIFER_BOUGHS_BASE})), crown);
			break;
		default:
			tree.Spindle({{CONIFER_TRUNK.low, FOOT}, {radius, CONIFER_BOUGHS_BASE}, {0.0, top}}, 4, 0.0, LEAF_TRANSLUCENCY);
			break;
	}
}

static void Jungle(TreeBuilder &tree, TreeDetail detail)
{
	SeedDice &dice = tree.Dice();
	double crown_z = dice.Between(JUNGLE_CROWN);
	Vec3 lean = {dice.Between(JUNGLE_LEAN), dice.Between(JUNGLE_LEAN), 0.0};
	Crown crown = {Vec3{0.0, 0.0, crown_z} + lean, crown_z - JUNGLE_UNDERSIDE, crown_z + JUNGLE_TOP};
	switch (detail) {
		case TreeDetail::Full: {
			std::array<SweepPoint, JUNGLE_BENDS.size()> spine;
			for (size_t bend = 0; bend < JUNGLE_BENDS.size(); bend++) {
				double share = JUNGLE_BENDS[bend].z;
				spine[bend] = {lean * share * share + Vec3{0.0, 0.0, FOOT + (crown_z - FOOT) * share}, JUNGLE_BENDS[bend].radius};
			}
			tree.Wood(Sweep(Circle(JUNGLE_TRUNK.low, 5), true, spine, {1.0, 0.0, 0.0}), crown_z);
			ModelMesh canopy = tree.Lump(crown.centre + Vec3{0.0, 0.0, JUNGLE_HEART_RISE}, JUNGLE_HEART, 1, JUNGLE_RING.roughness);
			canopy.Append(tree.Ring(crown.centre, JUNGLE_RING, JUNGLE_RING_LUMPS));
			tree.Leaves(std::move(canopy), crown);
			break;
		}
		case TreeDetail::Simple:
			tree.Trunk(4, JUNGLE_TRUNK, crown_z);
			tree.Leaves(tree.Lump(crown.centre + Vec3{0.0, 0.0, JUNGLE_MASS_RISE}, JUNGLE_MASS, 0, MASS_ROUGHNESS), crown);
			break;
		default:
			tree.Spindle({{JUNGLE_TRUNK.low, FOOT}, {JUNGLE_TRUNK.low, crown.underside}, {JUNGLE_MASS.x, crown_z}, {0.0, crown.top}}, 5, crown.underside, LEAF_TRANSLUCENCY);
			break;
	}
}

/* A frond arches out from the crown and droops toward its tip, folded along its rib. */
static ModelMesh Frond(const Vec3 &crown, double angle, double length, double droop, int points, bool folded)
{
	Vec3 out = {std::cos(angle), std::sin(angle), 0.0};
	std::vector<SweepPoint> rib;
	for (int point = 0; point < points; point++) {
		double share = static_cast<double>(point) / (points - 1);
		double width = FROND_WIDTH * std::sin(std::numbers::pi * std::min(share / FROND_WIDEST, 1.0)) + FROND_RIB;
		rib.push_back({crown + out * (length * share) + UP * (FROND_ARCH * share - droop * share * share), point == points - 1 ? 0.0 : width});
	}
	std::array<MapVector, 3> fold = {{{-1.0, FROND_FOLD}, {0.0, 0.0}, {1.0, FROND_FOLD}}};
	std::array<MapVector, 2> flat = {{{-1.0, 0.0}, {1.0, 0.0}}};
	Vec3 side = {-out.y, out.x, 0.0};
	return folded ? Sweep(fold, false, rib, side) : Sweep(flat, false, rib, side);
}

static void Palm(TreeBuilder &tree, TreeDetail detail)
{
	SeedDice &dice = tree.Dice();
	double height = dice.Between(PALM_HEIGHT);
	double lean = dice.Between(PALM_LEAN);
	Vec3 crown_at = {lean, 0.0, height};
	Crown crown = {crown_at, height - PALM_UNDERSIDE, height + PALM_TOP};
	if (detail == TreeDetail::Crude) {
		tree.Spindle({{PALM_TRUNK, FOOT}, {PALM_TRUNK, crown.underside}, {PALM_SPREAD, height}, {0.0, crown.top}}, 5, crown.underside, FROND_TRANSLUCENCY);
		return;
	}

	bool full = detail == TreeDetail::Full;
	int rings = full ? 6 : 3;
	std::vector<SweepPoint> spine;
	for (int ring = 0; ring < rings; ring++) {
		double share = static_cast<double>(ring) / (rings - 1);
		spine.push_back({{lean * share * share, 0.0, FOOT + (height - FOOT) * share}, 1.0 - PALM_TRUNK_TAPER * share});
	}
	tree.Wood(Sweep(Circle(PALM_TRUNK, full ? 6 : 4), true, spine, {0.0, 1.0, 0.0}), height);

	int fronds = full ? PALM_FRONDS_FEWEST + static_cast<int>(dice.Below(3)) : PALM_FRONDS_FEWEST - 2;
	double turn = dice.Between(0.0, TAU);
	ModelMesh leaves;
	for (int frond = 0; frond < fronds; frond++) {
		double angle = turn + TAU * frond / fronds + dice.Between(FROND_SCATTER);
		leaves.Append(Frond(crown_at, angle, dice.Between(FROND_LENGTH), dice.Between(FROND_DROOP), full ? 5 : 3, full));
	}
	if (full) leaves.Append(tree.Lump(crown_at - Vec3{0.0, 0.0, PALM_NUTS.z * 0.6}, PALM_NUTS, 0, MASS_ROUGHNESS));
	tree.Leaves(std::move(leaves), crown, FROND_TRANSLUCENCY);
}

/* An arm reaches out from the stem and turns upward, closing off at its tip. */
static ModelMesh CactusArm(double angle, double from, int sides, bool full)
{
	std::vector<SweepPoint> path;
	for (size_t bend = 0; bend < CACTUS_ARM_BENDS.size(); bend += full ? 1 : 2) {
		const SweepPoint &at = CACTUS_ARM_BENDS[bend];
		path.push_back({at.at + Vec3{0.0, 0.0, from}, at.scale});
	}
	return Sweep(Circle(CACTUS_ARM_RADIUS, sides), true, path, {0.0, 1.0, 0.0}).Transform(Mat4::Turn(UP, angle));
}

static void Cactus(TreeBuilder &tree, TreeDetail detail)
{
	SeedDice &dice = tree.Dice();
	double height = dice.Between(CACTUS_HEIGHT);
	Crown crown = {{0.0, 0.0, height * 0.5}, 0.0, height};
	switch (detail) {
		case TreeDetail::Full: {
			std::array<LatheRing, CACTUS_STEM.size()> stem;
			std::ranges::transform(CACTUS_STEM, stem.begin(), [&](const LatheRing &ring) { return LatheRing{ring.radius, height - ring.z}; });
			stem.front().z = CACTUS_FOOT;
			ModelMesh flesh = Lathe(stem, 8);
			int arms = 1 + static_cast<int>(dice.Below(2));
			double turn = dice.Between(0.0, TAU);
			for (int arm = 0; arm < arms; arm++) flesh.Append(CactusArm(turn + std::numbers::pi * arm + dice.Between(CACTUS_ARM_SCATTER), dice.Between(CACTUS_ARM_FROM), 6, true));
			tree.Leaves(std::move(flesh), crown, FLESH_TRANSLUCENCY);
			tree.Leaves(tree.Lump({0.0, 0.0, height}, CACTUS_FLOWER, 0, MASS_ROUGHNESS), crown, tree.Tones().wood, FLESH_TRANSLUCENCY);
			break;
		}
		case TreeDetail::Simple: {
			ModelMesh flesh = Lathe(std::array<LatheRing, 3>{{{CACTUS_RADIUS, CACTUS_FOOT}, {CACTUS_RADIUS, height - CACTUS_DOME}, {0.0, height}}}, 5);
			flesh.Append(CactusArm(dice.Between(0.0, TAU), CACTUS_ARM_FROM.low, 4, false));
			tree.Leaves(std::move(flesh), crown, FLESH_TRANSLUCENCY);
			break;
		}
		default:
			tree.Spindle({{CACTUS_RADIUS, CACTUS_FOOT}, {CACTUS_RADIUS, height - CACTUS_DOME}, {0.0, height}}, 4, CACTUS_FOOT, FLESH_TRANSLUCENCY);
			break;
	}
}

/* Toyland's sweets: a lollipop, a stack of gumdrops or a sugar cube, each on a stick. */
static void Toy(TreeBuilder &tree, TreeDetail detail, uint8_t variant)
{
	SeedDice &dice = tree.Dice();
	double crown_z = dice.Between(TOY_CROWN);
	Crown crown = {{0.0, 0.0, crown_z}, crown_z - TOY_CANDY, crown_z + TOY_CANDY};
	if (detail == TreeDetail::Crude) {
		tree.Spindle({{TOY_STICK.low, FOOT}, {TOY_STICK.low, crown.underside}, {TOY_CANDY, crown_z}, {0.0, crown.top}}, 5, crown.underside, CANDY_TRANSLUCENCY);
		return;
	}

	bool full = detail == TreeDetail::Full;
	tree.Trunk(full ? 6 : 4, TOY_STICK, crown.underside + TOY_STICK_INSET);
	switch (variant) {
		case 0:
			tree.Leaves(tree.Lump(crown.centre, TOY_LOLLIPOP, full ? 1 : 0, 0.0), crown, CANDY_TRANSLUCENCY);
			break;
		case 1: {
			int drops = full ? TOY_GUMDROPS : 1;
			ModelMesh stack;
			for (int drop = 0; drop < drops; drop++) {
				double share = static_cast<double>(drop) / drops;
				double height = full ? TOY_GUMDROP_HEIGHT : crown.top - crown.underside;
				stack.Append(Cone(full ? 8 : 5, TOY_CANDY * (1.0 - share * TOY_GUMDROP_TAPER), height).Transform(Mat4::Translation({0.0, 0.0, crown.underside + share * TOY_GUMDROP_STEP})));
			}
			tree.Leaves(std::move(stack), crown, CANDY_TRANSLUCENCY);
			break;
		}
		default: {
			Mat4 on_corner = Mat4::Translation(crown.centre) * Mat4::Turn({1.0, 1.0, 0.0}, std::atan(std::numbers::sqrt2)) * Mat4::Turn(UP, std::numbers::pi / 4.0);
			tree.Leaves(Box({-TOY_CUBE_HALF, -TOY_CUBE_HALF, -TOY_CUBE_HALF}, {TOY_CUBE_HALF, TOY_CUBE_HALF, TOY_CUBE_HALF}).Transform(on_corner), crown, CANDY_TRANSLUCENCY);
			break;
		}
	}
}

static ModelMesh BuildTree(TreeShape shape, TreeDetail detail)
{
	TreeBuilder tree(TONES[static_cast<size_t>(shape.kind)][shape.variant], Hash32(SHAPE_SALT + static_cast<uint32_t>(shape.Index())));
	switch (shape.kind) {
		case TreeKind::Broadleaf: Broadleaf(tree, detail); break;
		case TreeKind::Conifer: Conifer(tree, detail); break;
		case TreeKind::Jungle: Jungle(tree, detail); break;
		case TreeKind::Palm: Palm(tree, detail); break;
		case TreeKind::Cactus: Cactus(tree, detail); break;
		default: Toy(tree, detail, shape.variant); break;
	}
	return tree.Finish();
}

std::vector<ModelMesh> BuildTreeModels()
{
	std::vector<ModelMesh> meshes;
	for (size_t kind = 0; kind < TREE_KINDS; kind++) {
		for (uint8_t variant = 0; variant < TREE_SHAPES; variant++) {
			for (size_t detail = 0; detail < TREE_DETAILS; detail++) meshes.push_back(BuildTree({static_cast<TreeKind>(kind), variant}, static_cast<TreeDetail>(detail)));
		}
	}
	for (size_t mesh = 0; mesh < TREE_DRAWN_MESHES; mesh++) meshes.push_back(ModelMesh(meshes[mesh]).Weld());
	return meshes;
}
