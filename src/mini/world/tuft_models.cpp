/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tuft_models.cpp Tufts of grass: thin blades fanning out from a foot, painted like a vehicle's paintwork so each tuft takes its own green. */

#include "../../stdafx.h"
#include "tuft_models.h"

#include <cmath>

#include "../core/camera.h"
#include "../core/seed.h"
#include "vehicle_models.h"

#include "../../safeguards.h"

static constexpr Vec3 UP = {0.0, 0.0, 1.0};
static constexpr double GOLDEN_TURN = 2.39996323;
static constexpr uint32_t BLADE_FOOT = PaintworkTone(Paintwork::Primary, 0.7);
static constexpr uint32_t BLADE_TIP = PaintworkTone(Paintwork::Primary, 1.35);
static constexpr double NORMAL_SPREAD = 0.35;

/* How many blades a tuft has, how tall and how far they lean out, and how wide at the foot. */
struct TuftShape {
	int blades;
	SeedRange height;
	SeedRange lean;
	double width;
	uint32_t seed;
};

static constexpr std::array<TuftShape, TUFT_SHAPES> TUFT_SHAPE_LIST = {{
	{7, {0.03, 0.05}, {0.006, 0.02}, 0.007, 11},
	{10, {0.025, 0.045}, {0.01, 0.028}, 0.006, 23},
	{5, {0.04, 0.06}, {0.004, 0.014}, 0.008, 37},
}};

/* A blade rises from a foot near the tuft's middle to a tip leaning out, its faces lit as the ground about them is, a little toward the way it leans. */
static void AddBlade(ModelMesh &tuft, double turn, const TuftShape &shape, SeedDice &dice)
{
	MapVector out = {std::cos(turn), std::sin(turn)};
	MapVector side = {-out.y, out.x};
	double foot_reach = dice.Between(0.0, shape.width);
	double lean = dice.Between(shape.lean);
	Vec3 normal = Vec3{out.x * NORMAL_SPREAD, out.y * NORMAL_SPREAD, 0.0} + UP;
	Vec3 foot = {out.x * foot_reach, out.y * foot_reach, 0.0};
	Vec3 tip = foot + Vec3{out.x * lean, out.y * lean, dice.Between(shape.height)};
	Vec3 half_width = {side.x * shape.width / 2.0, side.y * shape.width / 2.0, 0.0};
	uint32_t left = tuft.Point(foot - half_width, normal);
	uint32_t right = tuft.Point(foot + half_width, normal);
	uint32_t top = tuft.Point(tip, normal);
	tuft.vertices[left].Paint(BLADE_FOOT);
	tuft.vertices[right].Paint(BLADE_FOOT);
	tuft.vertices[top].Paint(BLADE_TIP);
	tuft.Triangle(left, right, top);
}

static ModelMesh Tuft(const TuftShape &shape)
{
	ModelMesh tuft;
	SeedDice dice(shape.seed);
	for (int blade = 0; blade < shape.blades; blade++) AddBlade(tuft, blade * GOLDEN_TURN + dice.Between(-0.3, 0.3), shape, dice);
	return tuft;
}

std::vector<ModelMesh> BuildTuftModels()
{
	std::vector<ModelMesh> tufts;
	for (const TuftShape &shape : TUFT_SHAPE_LIST) tufts.push_back(Tuft(shape));
	return tufts;
}
