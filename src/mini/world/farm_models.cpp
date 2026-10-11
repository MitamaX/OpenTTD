/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file farm_models.cpp The pieces of the countryside scattered over open land: farmhouses, barns, silos, hay bales, fences and dry stone walls. */

#include "../../stdafx.h"
#include "farm_models.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "../core/seed.h"
#include "../map/building_form.h"
#include "../model/model_shapes.h"
#include "vehicle_models.h"

#include "../../safeguards.h"

static constexpr uint32_t WALLS = PaintworkTone(Paintwork::Primary, 1.0);
static constexpr uint32_t SHADED_WALLS = PaintworkTone(Paintwork::Primary, 0.8);
static constexpr uint32_t ROOF = PaintworkTone(Paintwork::Secondary, 1.0);
static constexpr uint32_t CHIMNEY = 0x7A6E66;
static constexpr uint32_t DOORS = 0x4A3A2C;
static constexpr uint32_t TIMBER = 0x6B5638;
static constexpr uint32_t STONE = 0xA49E90;
static constexpr int ROUND_SIDES = 10;
static constexpr double HALF_SIDE = 0.5;
static constexpr double FOOT_SINK = 0.025;
static constexpr Vec3 DEEP_BELOW = {0.0, 0.0, -10.0};
static constexpr double GROUND_LIGHTING = 0.5;

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
	return bale.Transform(Mat4::Translation({0.0, 0.0, RADIUS}) * Mat4::Turn(X_AXIS, QUARTER_TURN)).Paint(WALLS);
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

static constexpr int FENCE_POSTS = 7;
static constexpr double FENCE_POST = 0.014;
static constexpr double FENCE_HEIGHT = 0.055;
static constexpr double FENCE_RAIL = 0.009;
static constexpr std::array<double, 2> FENCE_RAIL_SHARES = {0.42, 0.82};
static constexpr double FENCE_RAIL_STANDOFF = 0.004;
static constexpr double FENCE_CAP_REACH = 0.003;
static constexpr double FENCE_CAP_HEIGHT = 0.005;
static constexpr double FENCE_POST_SHIFT = 0.2;
static constexpr double FENCE_LEAN = 0.006;
static constexpr SeedRange FENCE_TONES = {0.7, 1.15};
static constexpr uint32_t FENCE_SEED = 0xFE7CE;
static constexpr double GATE_POST = 0.026;
static constexpr double GATE_POST_HEIGHT = 0.075;
static constexpr double PIER_SIDE = 0.05;
static constexpr double PIER_HEIGHT = 0.06;
static constexpr double PIER_CAP_REACH = 0.006;
static constexpr double PIER_CAP_HEIGHT = 0.012;
static constexpr double PIER_CAP_SHADE = 1.1;

static ModelMesh &Leaned(ModelMesh &piece, double along, double across, double height)
{
	for (ModelVertex &vertex : piece.vertices) {
		double share = std::max(static_cast<double>(vertex.z), 0.0) / height;
		vertex.x += static_cast<float>(along * share);
		vertex.y += static_cast<float>(across * share);
	}
	return piece;
}

static ModelMesh FencePost(double x, double side, double height, SeedDice &dice)
{
	double half = side / 2.0;
	double cap = half + FENCE_CAP_REACH;
	ModelMesh post = Box({x - half, -half, -FOOT_SINK}, {x + half, half, height}, Capped::Neither);
	post.Append(Box({x - cap, -cap, height}, {x + cap, cap, height + FENCE_CAP_HEIGHT}, Capped::Top));
	post.Paint(ScaledRgb(TIMBER, dice.Between(FENCE_TONES)));
	double along = dice.Between(-FENCE_LEAN, FENCE_LEAN);
	double across = dice.Between(-FENCE_LEAN, FENCE_LEAN);
	return Leaned(post, along, across, height);
}

static ModelMesh GatePost()
{
	SeedDice dice(FENCE_SEED);
	return FencePost(0.0, GATE_POST, GATE_POST_HEIGHT, dice).Round(DEEP_BELOW, GROUND_LIGHTING);
}

/* Timber posts carrying two rails, lit half as the ground about them is so the fence reads as part of the ground. */
static ModelMesh Fence()
{
	SeedDice dice(FENCE_SEED);
	double spacing = 2.0 * HALF_SIDE / (FENCE_POSTS - 1);
	ModelMesh fence;
	for (int post = 0; post < FENCE_POSTS; post++) {
		bool end = post == 0 || post == FENCE_POSTS - 1;
		double shift = end ? 0.0 : dice.Between(-FENCE_POST_SHIFT, FENCE_POST_SHIFT) * spacing;
		fence.Append(FencePost(-HALF_SIDE + post * spacing + shift, FENCE_POST, FENCE_HEIGHT, dice));
	}
	double front = -FENCE_POST / 2.0 - FENCE_RAIL_STANDOFF;
	for (double share : FENCE_RAIL_SHARES) {
		double z = FENCE_HEIGHT * share;
		fence.Append(Box({-HALF_SIDE, front - FENCE_RAIL / 2.0, z - FENCE_RAIL / 2.0}, {HALF_SIDE, front + FENCE_RAIL / 2.0, z + FENCE_RAIL / 2.0}, Capped::Top).Paint(ScaledRgb(TIMBER, dice.Between(FENCE_TONES))));
	}
	return fence.Round(DEEP_BELOW, GROUND_LIGHTING);
}

/* One course of a dry stone wall: stones of uneven length and height, set a little off the line and tipped this way and that, narrowing toward the top. */
struct WallCourse {
	SeedRange length;
	SeedRange height;
	double half_foot;
	double half_top;
	double shade;
};

static constexpr std::array<WallCourse, 2> WALL_COURSES = {{
	{{0.07, 0.13}, {0.022, 0.03}, 0.022, 0.017, 0.8},
	{{0.04, 0.08}, {0.012, 0.02}, 0.016, 0.011, 1.0},
}};
static constexpr double WALL_SWAY = 0.006;
static constexpr double WALL_TIP = 0.004;
static constexpr SeedRange STONE_TONES = {0.65, 1.2};
static constexpr double STONE_GAP = 0.006;
static constexpr double FOOT_OPENNESS = 0.7;
static constexpr double WALL_HEIGHT = 0.04;
static constexpr double STONE_FACE_VARIETY = 0.08;

static void LayCourse(ModelMesh &wall, const WallCourse &course, double foot, double base, SeedDice &dice)
{
	double x0 = -HALF_SIDE - dice.Between(0.0, course.length.low);
	while (x0 < HALF_SIDE) {
		double x1 = std::min(x0 + dice.Between(course.length), HALF_SIDE);
		double start = std::max(x0, -HALF_SIDE) + STONE_GAP / 2.0;
		double end = x1 - STONE_GAP / 2.0;
		double sway = dice.Between(-WALL_SWAY, WALL_SWAY);
		double height = dice.Between(course.height);
		double tip = dice.Between(-WALL_TIP, WALL_TIP);
		std::array<MapVector, 4> outline = {MapVector{start, sway - course.half_foot}, MapVector{end, sway - course.half_foot}, MapVector{end, sway + course.half_foot}, MapVector{start, sway + course.half_foot}};
		ModelMesh stone = Extrusion(outline, height, Capped::Top);
		for (ModelVertex &vertex : stone.vertices) {
			if (vertex.z <= 0.0f) {
				vertex.z = static_cast<float>(foot);
				continue;
			}
			vertex.y = static_cast<float>(sway + (vertex.y - sway) * course.half_top / course.half_foot);
			vertex.z += static_cast<float>(base + tip * (vertex.x - (start + end) / 2.0) / (end - start));
		}
		wall.Append(stone.Facet().Paint(ScaledRgb(STONE, course.shade * dice.Between(STONE_TONES))).Vary(STONE_FACE_VARIETY, dice.Next()));
		x0 = x1;
	}
}

/* Dry stone laid along a tile's side in two courses, big stones below and smaller ones on top, so its line breaks up against the sky; the grass about its foot keeps the light off it there. */
static ModelMesh StoneWall()
{
	SeedDice dice(0x5704E);
	ModelMesh wall;
	double foot = -FOOT_SINK;
	double base = 0.0;
	for (const WallCourse &course : WALL_COURSES) {
		LayCourse(wall, course, foot, base, dice);
		base += course.height.low;
		foot = base;
	}
	wall.Round(DEEP_BELOW, GROUND_LIGHTING);
	return wall.Occlude(UpwardOpenness{0.0, WALL_HEIGHT, FOOT_OPENNESS, 1.0});
}

static ModelMesh WallPier()
{
	double half = PIER_SIDE / 2.0;
	double cap = half + PIER_CAP_REACH;
	ModelMesh pier = Box({-half, -half, -FOOT_SINK}, {half, half, PIER_HEIGHT}, Capped::Neither).Paint(STONE);
	pier.Append(Box({-cap, -cap, PIER_HEIGHT}, {cap, cap, PIER_HEIGHT + PIER_CAP_HEIGHT}, Capped::Top).Paint(ScaledRgb(STONE, PIER_CAP_SHADE)));
	pier.Facet().Round(DEEP_BELOW, GROUND_LIGHTING);
	return pier.Occlude(UpwardOpenness{0.0, PIER_HEIGHT, FOOT_OPENNESS, 1.0});
}

std::vector<ModelMesh> BuildFarmModels()
{
	std::vector<ModelMesh> pieces(FARM_PIECES);
	pieces[to_underlying(FarmPiece::House)] = House();
	pieces[to_underlying(FarmPiece::Barn)] = Barn();
	pieces[to_underlying(FarmPiece::Silo)] = Silo();
	pieces[to_underlying(FarmPiece::RoundBale)] = RoundBale();
	pieces[to_underlying(FarmPiece::BaleStack)] = BaleStack();
	return pieces;
}

std::vector<ModelMesh> BuildBoundaryModels()
{
	std::vector<ModelMesh> pieces(BOUNDARY_PIECES);
	pieces[to_underlying(BoundaryPiece::Fence)] = Fence();
	pieces[to_underlying(BoundaryPiece::StoneWall)] = StoneWall();
	pieces[to_underlying(BoundaryPiece::GatePost)] = GatePost();
	pieces[to_underlying(BoundaryPiece::WallPier)] = WallPier();
	return pieces;
}
