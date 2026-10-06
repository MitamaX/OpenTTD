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

#include "way_shapes.h"

#include "../../safeguards.h"

static constexpr double HALF_WIDTH = 0.07;
static constexpr double HALF_TRACK = 0.058;
static constexpr double WHEEL_RADIUS = 0.024;
static constexpr double WHEEL_WIDTH = 0.02;
static constexpr double FRONT = 0.215;
static constexpr double CHASSIS_LOW = 0.026;
static constexpr double CAB_BACK = 0.11;
static constexpr double LOAD_FRONT = 0.1;
static constexpr double LOAD_FLOOR = 0.045;
static constexpr double LAMP_HALF = 0.007;

static void Lamps(VehicleKit &kit, double back, double front, double z)
{
	kit.Fine(LampPair(front, HALF_WIDTH - 0.018, z, LAMP_HALF, HEADLAMP));
	kit.Fine(LampPair(back, HALF_WIDTH - 0.014, z + 0.01, LAMP_HALF, TAIL_LAMP));
}

static ModelMesh Bus(VehicleDetail detail)
{
	VehicleKit kit(detail);
	kit.Body(Hull({HALF_WIDTH, CHASSIS_LOW, 0.165, 0.025}, -FRONT, FRONT).Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(SideBand(-0.2, 0.19, HALF_WIDTH, 0.1, 0.15, GLASS).Gloss(GLASS_GLOSS));
	kit.Body(SideBand(-FRONT, FRONT, HALF_WIDTH, 0.068, 0.082, SECONDARY));
	kit.Fine(Windscreen(FRONT, HALF_WIDTH - 0.008, 0.09, 0.155));
	kit.Fine(Windscreen(-FRONT, HALF_WIDTH - 0.014, 0.11, 0.15));
	kit.Fine(Plank({-0.12, -0.04, 0.165}, {-0.02, 0.04, 0.178}, ROOF_GREY));
	for (double x : {-0.14, 0.14}) kit.Fine(WheelPair(x, HALF_TRACK, WHEEL_RADIUS, WHEEL_WIDTH));
	Lamps(kit, -FRONT, FRONT, 0.05);
	return kit.Done();
}

/* A lorry's chassis, wheels and cab, which every load body sits behind. */
static void Lorry(VehicleKit &kit)
{
	kit.Body(Plank({-FRONT, -0.05, CHASSIS_LOW}, {CAB_BACK, 0.05, LOAD_FLOOR}, RUNNING_GEAR));
	kit.Body(Hull({HALF_WIDTH, CHASSIS_LOW, 0.15, 0.02}, CAB_BACK, FRONT).Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Fine(Windscreen(FRONT, HALF_WIDTH - 0.008, 0.1, 0.14));
	kit.Fine(WindowRow(CAB_BACK + 0.04, FRONT - 0.015, 0.1, 0.138, HALF_WIDTH, 1));
	for (double x : {-0.16, -0.1, 0.16}) kit.Fine(WheelPair(x, HALF_TRACK, WHEEL_RADIUS, WHEEL_WIDTH));
	Lamps(kit, -FRONT, FRONT, 0.05);
}

static ModelMesh BoxTruck(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Lorry(kit);
	kit.Body(Hull({HALF_WIDTH + 0.002, LOAD_FLOOR, 0.17, 0.008}, -FRONT, LOAD_FRONT).Paint(CREAM));
	kit.Body(SideBand(-FRONT, LOAD_FRONT, HALF_WIDTH + 0.002, 0.11, 0.135, PRIMARY));
	return kit.Done();
}

static ModelMesh TankerTruck(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Lorry(kit);
	std::array<LatheRing, 4> tank = {{{0.04, -0.21}, {0.058, -0.195}, {0.058, 0.085}, {0.04, 0.1}}};
	kit.Body(Turned(tank, 10, 0.1).Paint(STEEL).Gloss(METAL_GLOSS));
	kit.Body(Barrel(-0.07, -0.03, 0.06, 0.1, 10).Paint(PRIMARY));
	return kit.Done();
}

static ModelMesh OpenBin(uint32_t tone)
{
	ModelMesh side = Plank({-FRONT, HALF_WIDTH - 0.008, LOAD_FLOOR}, {LOAD_FRONT, HALF_WIDTH, 0.12}, tone);
	side.Append(Mirrored(side));
	for (double x : {-FRONT, LOAD_FRONT - 0.008}) side.Append(Plank({x, -HALF_WIDTH, LOAD_FLOOR}, {x + 0.008, HALF_WIDTH, 0.12}, tone));
	return side;
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
	kit.Fine(Plank({LOAD_FRONT - 0.01, -HALF_WIDTH, LOAD_FLOOR}, {LOAD_FRONT, HALF_WIDTH, 0.15}, DARK_PRIMARY));
	return kit.Done();
}

static ModelMesh LivestockTruck(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Lorry(kit);
	kit.Body(Hull({HALF_WIDTH + 0.002, LOAD_FLOOR, 0.165, 0.008}, -FRONT, LOAD_FRONT).Paint(MUTED_PRIMARY));
	for (double z : {0.075, 0.11, 0.14}) kit.Fine(SideBand(-0.205, LOAD_FRONT - 0.01, HALF_WIDTH + 0.002, z, z + 0.01, SLAT_GAP));
	return kit.Done();
}

static ModelMesh Tram(VehicleDetail detail)
{
	VehicleKit kit(detail);
	kit.Body(Hull({HALF_WIDTH + 0.005, CHASSIS_LOW + 0.01, 0.19, 0.03}, -UNIT_REACH, UNIT_REACH).Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(SideBand(-UNIT_REACH, UNIT_REACH, HALF_WIDTH + 0.005, CHASSIS_LOW + 0.01, 0.085, SECONDARY));
	kit.Body(SideBand(-0.21, 0.21, HALF_WIDTH + 0.005, 0.105, 0.165, GLASS).Gloss(GLASS_GLOSS));
	kit.Fine(Windscreen(UNIT_REACH, HALF_WIDTH - 0.005, 0.1, 0.17));
	kit.Fine(Windscreen(-UNIT_REACH, HALF_WIDTH - 0.005, 0.1, 0.17));
	for (double x : {-0.15, 0.15}) kit.Fine(Bogie(x, HALF_TRACK, 0.018));
	kit.Fine(Plank({-0.03, -0.03, 0.19}, {0.03, 0.03, 0.2}, PANTOGRAPH));
	for (double side : {-0.025, 0.025}) {
		kit.Fine(Strut({-0.02, side, 0.2}, {0.03, side, 0.235}, 0.003).Paint(PANTOGRAPH));
		kit.Fine(Strut({0.03, side, 0.235}, {0.0, side, 0.27}, 0.003).Paint(PANTOGRAPH));
	}
	kit.Fine(Plank({-0.005, -0.05, 0.268}, {0.005, 0.05, 0.274}, PANTOGRAPH));
	Lamps(kit, -UNIT_REACH, UNIT_REACH, 0.06);
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
