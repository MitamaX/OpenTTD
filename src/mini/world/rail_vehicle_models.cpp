/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rail_vehicle_models.cpp Low poly locomotives of every traction, coaches, vans and wagons for each kind of cargo. */

#include "../../stdafx.h"
#include "vehicle_parts.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "way_shapes.h"

#include "../../safeguards.h"

static constexpr double HALF_GAUGE = 0.105;
static constexpr double WHEEL_RADIUS = 0.026;
static constexpr double BOGIE_X = 0.15;
static constexpr double FLOOR = 0.062;
static constexpr double BODY_HALF = 0.13;
static constexpr double COACH_TOP = 0.215;
static constexpr double ROOF_TOP = 0.248;
static constexpr double WIRE_REACH = 0.286;
static constexpr double STRUT_HALF = 0.0035;
static constexpr double HEAP_SPREAD = 0.004;
static constexpr uint32_t HEAP_SEED = 0x4EA9;
static constexpr uint32_t BRASS = 0xC8A040;

static constexpr HullProfile COACH_BODY = {BODY_HALF, FLOOR, COACH_TOP, 0.0};
static constexpr HullProfile COACH_ROOF = {BODY_HALF - 0.002, COACH_TOP, ROOF_TOP, 0.03};
static constexpr HullProfile VAN_BODY = {BODY_HALF - 0.005, FLOOR, COACH_TOP, 0.012};
static constexpr HullProfile STREAMLINED_BODY = {BODY_HALF, 0.07, 0.24, 0.05};
static constexpr double STREAMLINED_GLASS_FOOT = 0.17;
static constexpr Band STREAMLINED_BAND = {0.1, 0.125};
static constexpr HullProfile SLEEK_BODY = {0.12, 0.0, 0.17, 0.04};
static constexpr Band SLEEK_GLASS = {0.085, 0.13};
static constexpr Band SLEEK_BAND = {0.04, 0.055};
static constexpr HullProfile ELECTRIC_BODY = {BODY_HALF, 0.07, 0.235, 0.02};
static constexpr BodyEnd ELECTRIC_ENDS = RoundedEnd(0.03, 0.9, 0.86);
static constexpr Band ELECTRIC_BAND = {0.1, 0.122};
static constexpr WindowSpan ELECTRIC_WINDOWS = {-0.17, 0.17, 5};
static constexpr Band ELECTRIC_GLASS = {0.15, 0.2};
static constexpr Band ELECTRIC_CAB_GLASS = {0.165, 0.215};
static constexpr HullProfile LONG_HOOD = {0.085, 0.075, 0.2, 0.02};
static constexpr HullProfile SHORT_HOOD = {0.085, 0.075, 0.17, 0.02};
static constexpr BodyEnd HOOD_END = RoundedEnd(0.015, 0.9, 0.92);
static constexpr Band HOOD_BAND = {0.1, 0.12};
static constexpr HullProfile HOOD_CAB = {0.122, 0.075, 0.25, 0.02};
static constexpr BodyEnd HOOD_CAB_ENDS = RoundedEnd(0.012, 0.95, 0.96);
static constexpr Band HOOD_CAB_GLASS = {0.19, 0.23};
static constexpr double RUNNING_PLATE = 0.074;
static constexpr double BOILER_AXIS = 0.15;
static constexpr double BOILER_RADIUS = 0.075;
static constexpr int BOILER_SIDES = 12;
static constexpr std::array<double, 3> BOILER_BANDS = {-0.04, 0.03, 0.1};
static constexpr double BOILER_BAND_HALF = 0.003;
static constexpr double SMOKEBOX_BACK = 0.17;
static constexpr double SMOKEBOX_FRONT = 0.215;
static constexpr double STEAM_CAB_HALF = 0.125;
static constexpr double STEAM_CAB_FRONT = -0.08;
static constexpr HullProfile STEAM_CAB = {STEAM_CAB_HALF, RUNNING_PLATE, 0.25, 0.012};
static constexpr HullProfile STEAM_CAB_ROOF = {0.135, 0.245, 0.268, 0.02};
static constexpr WindowSpan STEAM_CAB_WINDOW = {-0.15, -0.09, 1};
static constexpr Band STEAM_CAB_GLASS = {0.18, 0.235};
static constexpr double STEAM_CAB_DOOR = -0.185;
static constexpr Band STEAM_CAB_OPENING = {0.11, 0.235};
static constexpr double STEAM_CAB_SCREEN_HALF = 0.12;
static constexpr Band STEAM_CAB_SCREEN = {0.2, 0.24};
static constexpr double CYLINDER_RADIUS = 0.028;
static constexpr double CYLINDER_AXIS = 0.085;
static constexpr double CYLINDER_SIDE = 0.098;
static constexpr std::array<double, 3> STEAM_DRIVERS = {-0.075, 0.015, 0.105};
static constexpr double DRIVER_RADIUS = 0.042;
static constexpr double DRIVER_WIDTH = 0.016;
static constexpr double STEAM_HALF_TRACK = 0.095;
static constexpr double LEADING_PAIR_X = 0.185;
static constexpr double LEADING_PAIR_RADIUS = 0.022;
static constexpr double LEADING_PAIR_WIDTH = 0.012;
static constexpr double TRAILING_PAIR_X = -0.17;
static constexpr double TRAILING_PAIR_WIDTH = 0.014;
static constexpr double SPLASHER_CLEARANCE = 0.004;
static constexpr int SPLASHER_STEPS = 4;
static constexpr double ROD_SIDE = STEAM_HALF_TRACK + DRIVER_WIDTH * 0.5 + 0.001;
static constexpr double ROD_HALF = 0.003;
static constexpr std::array<double, 4> VAN_RIBS = {-0.17, -0.11, 0.11, 0.17};
static constexpr Band VAN_DOOR = {0.07, 0.205};
static constexpr Band COACH_DOOR = {0.075, 0.2};
static constexpr std::array<double, 2> COACH_DOOR_XS = {-0.205, 0.205};
static constexpr double COACH_DOOR_WIDTH = 0.024;
static constexpr double GANGWAY_TUCK = 0.006;
static constexpr double GANGWAY_HALF = 0.07;
static constexpr double GANGWAY_FOOT = 0.015;
static constexpr double GANGWAY_HEAD = 0.008;
static constexpr WindowSpan COACH_WINDOWS = {-0.18, 0.18, 6};
static constexpr Band COACH_GLASS = {0.15, 0.198};
static constexpr std::array<double, 2> ROOF_UNITS = {-0.12, 0.12};
static constexpr Vec3 ROOF_UNIT_SIZE = {0.07, 0.09, 0.012};
static constexpr std::array<WindowSpan, 2> MAIL_WINDOWS = {{{-0.17, -0.11, 1}, {0.11, 0.17, 1}}};
static constexpr Band MAIL_GLASS = {0.155, 0.19};
static constexpr WindowSpan ARMOURED_WINDOWS = {-0.12, 0.12, 3};
static constexpr Band ARMOURED_GLASS = {0.165, 0.175};
static constexpr double ARMOURED_HALF = 0.12;
static constexpr double VAN_RIB_WIDTH = 0.008;
static constexpr double TANK_AXIS = 0.158;
static constexpr double TANK_RADIUS = 0.085;
static constexpr int TANK_SIDES = 12;
static constexpr double WALKWAY_REACH = 0.2;
static constexpr Band WALKWAY_ACROSS = {0.1, 0.125};
static constexpr Band WALKWAY_HEIGHT = {0.004, 0.008};
static constexpr double HANDRAIL_REACH = 0.19;
static constexpr Band HANDRAIL_STANDOFF = {0.004, 0.008};
static constexpr double HANDRAIL_HALF_HEIGHT = 0.002;
static constexpr std::array<LatheRing, 10> TANK_SHELL = {{
	{0.0, -0.226}, {0.045, -0.222}, {0.07, -0.212}, {0.082, -0.2}, {TANK_RADIUS, -0.186},
	{TANK_RADIUS, 0.186}, {0.082, 0.2}, {0.07, 0.212}, {0.045, 0.222}, {0.0, 0.226},
}};
static constexpr double WAGON_REACH = 0.225;
static constexpr double OPEN_HALF = 0.125;
static constexpr double OPEN_WALL = 0.01;
static constexpr Band OPEN_WALLS = {0.075, 0.17};
static constexpr double RIM_DEPTH = 0.007;
static constexpr std::array<double, 6> OPEN_STANCHIONS = {-0.17, -0.115, -0.06, 0.06, 0.115, 0.17};
static constexpr double STANCHION_HALF = 0.004;
static constexpr double OPEN_DOOR_HALF = 0.045;
static constexpr double OPEN_DOOR_INSET = 0.008;
static constexpr HullProfile HOPPER_BODY = {0.122, 0.1, 0.228, 0.06};
static constexpr BodyEnd HOPPER_ENDS = RoundedEnd(0.014, 0.9, 0.92);
static constexpr double HOPPER_REACH = 0.205;
static constexpr Band HOPPER_BAND = {0.15, 0.162};
static constexpr std::array<double, 6> HOPPER_RIBS = {-0.17, -0.105, -0.035, 0.035, 0.105, 0.17};
static constexpr double HOPPER_RIB_WIDTH = 0.006;
static constexpr std::array<double, 2> HOPPER_CHUTES = {-0.075, 0.075};
static constexpr double CHUTE_HALF_TOP = 0.072;
static constexpr double CHUTE_HALF_FOOT = 0.02;
static constexpr double CHUTE_FOOT = 0.04;
static constexpr double CHUTE_HALF_WIDTH = 0.085;
static constexpr double FLAT_HALF = 0.125;
static constexpr double FLAT_DECK = 0.078;
static constexpr std::array<double, 4> FLAT_STAKES = {-0.18, -0.06, 0.06, 0.18};
static constexpr double SILL_THICKNESS = 0.008;
static constexpr std::array<MapVector, 6> FISH_BELLY = {{
	{-UNIT_REACH, 0.05}, {-0.1, 0.034}, {0.1, 0.034}, {UNIT_REACH, 0.05}, {UNIT_REACH, FLAT_DECK}, {-UNIT_REACH, FLAT_DECK},
}};

static ModelMesh Coupler(double end)
{
	double body = end * UNIT_REACH;
	double reach = end * COUPLING_REACH;
	return Plank({std::min(body, reach), -0.015, 0.072}, {std::max(body, reach), 0.015, 0.086}, RUNNING_GEAR);
}

/* The frame under a body, couplers reaching for the next unit and a bogie at either end. */
static void Underframe(VehicleKit &kit, double half_width)
{
	kit.Body(Plank({-UNIT_REACH, -half_width, 0.05}, {UNIT_REACH, half_width, FLOOR + 0.004}, RUNNING_GEAR));
	for (double end : {-1.0, 1.0}) {
		kit.Fine(Coupler(end));
		kit.Fine(Bogie(end * BOGIE_X, HALF_GAUGE, WHEEL_RADIUS));
	}
}

/* A diamond frame on the roof, raised until its head meets the contact wire. */
static ModelMesh Pantograph(double x, double roof)
{
	ModelMesh pantograph = Plank({x - 0.03, -0.045, roof}, {x + 0.03, 0.045, roof + 0.012}, PANTOGRAPH);
	double knee = (roof + WIRE_REACH) * 0.5;
	for (double side : {-0.035, 0.035}) {
		pantograph.Append(Strut({x - 0.025, side, roof + 0.012}, {x + 0.035, side, knee}, STRUT_HALF));
		pantograph.Append(Strut({x + 0.035, side, knee}, {x, side, WIRE_REACH - 0.004}, STRUT_HALF));
	}
	pantograph.Append(Plank({x - 0.006, -0.07, WIRE_REACH - 0.006}, {x + 0.006, 0.07, WIRE_REACH}, PANTOGRAPH));
	return pantograph.Paint(PANTOGRAPH).Gloss(METAL_GLOSS);
}

/* Skirts hanging either side of a guideway the unit straddles or floats in. */
static ModelMesh GuidewaySkirts()
{
	return Flanked(Plank({-UNIT_REACH, 0.088, -0.075}, {UNIT_REACH, 0.112, 0.01}, DARK_SECONDARY));
}

/* A guard over the top of a driving wheel where it rises through the running plate. */
static ModelMesh Splasher(double x)
{
	double radius = DRIVER_RADIUS + SPLASHER_CLEARANCE;
	double rise = std::asin((RUNNING_PLATE - DRIVER_RADIUS) / radius);
	std::array<MapVector, SPLASHER_STEPS + 1> outline;
	for (int step = 0; step <= SPLASHER_STEPS; step++) {
		double angle = std::lerp(rise, std::numbers::pi - rise, static_cast<double>(step) / SPLASHER_STEPS);
		outline[step] = {x + radius * std::cos(angle), DRIVER_RADIUS + radius * std::sin(angle)};
	}
	return Upright(outline, DRIVER_WIDTH + SPLASHER_CLEARANCE).Move({0.0, STEAM_HALF_TRACK, 0.0}).Paint(PRIMARY).Gloss(PAINT_GLOSS);
}

/* A steam locomotive. */
static ModelMesh SteamEngine(VehicleDetail detail)
{
	VehicleKit kit(detail);
	kit.Body(Plank({-UNIT_REACH, -0.11, 0.05}, {UNIT_REACH, 0.11, RUNNING_PLATE}, RUNNING_GEAR));
	kit.Body(Barrel(STEAM_CAB_FRONT, SMOKEBOX_BACK, BOILER_RADIUS, BOILER_AXIS, BOILER_SIDES).Paint(DARK_PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(Barrel(SMOKEBOX_BACK, SMOKEBOX_FRONT, BOILER_RADIUS + 0.002, BOILER_AXIS, BOILER_SIDES).Paint(RUNNING_GEAR));
	kit.Body(Column(8, 0.024, STEAM_CHIMNEY_MOUTH, STEAM_CHIMNEY_HEIGHT).Transform(Mat4::Translation(STEAM_CHIMNEY_FOOT)).Paint(RUNNING_GEAR));
	kit.Body(BodyRun(STEAM_CAB, -0.215, STEAM_CAB_FRONT, SQUARE_END, SQUARE_END, kit.Steps()).Shell().Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(BodyRun(STEAM_CAB_ROOF, -UNIT_REACH, STEAM_CAB_FRONT + 0.01, SQUARE_END, SQUARE_END, kit.Steps()).Shell().Paint(RUNNING_GEAR));
	for (double x : BOILER_BANDS) kit.Fine(Barrel(x - BOILER_BAND_HALF, x + BOILER_BAND_HALF, BOILER_RADIUS + 0.0015, BOILER_AXIS, BOILER_SIDES).Paint(BRASS).Gloss(METAL_GLOSS));
	kit.Fine(Column(8, 0.032, 0.022, 0.04).Transform(Mat4::Translation({0.06, 0.0, 0.215})).Paint(BRASS).Gloss(METAL_GLOSS));
	kit.Fine(Column(8, 0.026, 0.02, 0.03).Transform(Mat4::Translation({-0.01, 0.0, 0.218})).Paint(DARK_PRIMARY).Gloss(PAINT_GLOSS));
	kit.Fine(Barrel(SMOKEBOX_FRONT, SMOKEBOX_FRONT + 0.004, 0.05, BOILER_AXIS, BOILER_SIDES).Paint(STEEL));
	kit.Fine(SideBand(-UNIT_REACH, UNIT_REACH, 0.11, 0.054, 0.07, SECONDARY));
	kit.Fine(Plank({SMOKEBOX_FRONT, -0.11, 0.05}, {UNIT_REACH, 0.11, 0.088}, SECONDARY));
	kit.Fine(Flanked(Barrel(UNIT_REACH, UNIT_REACH + 0.012, 0.009, 0.068, 6).Move({0.0, 0.07, 0.0}).Paint(STEEL)));
	kit.Fine(WindowGlass(STEAM_CAB_WINDOW, STEAM_CAB_GLASS, STEAM_CAB_HALF));
	kit.Fine(Rib(STEAM_CAB_DOOR, STEAM_CAB_HALF + PROUD, STEAM_CAB_OPENING, 0.03, RUNNING_GEAR));
	kit.Fine(Windscreen(STEAM_CAB_FRONT, STEAM_CAB_SCREEN_HALF, STEAM_CAB_SCREEN.low, STEAM_CAB_SCREEN.high));
	kit.Fine(Flanked(Barrel(0.15, SMOKEBOX_FRONT, CYLINDER_RADIUS, CYLINDER_AXIS, 8).Move({0.0, CYLINDER_SIDE, 0.0}).Paint(DARK_PRIMARY).Gloss(PAINT_GLOSS)));
	for (double x : STEAM_DRIVERS) {
		kit.Fine(WheelPair(x, STEAM_HALF_TRACK, DRIVER_RADIUS, DRIVER_WIDTH).Paint(DARK_SECONDARY));
		kit.Fine(Flanked(Splasher(x)));
	}
	kit.Fine(Flanked(Plank({STEAM_DRIVERS.front(), ROD_SIDE, DRIVER_RADIUS - ROD_HALF}, {STEAM_DRIVERS.back(), ROD_SIDE + 0.004, DRIVER_RADIUS + ROD_HALF}, STEEL).Gloss(METAL_GLOSS)));
	kit.Fine(WheelPair(LEADING_PAIR_X, STEAM_HALF_TRACK, LEADING_PAIR_RADIUS, LEADING_PAIR_WIDTH));
	kit.Fine(WheelPair(TRAILING_PAIR_X, STEAM_HALF_TRACK, WHEEL_RADIUS, TRAILING_PAIR_WIDTH));
	kit.Fine(Plank({0.222, -0.012, 0.2}, {0.226, 0.012, 0.222}, HEADLAMP).Glow(LAMP_GLOW));
	return kit.Done();
}

static void HoodUnitCab(VehicleKit &kit)
{
	BodyRun long_hood(LONG_HOOD, -0.215, 0.04, HOOD_END, SQUARE_END, kit.Steps());
	BodyRun short_hood(SHORT_HOOD, 0.13, 0.215, SQUARE_END, HOOD_END, kit.Steps());
	BodyRun cab(HOOD_CAB, 0.04, 0.13, HOOD_CAB_ENDS, HOOD_CAB_ENDS, kit.Steps());
	for (const BodyRun *hood : {&long_hood, &short_hood}) {
		kit.Body(hood->Shell().Paint(PRIMARY).Gloss(PAINT_GLOSS));
		kit.Body(hood->Stripe(HOOD_BAND, SECONDARY).Gloss(PAINT_GLOSS));
	}
	kit.Body(cab.Shell().Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(Hull({0.125, 0.25, 0.262, 0.008}, 0.035, 0.135).Paint(ROOF_GREY));
	kit.Fine(cab.Stripe(HOOD_CAB_GLASS, GLASS).Gloss(GLASS_GLOSS));
	kit.Fine(Plank({-0.19, -0.05, 0.2}, {-0.09, 0.05, 0.204}, RUNNING_GEAR));
	kit.Fine(Plank({-0.05, -0.015, 0.2}, {-0.025, 0.015, 0.222}, RUNNING_GEAR));
	kit.Fine(LampPair(0.215, 0.04, 0.15, 0.008, HEADLAMP));
	kit.Fine(LampPair(-0.215, 0.04, 0.15, 0.008, TAIL_LAMP));
}

static ModelMesh DieselEngine(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Underframe(kit, 0.12);
	kit.Fine(SideBand(-UNIT_REACH, UNIT_REACH, 0.12, 0.054, 0.064, SECONDARY));
	HoodUnitCab(kit);
	return kit.Done();
}

/* A full width body running into a rounded nose wrapped in glass, the streamlined look of fast or late engines. */
static void Streamliner(VehicleKit &kit, double nose_from, const NoseShape &shape)
{
	const HullProfile &profile = STREAMLINED_BODY;
	BodyRun body(profile, -UNIT_REACH, UNIT_REACH, SQUARE_END, TaperedEnd(UNIT_REACH - nose_from, shape), kit.Steps());
	kit.Body(body.Shell().Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(body.Over(profile.Grown(PROUD).Above(STREAMLINED_GLASS_FOOT), nose_from, UNIT_REACH).Paint(GLASS).Gloss(GLASS_GLOSS));
	kit.Body(body.Stripe(STREAMLINED_BAND, -UNIT_REACH, nose_from, SECONDARY).Gloss(PAINT_GLOSS));
	kit.Fine(body.Stripe(STREAMLINED_BAND, nose_from, UNIT_REACH, SECONDARY).Gloss(PAINT_GLOSS));
	kit.Fine(Windscreen(-UNIT_REACH, 0.09, 0.17, 0.215));
	kit.Fine(Plank({-0.15, -0.06, profile.high}, {-0.02, 0.06, profile.high + 0.006}, RUNNING_GEAR));
	kit.Fine(LampPair(UNIT_REACH - 0.008, 0.05, 0.095, 0.008, HEADLAMP));
	kit.Fine(LampPair(-UNIT_REACH, 0.05, 0.095, 0.008, TAIL_LAMP));
}

static ModelMesh DieselStreamliner(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Underframe(kit, 0.125);
	Streamliner(kit, 0.11, {0.72, 0.55, 1.6});
	return kit.Done();
}

static ModelMesh ElectricEngine(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Underframe(kit, 0.125);
	BodyRun body(ELECTRIC_BODY, -0.215, 0.215, ELECTRIC_ENDS, ELECTRIC_ENDS, kit.Steps());
	kit.Body(body.Shell().Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(body.Stripe(ELECTRIC_BAND, SECONDARY).Gloss(PAINT_GLOSS));
	kit.Body(body.Stripe(ELECTRIC_GLASS, ELECTRIC_WINDOWS.GlassBack(), ELECTRIC_WINDOWS.GlassFront(), GLASS).Gloss(GLASS_GLOSS));
	kit.Fine(WindowPosts(ELECTRIC_WINDOWS, ELECTRIC_GLASS, BODY_HALF + PROUD, PRIMARY).Gloss(PAINT_GLOSS));
	kit.Fine(body.Stripe(ELECTRIC_CAB_GLASS, body.Back(), body.Back() + ELECTRIC_ENDS.reach, GLASS).Gloss(GLASS_GLOSS));
	kit.Fine(body.Stripe(ELECTRIC_CAB_GLASS, body.Front() - ELECTRIC_ENDS.reach, body.Front(), GLASS).Gloss(GLASS_GLOSS));
	kit.Fine(Plank({-0.05, -0.06, 0.235}, {0.05, 0.06, 0.247}, ROOF_GREY));
	kit.Fine(Pantograph(0.11, 0.235));
	kit.Fine(LampPair(0.215, 0.05, 0.1, 0.008, HEADLAMP));
	kit.Fine(LampPair(-0.215, 0.05, 0.1, 0.008, TAIL_LAMP));
	return kit.Done();
}

static ModelMesh ElectricStreamliner(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Underframe(kit, 0.125);
	Streamliner(kit, 0.06, {0.55, 0.42, 1.8});
	kit.Fine(Pantograph(-0.08, STREAMLINED_BODY.high));
	return kit.Done();
}

static void Gangways(VehicleKit &kit, const HullProfile &body)
{
	for (double end : {-1.0, 1.0}) {
		double inner = end * (UNIT_REACH - GANGWAY_TUCK);
		double outer = end * COUPLING_REACH;
		kit.Body(Plank({std::min(inner, outer), -GANGWAY_HALF, body.low + GANGWAY_FOOT}, {std::max(inner, outer), GANGWAY_HALF, body.high - GANGWAY_HEAD}, RUNNING_GEAR));
	}
}

/* Monorail and maglev units share a body slung low around their guideway, rounded over the top. */
static void SleekBody(VehicleKit &kit, double nose_from, const NoseShape &shape)
{
	const HullProfile &profile = SLEEK_BODY;
	bool nosed = nose_from < UNIT_REACH;
	BodyRun body(profile, -UNIT_REACH, UNIT_REACH, SQUARE_END, nosed ? TaperedEnd(UNIT_REACH - nose_from, shape) : SQUARE_END, kit.Steps());
	kit.Body(body.Shell().Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(GuidewaySkirts());
	kit.Body(body.Stripe(SLEEK_GLASS, -UNIT_REACH, std::min(nose_from, UNIT_REACH), GLASS).Gloss(GLASS_GLOSS));
	kit.Fine(body.Stripe(SLEEK_BAND, SECONDARY).Gloss(PAINT_GLOSS));
	if (!nosed) return;
	kit.Body(body.Over(profile.Grown(PROUD).Above(SLEEK_GLASS.low), nose_from, UNIT_REACH).Paint(GLASS).Gloss(GLASS_GLOSS));
	kit.Fine(LampPair(UNIT_REACH - 0.006, 0.04, 0.035, 0.007, HEADLAMP));
}

static ModelMesh MonorailEngine(VehicleDetail detail)
{
	VehicleKit kit(detail);
	SleekBody(kit, 0.08, {0.6, 0.5, 1.7});
	return kit.Done();
}

static ModelMesh MaglevEngine(VehicleDetail detail)
{
	VehicleKit kit(detail);
	SleekBody(kit, -0.02, {0.32, 0.28, 1.4});
	return kit.Done();
}

static ModelMesh SleekCoach(VehicleDetail detail)
{
	VehicleKit kit(detail);
	SleekBody(kit, UNIT_REACH, {});
	Gangways(kit, SLEEK_BODY);
	return kit.Done();
}

static void CoachShell(VehicleKit &kit, uint32_t body_tone, uint32_t band_tone)
{
	Underframe(kit, 0.11);
	kit.Body(Hull(COACH_BODY, -UNIT_REACH, UNIT_REACH).Paint(body_tone).Gloss(PAINT_GLOSS));
	kit.Body(BodyRun(COACH_ROOF, -UNIT_REACH, UNIT_REACH, SQUARE_END, SQUARE_END, kit.Steps()).Shell().Paint(ROOF_GREY));
	kit.Body(SideBand(-UNIT_REACH, UNIT_REACH, BODY_HALF, 0.122, 0.14, band_tone));
	Gangways(kit, COACH_BODY);
}

static ModelMesh Coach(VehicleDetail detail)
{
	VehicleKit kit(detail);
	CoachShell(kit, PRIMARY, SECONDARY);
	kit.Body(WindowGlass(COACH_WINDOWS, COACH_GLASS, BODY_HALF));
	kit.Fine(WindowPosts(COACH_WINDOWS, COACH_GLASS, BODY_HALF + PROUD, PRIMARY).Gloss(PAINT_GLOSS));
	for (double x : ROOF_UNITS) {
		Vec3 half = ROOF_UNIT_SIZE * 0.5;
		kit.Fine(Plank({x - half.x, -half.y, ROOF_TOP}, {x + half.x, half.y, ROOF_TOP + ROOF_UNIT_SIZE.z}, STEEL).Gloss(METAL_GLOSS));
	}
	for (double x : COACH_DOOR_XS) kit.Fine(Rib(x, BODY_HALF + 0.001, COACH_DOOR, COACH_DOOR_WIDTH, DARK_SECONDARY));
	return kit.Done();
}

static ModelMesh MailVan(VehicleDetail detail)
{
	VehicleKit kit(detail);
	CoachShell(kit, MUTED_PRIMARY, SECONDARY);
	for (const WindowSpan &window : MAIL_WINDOWS) kit.Body(WindowGlass(window, MAIL_GLASS, BODY_HALF));
	kit.Fine(Rib(0.0, BODY_HALF + 0.001, COACH_DOOR, 0.08, DARK_SECONDARY));
	return kit.Done();
}

static void VanShell(VehicleKit &kit, uint32_t body_tone)
{
	Underframe(kit, 0.11);
	kit.Body(Hull(VAN_BODY, -UNIT_REACH, UNIT_REACH).Paint(body_tone).Gloss(PAINT_GLOSS));
	kit.Body(Hull({VAN_BODY.half_width, COACH_TOP, 0.235, 0.02}, -UNIT_REACH, UNIT_REACH).Paint(ROOF_GREY));
}

static ModelMesh BoxVan(VehicleDetail detail)
{
	VehicleKit kit(detail);
	VanShell(kit, MUTED_PRIMARY);
	kit.Fine(Rib(0.0, VAN_BODY.half_width, VAN_DOOR, 0.09, DARK_SECONDARY));
	kit.Fine(Ribs(VAN_RIBS, VAN_BODY.half_width, {FLOOR, COACH_TOP}, VAN_RIB_WIDTH, DARK_PRIMARY));
	return kit.Done();
}

static ModelMesh Reefer(VehicleDetail detail)
{
	VehicleKit kit(detail);
	VanShell(kit, CREAM);
	kit.Body(SideBand(-UNIT_REACH, UNIT_REACH, VAN_BODY.half_width, 0.17, 0.19, PRIMARY));
	kit.Fine(Rib(0.0, VAN_BODY.half_width, {VAN_DOOR.low, 0.16}, 0.08, STEEL));
	kit.Fine(Plank({-UNIT_REACH, -0.07, 0.235}, {-0.15, 0.07, 0.255}, STEEL));
	return kit.Done();
}

static ModelMesh ArmouredVan(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Underframe(kit, 0.11);
	kit.Body(Hull({ARMOURED_HALF, FLOOR, 0.195, 0.025}, -0.18, 0.18).Paint(DARK_PRIMARY).Gloss(METAL_GLOSS));
	kit.Body(SideBand(-0.18, 0.18, ARMOURED_HALF, 0.12, 0.135, SECONDARY));
	kit.Body(WindowGlass(ARMOURED_WINDOWS, ARMOURED_GLASS, ARMOURED_HALF));
	kit.Fine(WindowPosts(ARMOURED_WINDOWS, ARMOURED_GLASS, ARMOURED_HALF + PROUD, DARK_PRIMARY).Gloss(METAL_GLOSS));
	return kit.Done();
}

/* A tank wagon. */
static ModelMesh TankWagon(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Underframe(kit, 0.1);
	kit.Body(Turned(TANK_SHELL, TANK_SIDES, TANK_AXIS).Paint(STEEL).Gloss(METAL_GLOSS));
	kit.Body(Barrel(-0.03, 0.03, TANK_RADIUS + 0.002, TANK_AXIS, TANK_SIDES).Paint(PRIMARY));
	for (double end : {-1.0, 1.0}) kit.Body(Plank({end * BOGIE_X - 0.025, -0.07, FLOOR}, {end * BOGIE_X + 0.025, 0.07, TANK_AXIS - 0.06}, RUNNING_GEAR));
	kit.Fine(Column(8, 0.03, 0.026, 0.03).Transform(Mat4::Translation({0.0, 0.0, TANK_AXIS + TANK_RADIUS - 0.008})).Paint(STEEL).Gloss(METAL_GLOSS));
	kit.Fine(Flanked(Plank({-WALKWAY_REACH, WALKWAY_ACROSS.low, FLOOR + WALKWAY_HEIGHT.low}, {WALKWAY_REACH, WALKWAY_ACROSS.high, FLOOR + WALKWAY_HEIGHT.high}, RUNNING_GEAR)));
	kit.Fine(Flanked(Plank({-HANDRAIL_REACH, TANK_RADIUS + HANDRAIL_STANDOFF.low, TANK_AXIS - HANDRAIL_HALF_HEIGHT}, {HANDRAIL_REACH, TANK_RADIUS + HANDRAIL_STANDOFF.high, TANK_AXIS + HANDRAIL_HALF_HEIGHT}, STEEL)));
	return kit.Done();
}

/* A rim capping the walls round an open body, standing a little proud of them outside. */
static ModelMesh Rim(double reach, double half_width, double wall, double z, uint32_t tone)
{
	ModelMesh rim = Flanked(Plank({-reach - PROUD, half_width - wall, z}, {reach + PROUD, half_width + PROUD, z + RIM_DEPTH}, tone));
	for (double x : {-reach - PROUD, reach - wall}) rim.Append(Plank({x, -half_width, z}, {x + wall + PROUD, half_width, z + RIM_DEPTH}, tone));
	return rim;
}

/* A chute under a hopper's body, its ends sloping in to its outlet. */
static ModelMesh Chute(double x)
{
	std::array<MapVector, 4> outline = {{{x - CHUTE_HALF_FOOT, CHUTE_FOOT}, {x + CHUTE_HALF_FOOT, CHUTE_FOOT}, {x + CHUTE_HALF_TOP, HOPPER_BODY.low}, {x - CHUTE_HALF_TOP, HOPPER_BODY.low}}};
	return Upright(outline, CHUTE_HALF_WIDTH * 2.0).Paint(DARK_PRIMARY);
}

/* A low mound of loose cargo, heaped higher in the middle. */
static ModelMesh Heap(double half_length, double half_width, double base, double rise)
{
	std::array<LatheRing, 4> mound = {{{1.0, 0.0}, {0.78, 0.45}, {0.35, 0.85}, {0.0, 1.0}}};
	return Lathe(mound, 8).Transform(Mat4::Translation({0.0, 0.0, base}) * Mat4::Scaling({half_length, half_width, rise})).Displace(HEAP_SPREAD, HEAP_SEED).Facet().Paint(CARGO);
}

/* An open wagon. */
static ModelMesh OpenWagon(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Underframe(kit, 0.11);
	kit.Body(Plank({-WAGON_REACH, -OPEN_HALF, FLOOR}, {WAGON_REACH, OPEN_HALF, OPEN_WALLS.low}, DARK_PRIMARY));
	kit.Body(Flanked(Plank({-WAGON_REACH, OPEN_HALF - OPEN_WALL, OPEN_WALLS.low}, {WAGON_REACH, OPEN_HALF, OPEN_WALLS.high}, MUTED_PRIMARY)));
	for (double x : {-WAGON_REACH, WAGON_REACH - OPEN_WALL}) kit.Body(Plank({x, -OPEN_HALF, OPEN_WALLS.low}, {x + OPEN_WALL, OPEN_HALF, OPEN_WALLS.high}, MUTED_PRIMARY));
	kit.Body(Plank({-WAGON_REACH + OPEN_WALL, -OPEN_HALF + OPEN_WALL, OPEN_WALLS.low}, {WAGON_REACH - OPEN_WALL, OPEN_HALF - OPEN_WALL, 0.15}, CARGO));
	kit.Body(Heap(0.2, 0.105, 0.15, 0.045));
	kit.Fine(Rim(WAGON_REACH, OPEN_HALF, OPEN_WALL, OPEN_WALLS.high, DARK_PRIMARY));
	for (double x : OPEN_STANCHIONS) kit.Fine(Flanked(Plank({x - STANCHION_HALF, OPEN_HALF, OPEN_WALLS.low}, {x + STANCHION_HALF, OPEN_HALF + PROUD, OPEN_WALLS.high}, DARK_PRIMARY)));
	kit.Fine(Flanked(Plank({-OPEN_DOOR_HALF, OPEN_HALF, OPEN_WALLS.low + OPEN_DOOR_INSET}, {OPEN_DOOR_HALF, OPEN_HALF + PROUD * 0.5, OPEN_WALLS.high - OPEN_DOOR_INSET}, DARK_PRIMARY)));
	return kit.Done();
}

/* A covered hopper wagon. */
static ModelMesh CoveredHopper(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Underframe(kit, 0.1);
	BodyRun body(HOPPER_BODY, -HOPPER_REACH, HOPPER_REACH, HOPPER_ENDS, HOPPER_ENDS, kit.Steps());
	kit.Body(body.Shell().Paint(MUTED_PRIMARY).Gloss(PAINT_GLOSS));
	for (double x : HOPPER_CHUTES) kit.Body(Chute(x));
	kit.Fine(body.Stripe(HOPPER_BAND, SECONDARY).Gloss(PAINT_GLOSS));
	kit.Fine(Ribs(HOPPER_RIBS, HOPPER_BODY.half_width + PROUD, {HOPPER_BODY.low, HOPPER_BODY.high - HOPPER_BODY.bevel}, HOPPER_RIB_WIDTH, DARK_PRIMARY));
	kit.Fine(Plank({-0.19, -0.02, HOPPER_BODY.high}, {0.19, 0.02, HOPPER_BODY.high + 0.006}, ROOF_GREY));
	return kit.Done();
}

/* A timber deck on sills bellied deep between the bogies, logs lashed to it between stakes. */
static ModelMesh FlatWagon(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Underframe(kit, 0.1);
	kit.Body(Plank({-WAGON_REACH, -FLAT_HALF, FLOOR}, {WAGON_REACH, FLAT_HALF, FLAT_DECK}, TIMBER_DECK));
	kit.Body(Flanked(Upright(FISH_BELLY, SILL_THICKNESS).Move({0.0, FLAT_HALF - SILL_THICKNESS * 0.5 + PROUD, 0.0}).Paint(DARK_PRIMARY)));
	for (double y : {-0.07, 0.0, 0.07}) kit.Body(Barrel(-0.2, 0.2, 0.035, 0.113, 7).Transform(Mat4::Translation({0.0, y, 0.0})).Paint(CARGO));
	for (double y : {-0.035, 0.035}) kit.Fine(Barrel(-0.19, 0.19, 0.033, 0.175, 7).Transform(Mat4::Translation({0.0, y, 0.0})).Paint(BRIGHT_CARGO));
	for (double x : FLAT_STAKES) kit.Fine(Flanked(Plank({x - 0.006, 0.112, FLAT_DECK}, {x + 0.006, 0.124, 0.15}, RUNNING_GEAR)));
	return kit.Done();
}

static ModelMesh LivestockVan(VehicleDetail detail)
{
	VehicleKit kit(detail);
	VanShell(kit, MUTED_PRIMARY);
	for (double z : {0.09, 0.13, 0.17}) kit.Fine(SideBand(-0.21, 0.21, VAN_BODY.half_width, z, z + 0.012, SLAT_GAP));
	kit.Fine(Rib(0.0, VAN_BODY.half_width + 0.002, VAN_DOOR, 0.08, DARK_SECONDARY));
	return kit.Done();
}

ModelMesh RailVehicleModel(VehicleLook look, VehicleDetail detail)
{
	switch (look) {
		case VehicleLook::SteamEngine: return SteamEngine(detail);
		case VehicleLook::DieselEngine: return DieselEngine(detail);
		case VehicleLook::DieselStreamliner: return DieselStreamliner(detail);
		case VehicleLook::ElectricEngine: return ElectricEngine(detail);
		case VehicleLook::ElectricStreamliner: return ElectricStreamliner(detail);
		case VehicleLook::MonorailEngine: return MonorailEngine(detail);
		case VehicleLook::MaglevEngine: return MaglevEngine(detail);
		case VehicleLook::Coach: return Coach(detail);
		case VehicleLook::SleekCoach: return SleekCoach(detail);
		case VehicleLook::MailVan: return MailVan(detail);
		case VehicleLook::BoxVan: return BoxVan(detail);
		case VehicleLook::Reefer: return Reefer(detail);
		case VehicleLook::ArmouredVan: return ArmouredVan(detail);
		case VehicleLook::TankWagon: return TankWagon(detail);
		case VehicleLook::OpenWagon: return OpenWagon(detail);
		case VehicleLook::CoveredHopper: return CoveredHopper(detail);
		case VehicleLook::FlatWagon: return FlatWagon(detail);
		default: return LivestockVan(detail);
	}
}
