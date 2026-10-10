/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file road_vehicle_models.cpp Low poly buses, trams and lorries with a body for each kind of cargo. */

#include "../../stdafx.h"
#include "vehicle_parts.h"

#include <array>
#include <cmath>
#include <numbers>
#include <span>

#include "way_shapes.h"

#include "../../safeguards.h"

static constexpr double HALF_WIDTH = 0.07;
static constexpr double WHEEL_RADIUS = 0.024;
static constexpr double WHEEL_WIDTH = 0.02;
static constexpr double TYRE_PROUD = 0.003;
static constexpr double HALF_TRACK = HALF_WIDTH + TYRE_PROUD - WHEEL_WIDTH * 0.5;
static constexpr double TRAM_HALF_TRACK = 0.058;
static constexpr double FRONT = 0.215;
static constexpr double CHASSIS_LOW = 0.026;
static constexpr double CAB_BACK = 0.11;
static constexpr double LOAD_FRONT = 0.1;
static constexpr double LOAD_CLEARANCE = 0.006;
static constexpr double LOAD_FLOOR = WHEEL_RADIUS * 2.0 + LOAD_CLEARANCE;
static constexpr double LAMP_HALF = 0.007;
static constexpr double LAMP_HEIGHT = 0.05;
static constexpr double TRAM_LAMP_HEIGHT = 0.06;
static constexpr double TAIL_LAMP_RISE = 0.01;
static constexpr double HEADLAMP_INSET = 0.016;
static constexpr double TAIL_LAMP_INSET = 0.012;
static constexpr double HUB_SHARE = 0.55;
static constexpr double HUB_DEPTH = 0.003;
static constexpr double ARCH_RADIUS = WHEEL_RADIUS * 1.3;
static constexpr int ARCH_SIDES = 4;
static constexpr double ARCH_GAP = 0.0005;
static constexpr double ARCH_DEPTH = 0.001;
static constexpr double TANK_RISE = 0.055;
static constexpr double MUDGUARD = 0.003;
static constexpr double UNDERBODY_LIFT = 0.004;
static constexpr double UNDERBODY_GAP = 0.003;
static constexpr double UNDERBODY_INSET = 0.03;

struct Axle {
	double x;
	bool arched;
};

static constexpr std::array<Axle, 2> BUS_AXLES = {{{-0.1, true}, {0.12, true}}};
static constexpr std::array<Axle, 3> LORRY_AXLES = {{{-0.16, false}, {-0.1, false}, {0.16, true}}};

static constexpr HullProfile BUS_BODY = {HALF_WIDTH, CHASSIS_LOW, 0.165, 0.024};
static constexpr BodyEnd BUS_ENDS = RoundedEnd(0.035, 0.84, 0.96);
static constexpr Band BUS_GLASS = {0.098, 0.14};
static constexpr Band BUS_WINDSCREEN = {0.084, 0.141};
static constexpr Band BUS_BAND = {0.068, 0.082};
static constexpr HullProfile CAB_BODY = {HALF_WIDTH, CHASSIS_LOW, 0.15, 0.02};
static constexpr BodyEnd CAB_NOSE = RoundedEnd(0.025, 0.85, 0.86);
static constexpr Band CAB_GLASS = {0.098, 0.13};
static constexpr double CAB_GLASS_BACK = CAB_BACK + 0.03;
static constexpr HullProfile BOX_BODY = {HALF_WIDTH + 0.002, LOAD_FLOOR, 0.17, 0.01};
static constexpr HullProfile LIVESTOCK_BODY = {HALF_WIDTH + 0.002, LOAD_FLOOR, 0.165, 0.01};
static constexpr BodyEnd BOX_ENDS = RoundedEnd(0.008, 0.94, 0.97);
static constexpr Band BOX_BAND = {0.11, 0.135};
static constexpr HullProfile TRAM_BODY = {HALF_WIDTH + 0.005, CHASSIS_LOW + 0.01, 0.19, 0.03};
static constexpr BodyEnd TRAM_ENDS = RoundedEnd(0.035, 0.85, 0.9);
static constexpr Band TRAM_GLASS = {0.105, 0.16};
static constexpr Band TRAM_BAND = {CHASSIS_LOW + 0.01, 0.085};

static void HeadLamps(VehicleKit &kit, const BodyRun &nose, double z)
{
	kit.Fine(LampPair(nose.Front(), nose.HalfWidthAt(nose.Front()) - HEADLAMP_INSET, z, LAMP_HALF, HEADLAMP));
}

static void TailLamps(VehicleKit &kit, double x, double half_width, double z)
{
	kit.Fine(LampPair(x, half_width - TAIL_LAMP_INSET, z, LAMP_HALF, TAIL_LAMP));
}

static void BodyLamps(VehicleKit &kit, const BodyRun &body, double z)
{
	HeadLamps(kit, body, z);
	TailLamps(kit, body.Back(), body.HalfWidthAt(body.Back()), z + TAIL_LAMP_RISE);
}

static ModelMesh WheelArch(double x)
{
	std::array<MapVector, ARCH_SIDES + 1> outline;
	for (int side = 0; side <= ARCH_SIDES; side++) {
		double angle = std::numbers::pi * side / ARCH_SIDES;
		outline[side] = {x + ARCH_RADIUS * std::cos(angle), WHEEL_RADIUS + ARCH_RADIUS * std::sin(angle)};
	}
	ModelMesh arch = Upright(outline, ARCH_DEPTH).Move({0.0, HALF_WIDTH + ARCH_GAP + ARCH_DEPTH * 0.5, 0.0}).Paint(RUNNING_GEAR);
	return arch.Append(Mirrored(arch));
}

static void Wheels(VehicleKit &kit, std::span<const Axle> axles)
{
	for (const Axle &axle : axles) {
		kit.Fine(WheelPair(axle.x, HALF_TRACK, WHEEL_RADIUS, WHEEL_WIDTH));
		ModelMesh hub = Hub(axle.x, HALF_TRACK + WHEEL_WIDTH * 0.5 + HUB_DEPTH * 0.5, WHEEL_RADIUS, WHEEL_RADIUS * HUB_SHARE, HUB_DEPTH);
		kit.Fine(hub.Append(Mirrored(hub)));
		if (axle.arched) kit.Fine(WheelArch(axle.x));
	}
}

static ModelMesh Mudguard(double back_axle, double front_axle)
{
	ModelMesh guard = Plank({back_axle - WHEEL_RADIUS - MUDGUARD, HALF_TRACK - WHEEL_WIDTH * 0.5, LOAD_FLOOR - MUDGUARD}, {front_axle + WHEEL_RADIUS + MUDGUARD, HALF_TRACK + WHEEL_WIDTH * 0.5, LOAD_FLOOR}, TRIM);
	return guard.Append(Mirrored(guard));
}

static void Underbody(VehicleKit &kit, double back, double front)
{
	double half = HALF_TRACK - WHEEL_WIDTH * 0.5 - UNDERBODY_GAP;
	kit.Body(Plank({back + UNDERBODY_INSET, -half, UNDERBODY_LIFT}, {front - UNDERBODY_INSET, half, CHASSIS_LOW}, RUNNING_GEAR));
}

static ModelMesh Bus(VehicleDetail detail)
{
	VehicleKit kit(detail);
	BodyRun body(BUS_BODY, -FRONT, FRONT, BUS_ENDS, BUS_ENDS, kit.Steps());
	kit.Body(body.Shell().Paint(PRIMARY).Gloss(PAINT_GLOSS));
	double windscreen = body.Front() - BUS_ENDS.reach;
	kit.Body(body.Stripe(BUS_GLASS, body.Back(), windscreen, GLASS).Gloss(GLASS_GLOSS));
	kit.Body(body.Stripe(BUS_WINDSCREEN, windscreen, body.Front(), GLASS).Gloss(GLASS_GLOSS));
	kit.Body(body.Stripe(BUS_BAND, SECONDARY).Gloss(PAINT_GLOSS));
	kit.Fine(Plank({-0.12, -0.04, 0.165}, {-0.02, 0.04, 0.178}, ROOF_GREY));
	Underbody(kit, body.Back(), body.Front());
	Wheels(kit, BUS_AXLES);
	BodyLamps(kit, body, LAMP_HEIGHT);
	return kit.Done();
}

/* A lorry's chassis, wheels and cab, which every load body sits behind. */
static void Lorry(VehicleKit &kit)
{
	kit.Body(Plank({-FRONT, -0.05, CHASSIS_LOW}, {CAB_BACK, 0.05, LOAD_FLOOR}, RUNNING_GEAR));
	BodyRun cab(CAB_BODY, CAB_BACK, FRONT, SQUARE_END, CAB_NOSE, kit.Steps());
	kit.Body(cab.Shell().Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(cab.Stripe(CAB_GLASS, CAB_GLASS_BACK, FRONT, GLASS).Gloss(GLASS_GLOSS));
	Underbody(kit, -FRONT, FRONT);
	Wheels(kit, LORRY_AXLES);
	kit.Fine(Mudguard(LORRY_AXLES[0].x, LORRY_AXLES[1].x));
	HeadLamps(kit, cab, LAMP_HEIGHT);
	TailLamps(kit, -FRONT, HALF_WIDTH, LAMP_HEIGHT + TAIL_LAMP_RISE);
}

static ModelMesh BoxTruck(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Lorry(kit);
	BodyRun box(BOX_BODY, -FRONT, LOAD_FRONT, BOX_ENDS, BOX_ENDS, kit.Steps());
	kit.Body(box.Shell().Paint(CREAM).Gloss(PAINT_GLOSS));
	kit.Body(box.Stripe(BOX_BAND, PRIMARY).Gloss(PAINT_GLOSS));
	return kit.Done();
}

static ModelMesh TankerTruck(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Lorry(kit);
	std::array<LatheRing, 4> tank = {{{0.04, -0.21}, {0.058, -0.195}, {0.058, 0.085}, {0.04, 0.1}}};
	kit.Body(Turned(tank, 10, LOAD_FLOOR + TANK_RISE).Paint(STEEL).Gloss(METAL_GLOSS));
	kit.Body(Barrel(-0.07, -0.03, 0.06, LOAD_FLOOR + TANK_RISE, 10).Paint(PRIMARY).Gloss(PAINT_GLOSS));
	return kit.Done();
}

static ModelMesh OpenBin(uint32_t tone)
{
	ModelMesh side = Plank({-FRONT, HALF_WIDTH - 0.008, LOAD_FLOOR}, {LOAD_FRONT, HALF_WIDTH, 0.12}, tone);
	side.Append(Mirrored(side));
	for (double x : {-FRONT, LOAD_FRONT - 0.008}) side.Append(Plank({x, -HALF_WIDTH, LOAD_FLOOR}, {x + 0.008, HALF_WIDTH, 0.12}, tone));
	return side.Gloss(PAINT_GLOSS);
}

static ModelMesh TipperTruck(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Lorry(kit);
	kit.Body(OpenBin(MUTED_PRIMARY));
	kit.Body(Plank({-FRONT + 0.008, -HALF_WIDTH + 0.008, LOAD_FLOOR}, {LOAD_FRONT - 0.008, HALF_WIDTH - 0.008, 0.1}, CARGO));
	std::array<LatheRing, 3> mound = {{{1.0, 0.0}, {0.6, 0.6}, {0.0, 1.0}}};
	kit.Body(Lathe(mound, 6).Transform(Mat4::Translation({-0.055, 0.0, 0.1}) * Mat4::Scaling({0.14, 0.055, 0.035})).Facet().Paint(CARGO));
	return kit.Done();
}

static ModelMesh FlatbedTruck(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Lorry(kit);
	kit.Body(Plank({-FRONT, -HALF_WIDTH, LOAD_FLOOR}, {LOAD_FRONT, HALF_WIDTH, LOAD_FLOOR + 0.01}, TIMBER_DECK));
	for (double y : {-0.035, 0.035}) kit.Body(Barrel(-0.2, 0.09, 0.03, LOAD_FLOOR + 0.04, 7).Transform(Mat4::Translation({0.0, y, 0.0})).Paint(CARGO));
	kit.Fine(Barrel(-0.19, 0.08, 0.028, LOAD_FLOOR + 0.09, 7).Paint(BRIGHT_CARGO));
	kit.Fine(Plank({LOAD_FRONT - 0.01, -HALF_WIDTH, LOAD_FLOOR}, {LOAD_FRONT, HALF_WIDTH, 0.15}, DARK_PRIMARY).Gloss(PAINT_GLOSS));
	return kit.Done();
}

static ModelMesh LivestockTruck(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Lorry(kit);
	kit.Body(BodyRun(LIVESTOCK_BODY, -FRONT, LOAD_FRONT, BOX_ENDS, BOX_ENDS, kit.Steps()).Shell().Paint(MUTED_PRIMARY).Gloss(PAINT_GLOSS));
	for (double z : {0.075, 0.11, 0.14}) kit.Fine(SideBand(-0.205, LOAD_FRONT - 0.01, HALF_WIDTH + 0.002, z, z + 0.01, SLAT_GAP));
	return kit.Done();
}

static ModelMesh Tram(VehicleDetail detail)
{
	VehicleKit kit(detail);
	BodyRun body(TRAM_BODY, -UNIT_REACH, UNIT_REACH, TRAM_ENDS, TRAM_ENDS, kit.Steps());
	kit.Body(body.Shell().Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(body.Stripe(TRAM_BAND, SECONDARY).Gloss(PAINT_GLOSS));
	kit.Body(body.Stripe(TRAM_GLASS, GLASS).Gloss(GLASS_GLOSS));
	for (double x : {-0.15, 0.15}) kit.Fine(Bogie(x, TRAM_HALF_TRACK, 0.018));
	kit.Fine(Plank({-0.03, -0.03, 0.19}, {0.03, 0.03, 0.2}, PANTOGRAPH));
	for (double side : {-0.025, 0.025}) {
		kit.Fine(Strut({-0.02, side, 0.2}, {0.03, side, 0.235}, 0.003).Paint(PANTOGRAPH));
		kit.Fine(Strut({0.03, side, 0.235}, {0.0, side, 0.27}, 0.003).Paint(PANTOGRAPH));
	}
	kit.Fine(Plank({-0.005, -0.05, 0.268}, {0.005, 0.05, 0.274}, PANTOGRAPH));
	BodyLamps(kit, body, TRAM_LAMP_HEIGHT);
	return kit.Done();
}

ModelMesh RoadVehicleModel(VehicleLook look, VehicleDetail detail)
{
	switch (look) {
		case VehicleLook::Bus: return Bus(detail);
		case VehicleLook::BoxTruck: return BoxTruck(detail);
		case VehicleLook::TankerTruck: return TankerTruck(detail);
		case VehicleLook::TipperTruck: return TipperTruck(detail);
		case VehicleLook::FlatbedTruck: return FlatbedTruck(detail);
		case VehicleLook::LivestockTruck: return LivestockTruck(detail);
		default: return Tram(detail);
	}
}
