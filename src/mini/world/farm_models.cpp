/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file farm_models.cpp The pieces of the countryside scattered over open land: farmhouses, barns, silos, hay bales, fences and dry stone walls. */

#include "../../stdafx.h"
#include "farm_models.h"

#include <array>
#include <cmath>

#include "../core/seed.h"
#include "../model/model_shapes.h"
#include "vehicle_models.h"

#include "../../safeguards.h"

static constexpr uint32_t WALLS = PaintworkTone(Paintwork::Primary, 1.0);
static constexpr uint32_t SHADED_WALLS = PaintworkTone(Paintwork::Primary, 0.8);
static constexpr uint32_t ROOF = PaintworkTone(Paintwork::Secondary, 1.0);
static constexpr uint32_t CHIMNEY = 0x7A6E66;
static constexpr uint32_t DOORS = 0x4A3A2C;
static constexpr uint32_t TIMBER = 0x6B5638;
static constexpr uint32_t STONE = 0x8C887E;
static constexpr int ROUND_SIDES = 10;
static constexpr double HALF_SIDE = 0.5;

/* A building's body: four walls under a roof running along x, the roof ridged over the middle and its eaves standing out past the walls. */
struct Shed {
	double length;
	double width;
	double walls;
	double rise;
	double overhang;
};

static constexpr Shed HOUSE = {0.34, 0.19, 0.14, 0.1, 0.02};
static constexpr Shed BARN = {0.46, 0.26, 0.17, 0.12, 0.025};

/* A pitched roof's two slopes and its gable ends, faceted so each face shades apart. */
static ModelMesh GableRoof(const Shed &shed)
{
	double x = shed.length / 2.0 + shed.overhang;
	double y = shed.width / 2.0 + shed.overhang;
	double eave = shed.walls - shed.overhang * shed.rise / (shed.width / 2.0);
	double ridge = shed.walls + shed.rise;
	ModelMesh roof;
	std::array<uint32_t, 6> corners = {
		roof.Point({-x, -y, eave}), roof.Point({x, -y, eave}), roof.Point({x, 0.0, ridge}),
		roof.Point({-x, 0.0, ridge}), roof.Point({-x, y, eave}), roof.Point({x, y, eave}),
	};
	roof.Quad(corners[0], corners[1], corners[2], corners[3]);
	roof.Quad(corners[3], corners[2], corners[5], corners[4]);
	roof.Paint(ROOF);

	ModelMesh gables;
	double wall_x = shed.length / 2.0;
	double half_width = shed.width / 2.0;
	for (double end : {-wall_x, wall_x}) gables.Triangle(gables.Point({end, -half_width, shed.walls}), gables.Point({end, half_width, shed.walls}), gables.Point({end, 0.0, ridge}));
	gables.Paint(WALLS);
	return roof.Append(gables).Facet();
}

static ModelMesh ShedBody(const Shed &shed)
{
	ModelMesh body = Box({-shed.length / 2.0, -shed.width / 2.0, 0.0}, {shed.length / 2.0, shed.width / 2.0, shed.walls}).Paint(WALLS);
	return body.Append(GableRoof(shed));
}

/* A farmhouse: a chimney at one gable and a door on its long side. */
static ModelMesh House()
{
	ModelMesh house = ShedBody(HOUSE);
	double chimney_x = HOUSE.length / 2.0 - 0.04;
	house.Append(Box({chimney_x - 0.02, -0.02, HOUSE.walls}, {chimney_x + 0.02, 0.02, HOUSE.walls + HOUSE.rise + 0.04}).Paint(CHIMNEY));
	house.Append(Box({-0.025, -HOUSE.width / 2.0 - 0.004, 0.0}, {0.025, -HOUSE.width / 2.0, 0.08}).Paint(DOORS));
	return house;
}

/* A barn with tall doors in either gable end and a lean-to along one side. */
static ModelMesh Barn()
{
	ModelMesh barn = ShedBody(BARN);
	constexpr double DOOR_DEPTH = 0.004;
	double gable = BARN.length / 2.0;
	barn.Append(Box({-gable - DOOR_DEPTH, -0.07, 0.0}, {gable + DOOR_DEPTH, 0.07, 0.13}).Paint(DOORS));
	Shed lean_to = {BARN.length * 0.6, 0.1, 0.09, 0.03, 0.012};
	ModelMesh side = Box({-lean_to.length / 2.0, -lean_to.width / 2.0, 0.0}, {lean_to.length / 2.0, lean_to.width / 2.0, lean_to.walls}).Paint(SHADED_WALLS);
	side.Append(Box({-lean_to.length / 2.0 - lean_to.overhang, -lean_to.width / 2.0 - lean_to.overhang, lean_to.walls}, {lean_to.length / 2.0 + lean_to.overhang, lean_to.width / 2.0, lean_to.walls + lean_to.rise}).Paint(ROOF));
	return barn.Append(side.Transform(Mat4::Translation({-0.05, BARN.width / 2.0 + lean_to.width / 2.0, 0.0})));
}

static ModelMesh Silo()
{
	static constexpr std::array<LatheRing, 5> PROFILE = {{{0.055, 0.0}, {0.055, 0.36}, {0.047, 0.39}, {0.025, 0.41}, {0.0, 0.415}}};
	return Lathe(PROFILE, ROUND_SIDES).Paint(WALLS);
}

/* A round bale lies on its side, its wound face to the sky. */
static ModelMesh RoundBale()
{
	constexpr double RADIUS = 0.028;
	constexpr double WIDTH = 0.038;
	ModelMesh bale = Column(ROUND_SIDES, RADIUS, RADIUS, WIDTH).Transform(Mat4::Translation({0.0, 0.0, -WIDTH / 2.0}));
	return bale.Transform(Mat4::Translation({0.0, 0.0, RADIUS}) * Mat4::Turn({1.0, 0.0, 0.0}, M_PI / 2.0)).Paint(WALLS);
}

/* Square bales stacked two high, the top course turned across the bottom one. */
static ModelMesh BaleStack()
{
	constexpr Vec3 BALE = {0.06, 0.03, 0.026};
	ModelMesh stack;
	for (int column = 0; column < 3; column++) {
		double y = (column - 1) * BALE.y;
		stack.Append(Box({-BALE.x / 2.0, y - BALE.y / 2.0, 0.0}, {BALE.x / 2.0, y + BALE.y / 2.0, BALE.z}));
	}
	for (int row = 0; row < 2; row++) {
		double x = (row - 0.5) * BALE.y;
		stack.Append(Box({x - BALE.y / 2.0, -BALE.x / 2.0 * 1.4, BALE.z}, {x + BALE.y / 2.0, BALE.x / 2.0 * 1.4, 2.0 * BALE.z}));
	}
	return stack.Paint(WALLS);
}

/* Timber posts along a tile's side carrying two rails. */
static ModelMesh Fence()
{
	constexpr int POSTS = 5;
	constexpr double POST = 0.01;
	constexpr double HEIGHT = 0.05;
	constexpr double RAIL = 0.006;
	ModelMesh fence;
	for (int post = 0; post < POSTS; post++) {
		double x = -HALF_SIDE + post * (2.0 * HALF_SIDE) / (POSTS - 1);
		fence.Append(Box({x - POST / 2.0, -POST / 2.0, 0.0}, {x + POST / 2.0, POST / 2.0, HEIGHT}));
	}
	for (double z : {HEIGHT * 0.45, HEIGHT * 0.85}) fence.Append(Box({-HALF_SIDE, -RAIL / 2.0, z - RAIL / 2.0}, {HALF_SIDE, RAIL / 2.0, z + RAIL / 2.0}));
	return fence.Paint(TIMBER);
}

/* Dry stone laid along a tile's side in blocks of uneven height, a little wider at the foot. */
static ModelMesh StoneWall()
{
	constexpr int BLOCKS = 10;
	constexpr double HALF_FOOT = 0.02;
	constexpr double HALF_TOP = 0.013;
	constexpr SeedRange HEIGHT = {0.036, 0.048};
	SeedDice dice(0x5704E);
	ModelMesh wall;
	double length = 2.0 * HALF_SIDE / BLOCKS;
	for (int block = 0; block < BLOCKS; block++) {
		double x0 = -HALF_SIDE + block * length;
		double height = dice.Between(HEIGHT);
		std::array<MapVector, 4> foot = {MapVector{x0, -HALF_FOOT}, MapVector{x0 + length, -HALF_FOOT}, MapVector{x0 + length, HALF_FOOT}, MapVector{x0, HALF_FOOT}};
		ModelMesh stone = Extrusion(foot, height);
		for (ModelVertex &vertex : stone.vertices) {
			if (vertex.z > 0.0f) vertex.y *= static_cast<float>(HALF_TOP / HALF_FOOT);
		}
		wall.Append(stone.Facet().Paint(STONE).Vary(0.12, dice.Next()));
	}
	return wall;
}

std::vector<ModelMesh> BuildFarmModels()
{
	std::vector<ModelMesh> pieces(FARM_PIECES);
	pieces[to_underlying(FarmPiece::House)] = House();
	pieces[to_underlying(FarmPiece::Barn)] = Barn();
	pieces[to_underlying(FarmPiece::Silo)] = Silo();
	pieces[to_underlying(FarmPiece::RoundBale)] = RoundBale();
	pieces[to_underlying(FarmPiece::BaleStack)] = BaleStack();
	pieces[to_underlying(FarmPiece::Fence)] = Fence();
	pieces[to_underlying(FarmPiece::StoneWall)] = StoneWall();
	return pieces;
}
