/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file street_furniture.cpp The small things standing beside ways: lamp posts, benches, litter bins, street trees and level crossing barriers. */

#include "../../stdafx.h"
#include "street_furniture.h"

#include <array>
#include <cmath>

#include "../model/model_shapes.h"

#include "../map/car_look.h"

#include "../../safeguards.h"

static constexpr Vec3 UP = {0.0, 0.0, 1.0};

static constexpr double POLE_RADIUS = 0.007;
static constexpr double POLE_HEIGHT = 0.26;
static constexpr double ARM_REACH = 0.055;
static constexpr double ARM_HALF = 0.004;
static constexpr Vec3 LANTERN_LOW = {ARM_REACH - 0.016, -0.011, POLE_HEIGHT - 0.014};
static constexpr Vec3 LANTERN_HIGH = {ARM_REACH + 0.012, 0.011, POLE_HEIGHT + 0.004};

static constexpr Vec3 SEAT_LOW = {-0.014, -0.045, 0.024};
static constexpr Vec3 SEAT_HIGH = {0.012, 0.045, 0.031};
static constexpr Vec3 BACK_LOW = {0.009, -0.045, 0.031};
static constexpr Vec3 BACK_HIGH = {0.014, 0.045, 0.06};
static constexpr Vec3 LEGS_LOW = {-0.01, -0.035, 0.0};
static constexpr Vec3 LEGS_HIGH = {0.01, 0.035, 0.024};

static constexpr int BIN_SIDES = 8;
static constexpr double BIN_RADIUS = 0.011;
static constexpr double BIN_HEIGHT = 0.03;
static constexpr double BIN_LID = 0.004;

static constexpr int TRUNK_SIDES = 5;
static constexpr double TRUNK_RADIUS = 0.006;
static constexpr double TRUNK_HEIGHT = 0.15;
static constexpr Vec3 CANOPY_SPAN = {0.065, 0.065, 0.075};
static constexpr double CANOPY_LUMPINESS = 0.18;
static constexpr double CANOPY_VARIETY = 0.12;

static constexpr Vec3 POST_LOW = {-0.012, -0.012, 0.0};
static constexpr Vec3 POST_HIGH = {0.012, 0.012, 0.17};
static constexpr Vec3 HOUSING_LOW = {0.012, -0.03, 0.13};
static constexpr Vec3 HOUSING_HIGH = {0.02, 0.03, 0.165};
static constexpr double LAMP_HALF = 0.009;
static constexpr double LAMP_SPREAD = 0.017;
static constexpr double LAMP_Z = 0.147;
static constexpr double BOOM_HALF = 0.006;
static constexpr double BOOM_FOOT = 0.1;
static constexpr int BOOM_BANDS = 5;
static constexpr double BOOM_BAND = 0.065;

static constexpr uint32_t POLE = 0x3E4348;
static constexpr uint32_t LANTERN = 0xEDE3C6;
static constexpr uint32_t TIMBER = 0x86603E;
static constexpr uint32_t IRON = 0x2E3236;
static constexpr uint32_t POST_WHITE = 0xE6E3DC;
static constexpr uint32_t WARNING_RED = 0xB8322A;
static constexpr uint32_t LAMP_RED = 0x5A1A16;
static constexpr uint32_t BIN_GREEN = 0x2F4A36;
static constexpr uint32_t BARK = 0x5E4632;
static constexpr std::array<uint32_t, 3> LEAVES = {0x4E7F36, 0x5D8C3A, 0x3F6E34};
static constexpr double METAL_GLOSS = 0.4;
static constexpr double PAINT_GLOSS = 0.6;
static constexpr double CAR_ROOF = 0.004;

/* A part built about its own foot, facing along x, stood on the map. */
static ModelMesh Placed(ModelMesh part, const MapVector &at, const MapVector &facing, double base)
{
	return part.Transform(Mat4::Translation({at.x, at.y, base}) * Mat4::Turn(UP, std::atan2(facing.y, facing.x)));
}

/* The lantern hangs from an arm reaching out over whatever the post faces. */
ModelMesh LampPost(const MapVector &at, const MapVector &facing, double base)
{
	ModelMesh post = Prism(6, POLE_RADIUS, POLE_HEIGHT).Paint(POLE).Gloss(METAL_GLOSS);
	post.Append(Box({0.0, -ARM_HALF, POLE_HEIGHT - ARM_HALF}, {ARM_REACH, ARM_HALF, POLE_HEIGHT + ARM_HALF}).Paint(POLE).Gloss(METAL_GLOSS));
	post.Append(Box(LANTERN_LOW, LANTERN_HIGH).Paint(LANTERN));
	return Placed(std::move(post), at, facing, base);
}

ModelMesh Bench(const MapVector &at, const MapVector &facing, double base)
{
	ModelMesh bench = Box(SEAT_LOW, SEAT_HIGH).Paint(TIMBER);
	bench.Append(Box(BACK_LOW, BACK_HIGH).Paint(TIMBER));
	bench.Append(Box(LEGS_LOW, LEGS_HIGH).Paint(IRON).Gloss(METAL_GLOSS));
	return Placed(std::move(bench), at, facing, base);
}

ModelMesh LitterBin(const MapVector &at, double base)
{
	ModelMesh bin = Prism(BIN_SIDES, BIN_RADIUS, BIN_HEIGHT).Paint(BIN_GREEN);
	bin.Append(Prism(BIN_SIDES, BIN_RADIUS * 1.15, BIN_LID).Transform(Mat4::Translation({0.0, 0.0, BIN_HEIGHT})).Paint(IRON));
	return Placed(std::move(bin), at, {1.0, 0.0}, base);
}

ModelMesh StreetTree(const MapVector &at, double base, uint32_t seed)
{
	ModelMesh tree = Prism(TRUNK_SIDES, TRUNK_RADIUS, TRUNK_HEIGHT).Paint(BARK);
	ModelMesh canopy = Icosphere(1).Displace(CANOPY_LUMPINESS, seed).Transform(Mat4::Translation({0.0, 0.0, TRUNK_HEIGHT + CANOPY_SPAN.z * 0.6}) * Mat4::Scaling(CANOPY_SPAN));
	tree.Append(canopy.Facet().Paint(LEAVES[seed % LEAVES.size()]).Vary(CANOPY_VARIETY, seed));
	return Placed(std::move(tree), at, {1.0, 0.0}, base);
}

/* A post with a pair of warning lamps facing the road, its striped boom raised beside it. */
ModelMesh CrossingBarrier(const MapVector &at, const MapVector &facing, double base)
{
	ModelMesh barrier = Box(POST_LOW, POST_HIGH).Paint(POST_WHITE);
	barrier.Append(Box(HOUSING_LOW, HOUSING_HIGH).Paint(IRON));
	for (double side : {-1.0, 1.0}) {
		double middle = side * LAMP_SPREAD;
		barrier.Append(Box({HOUSING_HIGH.x, middle - LAMP_HALF, LAMP_Z - LAMP_HALF}, {HOUSING_HIGH.x + 0.002, middle + LAMP_HALF, LAMP_Z + LAMP_HALF}).Paint(LAMP_RED).Gloss(METAL_GLOSS));
	}
	for (int band = 0; band < BOOM_BANDS; band++) {
		double low = BOOM_FOOT + band * BOOM_BAND;
		barrier.Append(Box({-BOOM_HALF, POST_HIGH.y, low}, {BOOM_HALF, POST_HIGH.y + 2.0 * BOOM_HALF, low + BOOM_BAND}).Paint(band % 2 == 0 ? WARNING_RED : POST_WHITE));
	}
	return Placed(std::move(barrier), at, facing, base);
}

ModelMesh ParkedCar(const MapVector &at, const MapVector &facing, double base, uint32_t paint)
{
	double half_length = CAR_LENGTH / 2.0;
	double half_width = CAR_WIDTH / 2.0;
	double cab_half_length = half_length * CAB_SHARE;
	double cab_half_width = half_width - CAB_INSET;
	double body_top = CAR_CLEARANCE + CAR_BODY_HEIGHT;
	double cab_top = body_top + CAB_HEIGHT;
	ModelMesh car = Box({-half_length, -half_width, CAR_CLEARANCE}, {half_length, half_width, body_top}).Paint(paint).Gloss(PAINT_GLOSS);
	car.Append(Box({-cab_half_length, -cab_half_width, body_top}, {cab_half_length, cab_half_width, cab_top - CAR_ROOF}).Paint(CAR_GLASS).Gloss(PAINT_GLOSS));
	car.Append(Box({-cab_half_length, -cab_half_width, cab_top - CAR_ROOF}, {cab_half_length, cab_half_width, cab_top}).Paint(paint).Gloss(PAINT_GLOSS));
	return Placed(std::move(car), at, facing, base);
}
