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
#include <numeric>
#include <ranges>
#include <numbers>

#include "../core/camera.h"
#include "../core/seed.h"
#include "../core/tones.h"
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
static constexpr double SOFT_CANOPY_ROUNDING = 0.6;
static constexpr double SHADED_UNDERSIDE = 0.4;
static constexpr double CROWN_HOLLOW = 0.55;
static constexpr SeedRange CROWN_CORE_EDGES = {0.45, 0.9};
static constexpr double SHADED_FOOT = 0.45;
static constexpr double LIT_TRUNK = 0.8;
static constexpr uint32_t SHAPE_SALT = 0x5EED7EE5U;

static constexpr SeedRange LUMP_SCATTER = {-0.3, 0.3};
static constexpr double MASS_ROUGHNESS = 0.06;

static constexpr double TRUNK_INSET = 0.04;
static constexpr std::array<LatheRing, 4> TRUNK_BENDS = {{{1.0, 0.0}, {0.85, 0.4}, {0.75, 0.75}, {0.65, 1.0}}};
static constexpr SeedRange CLUSTER_HEIGHTS = {-0.3, 0.8};
static constexpr double CLUSTER_ROUGHNESS = 0.12;
static constexpr double HEART_SHARE = 0.55;
static constexpr double LIMB_RADIUS = 0.008;
static constexpr int LIMB_SIDES = 4;
static constexpr double LIMB_REACH = 0.85;
static constexpr double LIMB_TAPER = 0.5;
static constexpr double LIMB_SKIP = 0.04;

static constexpr SeedRange CONIFER_GIRTH = {0.9, 1.1};
static constexpr SeedRange CONIFER_TRUNK = {0.02, 0.012};
static constexpr double CONIFER_TRUNK_TOP = 0.2;
static constexpr uint32_t CONIFER_DEEP_GREEN = 0x2C5A44;
static constexpr uint CONIFER_DEEP_SHARE = 110;
static constexpr double CONIFER_TIER_SPAN = 0.67;
static constexpr double CONIFER_TIER_DEPTH = 0.45;
static constexpr double CONIFER_TIER_SHRINK = 0.19;
static constexpr int CONIFER_SIMPLE_TIPS = 4;
static constexpr LatheRing CONIFER_WAIST = {0.55, 0.45};
static constexpr double CONIFER_ROUGHNESS = 0.008;

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
static constexpr double TOY_CANDY = 0.13;
static constexpr Vec3 TOY_LOLLIPOP = {TOY_CANDY, TOY_CANDY, TOY_CANDY * 0.9};
static constexpr SeedRange TOY_STICK = {0.013, 0.011};
static constexpr double TOY_STICK_INSET = 0.03;
static constexpr int TOY_GUMDROPS = 3;
static constexpr double TOY_GUMDROP_HEIGHT = 0.1;
static constexpr double TOY_GUMDROP_TAPER = 0.75;
static constexpr double TOY_GUMDROP_STEP = 0.2;
static constexpr double TOY_CUBE_HALF = 0.09;

/* A shape's two colours, picked on screen: its leaves, and its wood or whatever else sets them off. */
struct TreeTones {
	uint32_t leaves;
	uint32_t wood;
};

static constexpr size_t TONE_SETS = 3;
static constexpr std::array<std::array<TreeTones, TONE_SETS>, TREE_KINDS> TONES = {{
	{{{0x5E8C3A, 0x5E5246}, {0x4E8034, 0x564A3E}, {0x7EA046, 0xCFC9BA}}},
	{{{0x3A6B38, 0x54463A}, {0x2F6046, 0x4E4136}, {0x557F3A, 0x8A5A3C}}},
	{{{0x3B7A2E, 0x6A604E}, {0x2F6E2A, 0x625846}, {0x4C8A30, 0x72684F}}},
	{{{0x5E9A3A, 0x8A7050}, {0x6AA040, 0x7E6649}, {0x548E36, 0x927858}}},
	{{{0x5E8A4A, 0xE86FA8}, {0x6B9A55, 0xF2C94A}, {0x557F45, 0xF4EEE4}}},
	{{{0xF07FB0, 0xF4EEE4}, {0x7FC8F0, 0xF4EEE4}, {0xF2B24A, 0xF4EEE4}}},
}};

static double FootOpenness(const Vec3 &at, double top)
{
	return std::lerp(SHADED_FOOT, LIT_TRUNK, std::clamp((at.z - FOOT) / (top - FOOT), 0.0, 1.0));
}

/* Where a canopy spans from its underside to its top, so its facets darken toward the trunk. */
struct Crown {
	Vec3 centre;
	double underside;
	double top;
	double reach = 0.0;

	double Outness(const Vec3 &at) const
	{
		if (this->reach <= 0.0) return 1.0;
		double across = std::hypot(at.x - this->centre.x, at.y - this->centre.y) / this->reach;
		double rise = at.z - this->centre.z;
		double up = rise / (rise > 0.0 ? this->top - this->centre.z : this->centre.z - this->underside);
		return SmoothStep(CROWN_CORE_EDGES.low, CROWN_CORE_EDGES.high, std::hypot(across, up));
	}

	double Openness(const Vec3 &at) const
	{
		double height = std::lerp(SHADED_UNDERSIDE, 1.0, SmoothStep(this->underside, this->top, at.z));
		return height * std::lerp(CROWN_HOLLOW, 1.0, this->Outness(at));
	}
};

struct CrownHabit {
	SeedRange base;
	Vec3 hull;
	int clusters;
	uint32_t extra_clusters;
	SeedRange cluster;
	double squash;
	bool heart;
	SeedRange lean;
	SeedRange trunk;
	std::array<uint8_t, NUM_LANDSCAPE> habitat;
};

static constexpr std::array<CrownHabit, TREE_SHAPES> BROADLEAF_HABITS = {{
	{{0.15, 0.18}, {0.29, 0.29, 0.13}, 5, 2, {0.075, 0.095}, 0.75, true, {0.0, 0.0}, {0.032, 0.02}, {4, 1, 1, 1}},
	{{0.11, 0.13}, {0.22, 0.22, 0.17}, 4, 2, {0.085, 0.105}, 0.95, true, {0.0, 0.0}, {0.032, 0.02}, {3, 1, 1, 1}},
	{{0.2, 0.23}, {0.18, 0.18, 0.16}, 7, 2, {0.05, 0.065}, 0.85, false, {0.0, 0.02}, {0.02, 0.014}, {1, 6, 1, 1}},
	{{0.05, 0.07}, {0.19, 0.19, 0.12}, 4, 1, {0.07, 0.085}, 0.9, true, {0.0, 0.0}, {0.032, 0.02}, {1, 2, 1, 1}},
	{{0.16, 0.18}, {0.23, 0.23, 0.12}, 4, 1, {0.07, 0.09}, 0.8, true, {0.05, 0.08}, {0.032, 0.02}, {1, 1, 1, 1}},
	{{0.07, 0.09}, {0.12, 0.12, 0.24}, 4, 1, {0.065, 0.08}, 1.15, false, {0.0, 0.0}, {0.026, 0.016}, {2, 0, 1, 1}},
}};

struct ConiferHabit {
	SeedRange top;
	double boughs;
	double radius;
	int tiers;
	uint32_t extra_tiers;
	double taper;
	int tips;
	double notch;
	double droop;
	std::array<uint8_t, NUM_LANDSCAPE> habitat;
};

static constexpr std::array<ConiferHabit, TREE_SHAPES> CONIFER_HABITS = {{
	{{0.48, 0.54}, 0.06, 0.18, 4, 0, 0.7, 7, 0.72, 0.22, {4, 1, 1, 1}},
	{{0.52, 0.58}, 0.05, 0.145, 5, 0, 0.75, 7, 0.62, 0.38, {2, 4, 1, 1}},
	{{0.48, 0.54}, 0.28, 0.16, 2, 0, 0.35, 6, 0.55, 0.08, {3, 1, 1, 1}},
	{{0.28, 0.34}, 0.03, 0.13, 3, 0, 0.7, 7, 0.7, 0.28, {1, 2, 1, 1}},
	{{0.5, 0.58}, 0.04, 0.1, 6, 0, 0.6, 6, 0.75, 0.45, {0, 4, 1, 1}},
	{{0.46, 0.52}, 0.08, 0.135, 4, 0, 0.7, 6, 0.5, 0.18, {1, 2, 1, 1}},
}};

static constexpr std::array<CrownHabit, TREE_SHAPES> JUNGLE_HABITS = {{
	{{0.4, 0.44}, {0.26, 0.26, 0.065}, 6, 2, {0.07, 0.09}, 0.55, true, {0.0, 0.03}, {0.024, 0.016}, {1, 1, 1, 1}},
	{{0.2, 0.24}, {0.22, 0.22, 0.12}, 5, 2, {0.08, 0.1}, 0.75, true, {0.0, 0.02}, {0.024, 0.016}, {1, 1, 1, 1}},
	{{0.16, 0.2}, {0.18, 0.18, 0.17}, 6, 1, {0.06, 0.075}, 0.5, false, {0.0, 0.02}, {0.024, 0.016}, {1, 1, 1, 1}},
	{{0.28, 0.32}, {0.2, 0.2, 0.09}, 5, 1, {0.07, 0.09}, 0.6, true, {0.06, 0.09}, {0.024, 0.016}, {1, 1, 1, 1}},
	{{0.05, 0.07}, {0.17, 0.17, 0.12}, 5, 1, {0.07, 0.085}, 0.85, true, {0.0, 0.0}, {0.024, 0.016}, {1, 1, 2, 1}},
	{{0.3, 0.34}, {0.13, 0.13, 0.1}, 4, 1, {0.06, 0.075}, 0.7, false, {0.0, 0.03}, {0.018, 0.012}, {1, 1, 1, 1}},
}};

static constexpr std::array<uint8_t, TREE_SHAPES> EVEN_WEIGHTS = {1, 1, 1, 1, 1, 1};

template <class Habit>
static std::array<uint8_t, TREE_SHAPES> Weights(const std::array<Habit, TREE_SHAPES> &habits, LandscapeType landscape)
{
	std::array<uint8_t, TREE_SHAPES> weights;
	std::ranges::transform(habits, weights.begin(), [&](const Habit &habit) { return habit.habitat[to_underlying(landscape)]; });
	return weights;
}

static std::array<uint8_t, TREE_SHAPES> HabitatWeights(TreeKind kind, LandscapeType landscape)
{
	switch (kind) {
		case TreeKind::Broadleaf: return Weights(BROADLEAF_HABITS, landscape);
		case TreeKind::Conifer: return Weights(CONIFER_HABITS, landscape);
		case TreeKind::Jungle: return Weights(JUNGLE_HABITS, landscape);
		default: return EVEN_WEIGHTS;
	}
}

static std::array<uint8_t, TREE_SHAPES> WeightsOf(TreeKind kind, LandscapeType landscape)
{
	std::array<uint8_t, TREE_SHAPES> weights = HabitatWeights(kind, landscape);
	return std::ranges::all_of(weights, [](uint8_t weight) { return weight == 0; }) ? EVEN_WEIGHTS : weights;
}

uint8_t PickTreeShape(TreeKind kind, LandscapeType landscape, double roll)
{
	std::array<uint8_t, TREE_SHAPES> weights = WeightsOf(kind, landscape);
	double reach = roll * std::reduce(weights.begin(), weights.end(), 0U);
	uint8_t picked = 0;
	uint passed = 0;
	for (uint8_t shape = 0; shape < TREE_SHAPES; shape++) {
		if (weights[shape] == 0) continue;
		picked = shape;
		passed += weights[shape];
		if (reach < passed) break;
	}
	return picked;
}

enum class LeafSurface : uint8_t {
	Soft,
	Faceted,
};

static LeafSurface SurfaceOf(TreeKind kind)
{
	return kind == TreeKind::Broadleaf || kind == TreeKind::Jungle ? LeafSurface::Soft : LeafSurface::Faceted;
}

/* One shape at one detail, built up part by part. */
class TreeBuilder {
public:
	TreeBuilder(const TreeTones &tones, LeafSurface surface, uint32_t seed) : tones(tones), surface(surface), dice(seed) {}

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
		if (this->surface == LeafSurface::Faceted) {
			part.Facet().Round(crown.centre, CANOPY_ROUNDING).Paint(tone).Vary(FACET_SPREAD, this->dice.Next());
		} else {
			part.Smooth().Round(crown.centre, SOFT_CANOPY_ROUNDING).Paint(tone);
		}
		part.Translucent(translucency);
		this->model.Append(part.Occlude([&](const Vec3 &at) { return crown.Openness(at); }));
	}

	template <class Openness>
	void Wood(ModelMesh part, Openness openness)
	{
		part.Facet().Paint(this->tones.wood).Vary(FACET_SPREAD, this->dice.Next());
		this->model.Append(part.Occlude(openness));
	}

	/* Wood darkens toward the ground it stands in. */
	void Wood(ModelMesh part, double top)
	{
		this->Wood(std::move(part), [top](const Vec3 &at) { return FootOpenness(at, top); });
	}

	/* A trunk tapering from its foot radius to its top one. */
	void Trunk(int sides, SeedRange radii, double top, const Vec3 &lean = {})
	{
		if (lean.x == 0.0 && lean.y == 0.0) {
			this->Wood(Column(sides, radii.low, radii.high, top - FOOT).Transform(Mat4::Translation({0.0, 0.0, FOOT})), top);
			return;
		}
		std::array<SweepPoint, TRUNK_BENDS.size()> spine;
		for (size_t bend = 0; bend < TRUNK_BENDS.size(); bend++) {
			double share = TRUNK_BENDS[bend].z;
			spine[bend] = {lean * share * share + Vec3{0.0, 0.0, FOOT + (top - FOOT) * share}, TRUNK_BENDS[bend].radius};
		}
		this->Wood(Sweep(Circle(radii.low, sides), true, spine, {1.0, 0.0, 0.0}), top);
	}

	void Limb(const Vec3 &from, const Vec3 &to, const Crown &crown)
	{
		Vec3 out = to - from;
		std::array<SweepPoint, 2> path = {{{from, 1.0}, {from + out * LIMB_REACH, LIMB_TAPER}}};
		this->Wood(Sweep(Circle(LIMB_RADIUS, LIMB_SIDES), true, path, {-out.y, out.x, 0.0}), [&](const Vec3 &at) { return crown.Openness(at); });
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
	LeafSurface surface;
	SeedDice dice;
	ModelMesh model;
};

static ModelMesh Clusters(TreeBuilder &tree, const CrownHabit &habit, const Crown &crown, const Vec3 &trunk_top)
{
	SeedDice &dice = tree.Dice();
	int count = habit.clusters + static_cast<int>(dice.Below(habit.extra_clusters + 1));
	double turn = dice.Between(0.0, TAU);
	ModelMesh clusters;
	for (int cluster = 0; cluster < count; cluster++) {
		double height = dice.Between(CLUSTER_HEIGHTS);
		double angle = turn + TAU * cluster / count + dice.Between(LUMP_SCATTER);
		double radius = dice.Between(habit.cluster);
		double out = (habit.hull.x - radius) * std::sqrt(1.0 - height * height);
		Vec3 at = crown.centre + Vec3{out * std::cos(angle), out * std::sin(angle), height * (habit.hull.z - radius * habit.squash)};
		clusters.Append(tree.Lump(at, {radius, radius, radius * habit.squash}, 1, CLUSTER_ROUGHNESS));
		if (std::hypot(at.x - trunk_top.x, at.y - trunk_top.y) > LIMB_SKIP) tree.Limb(trunk_top, at, crown);
	}
	if (habit.heart) clusters.Append(tree.Lump(crown.centre, habit.hull * HEART_SHARE, 1, CLUSTER_ROUGHNESS));
	return clusters;
}

static void Canopy(TreeBuilder &tree, TreeDetail detail, const CrownHabit &habit)
{
	SeedDice &dice = tree.Dice();
	double heading = dice.Between(0.0, TAU);
	Vec3 lean = Vec3{std::cos(heading), std::sin(heading), 0.0} * dice.Between(habit.lean);
	double base = dice.Between(habit.base);
	Crown crown = {lean + Vec3{0.0, 0.0, base + habit.hull.z}, base, base + 2.0 * habit.hull.z, habit.hull.x};
	double trunk_top = base + TRUNK_INSET;
	switch (detail) {
		case TreeDetail::Full:
			tree.Trunk(6, habit.trunk, trunk_top, lean);
			tree.Leaves(Clusters(tree, habit, crown, lean + Vec3{0.0, 0.0, trunk_top}), crown);
			break;
		case TreeDetail::Simple:
			tree.Trunk(4, habit.trunk, trunk_top, lean);
			tree.Leaves(tree.Lump(crown.centre, habit.hull, 0, MASS_ROUGHNESS), crown);
			break;
		default:
			tree.Spindle({{habit.trunk.low, FOOT}, {habit.trunk.low, base}, {habit.hull.x, crown.centre.z}, {0.0, crown.top}}, 5, base, LEAF_TRANSLUCENCY);
			break;
	}
}

static ModelMesh Skirt(const ConiferHabit &habit, int tips, double radius, double height)
{
	std::array<LatheRing, 3> profile = {{{radius, 0.0}, {radius * CONIFER_WAIST.radius, height * CONIFER_WAIST.z}, {0.0, height}}};
	ModelMesh skirt = Lathe(profile, 2 * tips);
	skirt.Reshape([&](const Vec3 &at) {
		double out = std::hypot(at.x, at.y) / radius;
		double tip = 0.5 + 0.5 * std::cos(tips * std::atan2(at.y, at.x));
		double spread = std::lerp(1.0, std::lerp(habit.notch, 1.0, tip), out);
		return Vec3{at.x * spread, at.y * spread, at.z - habit.droop * radius * tip * out * out};
	});
	return skirt;
}

static void Conifer(TreeBuilder &tree, TreeDetail detail, const ConiferHabit &habit)
{
	SeedDice &dice = tree.Dice();
	double girth = dice.Between(CONIFER_GIRTH);
	double top = dice.Between(habit.top);
	double radius = habit.radius * girth;
	Crown crown = {{0.0, 0.0, (habit.boughs + top) * 0.5}, habit.boughs, top, radius};
	switch (detail) {
		case TreeDetail::Full: {
			tree.Trunk(5, CONIFER_TRUNK, std::max(habit.boughs, CONIFER_TRUNK_TOP));
			int tiers = habit.tiers + static_cast<int>(dice.Below(habit.extra_tiers + 1));
			for (int tier = 0; tier < tiers; tier++) {
				double share = static_cast<double>(tier) / tiers;
				double base = habit.boughs + share * (top - habit.boughs) * CONIFER_TIER_SPAN;
				double height = tier == tiers - 1 ? top - base : (top - habit.boughs) * CONIFER_TIER_DEPTH * (1.0 - share * CONIFER_TIER_SHRINK);
				ModelMesh bough = Skirt(habit, habit.tips, radius * (1.0 - share * habit.taper), height);
				bough.Transform(Mat4::Translation({0.0, 0.0, base}) * Mat4::Turn(UP, dice.Between(0.0, TAU)));
				uint32_t tone = Mix(tree.Tones().leaves, CONIFER_DEEP_GREEN, static_cast<uint>(std::lround(CONIFER_DEEP_SHARE * (1.0 - share))));
				tree.Leaves(bough.Displace(CONIFER_ROUGHNESS, dice.Next()), crown, tone, LEAF_TRANSLUCENCY);
			}
			break;
		}
		case TreeDetail::Simple:
			tree.Trunk(4, CONIFER_TRUNK, habit.boughs);
			tree.Leaves(Skirt(habit, std::max(habit.tips / 2, CONIFER_SIMPLE_TIPS), radius, top - habit.boughs).Transform(Mat4::Translation({0.0, 0.0, habit.boughs})), crown);
			break;
		default:
			tree.Spindle({{CONIFER_TRUNK.low, FOOT}, {CONIFER_TRUNK.low, habit.boughs}, {radius, habit.boughs}, {0.0, top}}, 4, habit.boughs, LEAF_TRANSLUCENCY);
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
	switch (variant % TONE_SETS) {
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
	TreeBuilder tree(TONES[static_cast<size_t>(shape.kind)][shape.variant % TONE_SETS], SurfaceOf(shape.kind), Hash32(SHAPE_SALT + static_cast<uint32_t>(shape.Index())));
	switch (shape.kind) {
		case TreeKind::Broadleaf: Canopy(tree, detail, BROADLEAF_HABITS[shape.variant]); break;
		case TreeKind::Conifer: Conifer(tree, detail, CONIFER_HABITS[shape.variant]); break;
		case TreeKind::Jungle: Canopy(tree, detail, JUNGLE_HABITS[shape.variant]); break;
		case TreeKind::Palm: Palm(tree, detail); break;
		case TreeKind::Cactus: Cactus(tree, detail); break;
		default: Toy(tree, detail, shape.variant); break;
	}
	return tree.Finish();
}

static bool WithinModelBounds(const ModelMesh &mesh)
{
	return std::ranges::all_of(mesh.vertices, [](const ModelVertex &vertex) {
		return std::hypot(vertex.x, vertex.y) <= TREE_MODEL_REACH && vertex.z <= TREE_MODEL_TALLEST;
	});
}

std::vector<ModelMesh> BuildTreeModels()
{
	std::vector<ModelMesh> meshes;
	for (size_t kind = 0; kind < TREE_KINDS; kind++) {
		for (uint8_t variant = 0; variant < TREE_SHAPES; variant++) {
			for (size_t detail = 0; detail < TREE_DETAILS; detail++) meshes.push_back(BuildTree({static_cast<TreeKind>(kind), variant}, static_cast<TreeDetail>(detail)));
		}
	}
	assert(std::ranges::all_of(meshes, WithinModelBounds));
	for (size_t mesh = 0; mesh < TREE_DRAWN_MESHES; mesh++) meshes.push_back(ModelMesh(meshes[mesh]).Weld());
	return meshes;
}
