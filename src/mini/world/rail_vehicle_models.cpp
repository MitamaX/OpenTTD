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
static constexpr HullProfile SLEEK_BODY = {0.12, 0.0, 0.17, 0.06};

static ModelMesh Coupler(double end)
{
	double body = end * UNIT_REACH;
	double reach = end * UNIT_TILES * 0.5;
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
	ModelMesh skirt = Plank({-UNIT_REACH, 0.088, -0.075}, {UNIT_REACH, 0.112, 0.01}, DARK_SECONDARY);
	return skirt.Append(Mirrored(skirt));
}

static ModelMesh SteamEngine(VehicleDetail detail)
{
	VehicleKit kit(detail);
	kit.Body(Plank({-UNIT_REACH, -0.11, 0.05}, {UNIT_REACH, 0.11, 0.074}, RUNNING_GEAR));
	kit.Body(Barrel(-0.08, 0.17, 0.075, 0.15, 10).Paint(DARK_PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(Barrel(0.17, 0.215, 0.077, 0.15, 10).Paint(RUNNING_GEAR));
	kit.Body(Column(8, 0.024, STEAM_CHIMNEY_MOUTH, STEAM_CHIMNEY_HEIGHT).Transform(Mat4::Translation(STEAM_CHIMNEY_FOOT)).Paint(RUNNING_GEAR));
	kit.Body(Hull({0.125, 0.074, 0.255, 0.01}, -0.215, -0.08).Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(Plank({-0.228, -0.135, 0.255}, {-0.07, 0.135, 0.268}, RUNNING_GEAR));
	kit.Fine(Column(8, 0.032, 0.022, 0.04).Transform(Mat4::Translation({0.06, 0.0, 0.215})).Paint(BRASS).Gloss(METAL_GLOSS));
	kit.Fine(Barrel(0.215, 0.219, 0.05, 0.15, 10).Paint(STEEL));
	kit.Fine(SideBand(-UNIT_REACH, UNIT_REACH, 0.11, 0.054, 0.07, SECONDARY));
	kit.Fine(Plank({0.215, -0.11, 0.05}, {0.228, 0.11, 0.088}, SECONDARY));
	kit.Fine(WindowRow(-0.2, -0.1, 0.18, 0.235, 0.125, 1));
	kit.Fine(Windscreen(-0.08, 0.12, 0.2, 0.24));
	for (double x : {-0.03, 0.065, 0.16}) kit.Fine(WheelPair(x, 0.095, 0.045, 0.016).Paint(DARK_SECONDARY));
	kit.Fine(WheelPair(-0.17, 0.095, WHEEL_RADIUS, 0.014));
	kit.Fine(Plank({0.222, -0.012, 0.2}, {0.226, 0.012, 0.222}, HEADLAMP).Glow(LAMP_GLOW));
	return kit.Done();
}

static void HoodUnitCab(VehicleKit &kit)
{
	kit.Body(Hull({0.085, 0.075, 0.2, 0.02}, -0.215, 0.04).Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(Hull({0.122, 0.075, 0.25, 0.02}, 0.04, 0.13).Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(Hull({0.085, 0.075, 0.17, 0.02}, 0.13, 0.215).Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(Hull({0.125, 0.25, 0.262, 0.008}, 0.035, 0.135).Paint(ROOF_GREY));
	kit.Body(SideBand(-0.215, 0.215, 0.085, 0.1, 0.12, SECONDARY));
	kit.Fine(Windscreen(0.13, 0.095, 0.19, 0.235));
	kit.Fine(Windscreen(0.04, 0.095, 0.19, 0.235));
	kit.Fine(WindowRow(0.055, 0.115, 0.19, 0.235, 0.122, 1));
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
	const HullProfile &body = STREAMLINED_BODY;
	kit.Body(Hull(body, -UNIT_REACH, nose_from).Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(Nose(body, nose_from, UNIT_REACH, shape, body.low).Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(Nose({body.half_width + 0.003, 0.17, body.high + 0.003, body.bevel}, nose_from, UNIT_REACH, shape, body.low).Paint(GLASS).Gloss(GLASS_GLOSS));
	kit.Body(SideBand(-UNIT_REACH, nose_from, body.half_width, 0.1, 0.125, SECONDARY));
	kit.Fine(Nose({body.half_width + 0.0025, 0.1, 0.125, 0.0}, nose_from, UNIT_REACH, shape, body.low).Paint(SECONDARY));
	kit.Fine(Windscreen(-UNIT_REACH, 0.09, 0.17, 0.215));
	kit.Fine(Plank({-0.15, -0.06, body.high}, {-0.02, 0.06, body.high + 0.006}, RUNNING_GEAR));
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
	kit.Body(Hull({BODY_HALF, 0.07, 0.235, 0.035}, -0.215, 0.215).Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(SideBand(-0.215, 0.215, BODY_HALF, 0.1, 0.122, SECONDARY));
	kit.Fine(Windscreen(0.215, 0.1, 0.17, 0.215));
	kit.Fine(Windscreen(-0.215, 0.1, 0.17, 0.215));
	kit.Fine(WindowRow(-0.17, 0.17, 0.15, 0.2, BODY_HALF, 5));
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

/* Monorail and maglev units share a body slung low around their guideway, rounded over the top. */
static void SleekBody(VehicleKit &kit, double nose_from, const NoseShape &shape)
{
	const HullProfile &body = SLEEK_BODY;
	kit.Body(Hull(body, -UNIT_REACH, nose_from).Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(GuidewaySkirts());
	kit.Body(SideBand(-UNIT_REACH, nose_from, body.half_width, 0.085, 0.135, GLASS).Gloss(GLASS_GLOSS));
	if (nose_from >= UNIT_REACH) return;
	kit.Body(Nose(body, nose_from, UNIT_REACH, shape, body.low).Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(Nose({body.half_width + 0.003, 0.085, body.high + 0.003, body.bevel}, nose_from, UNIT_REACH, shape, body.low).Paint(GLASS).Gloss(GLASS_GLOSS));
	kit.Fine(LampPair(UNIT_REACH - 0.006, 0.04, 0.035, 0.007, HEADLAMP));
}

static ModelMesh MonorailEngine(VehicleDetail detail)
{
	VehicleKit kit(detail);
	SleekBody(kit, 0.08, {0.6, 0.5, 1.7});
	kit.Fine(SideBand(-UNIT_REACH, 0.08, SLEEK_BODY.half_width, 0.04, 0.055, SECONDARY));
	return kit.Done();
}

static ModelMesh MaglevEngine(VehicleDetail detail)
{
	VehicleKit kit(detail);
	SleekBody(kit, -0.02, {0.32, 0.28, 1.4});
	kit.Fine(SideBand(-UNIT_REACH, -0.02, SLEEK_BODY.half_width, 0.04, 0.055, SECONDARY));
	return kit.Done();
}

static ModelMesh SleekCoach(VehicleDetail detail)
{
	VehicleKit kit(detail);
	SleekBody(kit, UNIT_REACH, {});
	kit.Fine(SideBand(-UNIT_REACH, UNIT_REACH, SLEEK_BODY.half_width, 0.04, 0.055, SECONDARY));
	return kit.Done();
}

static void CoachShell(VehicleKit &kit, uint32_t body_tone, uint32_t band_tone)
{
	Underframe(kit, 0.11);
	kit.Body(Hull(COACH_BODY, -UNIT_REACH, UNIT_REACH).Paint(body_tone).Gloss(PAINT_GLOSS));
	kit.Body(Hull(COACH_ROOF, -UNIT_REACH, UNIT_REACH).Paint(ROOF_GREY));
	kit.Body(SideBand(-UNIT_REACH, UNIT_REACH, BODY_HALF, 0.122, 0.14, band_tone));
	kit.Fine(Plank({-0.24, -0.05, 0.08}, {0.24, 0.05, 0.205}, RUNNING_GEAR));
}

static ModelMesh Coach(VehicleDetail detail)
{
	VehicleKit kit(detail);
	CoachShell(kit, PRIMARY, SECONDARY);
	kit.Fine(WindowRow(-0.18, 0.18, 0.15, 0.198, BODY_HALF, 6));
	for (double end : {-0.205, 0.205}) kit.Fine(SideBand(end - 0.012, end + 0.012, BODY_HALF + 0.001, 0.075, 0.2, DARK_SECONDARY));
	return kit.Done();
}

static ModelMesh MailVan(VehicleDetail detail)
{
	VehicleKit kit(detail);
	CoachShell(kit, MUTED_PRIMARY, SECONDARY);
	kit.Fine(WindowRow(-0.17, -0.11, 0.155, 0.19, BODY_HALF, 1));
	kit.Fine(WindowRow(0.11, 0.17, 0.155, 0.19, BODY_HALF, 1));
	kit.Fine(SideBand(-0.04, 0.04, BODY_HALF + 0.001, 0.075, 0.2, DARK_SECONDARY));
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
	kit.Fine(SideBand(-0.045, 0.045, VAN_BODY.half_width, 0.07, 0.205, DARK_SECONDARY));
	for (double x : {-0.17, -0.11, 0.11, 0.17}) kit.Fine(SideBand(x - 0.004, x + 0.004, VAN_BODY.half_width, FLOOR, COACH_TOP, DARK_PRIMARY));
	return kit.Done();
}

static ModelMesh Reefer(VehicleDetail detail)
{
	VehicleKit kit(detail);
	VanShell(kit, CREAM);
	kit.Body(SideBand(-UNIT_REACH, UNIT_REACH, VAN_BODY.half_width, 0.17, 0.19, PRIMARY));
	kit.Fine(SideBand(-0.04, 0.04, VAN_BODY.half_width, 0.07, 0.16, STEEL));
	kit.Fine(Plank({-UNIT_REACH, -0.07, 0.235}, {-0.15, 0.07, 0.255}, STEEL));
	return kit.Done();
}

static ModelMesh ArmouredVan(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Underframe(kit, 0.11);
	kit.Body(Hull({0.12, FLOOR, 0.195, 0.025}, -0.18, 0.18).Paint(DARK_PRIMARY).Gloss(METAL_GLOSS));
	kit.Body(SideBand(-0.18, 0.18, 0.12, 0.12, 0.135, SECONDARY));
	kit.Fine(WindowRow(-0.12, 0.12, 0.165, 0.175, 0.12, 3));
	return kit.Done();
}

static ModelMesh TankWagon(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Underframe(kit, 0.1);
	std::array<LatheRing, 4> tank = {{{0.055, -0.218}, {0.085, -0.2}, {0.085, 0.2}, {0.055, 0.218}}};
	kit.Body(Turned(tank, 12, 0.155).Paint(STEEL).Gloss(METAL_GLOSS));
	kit.Body(Barrel(-0.03, 0.03, 0.087, 0.155, 12).Paint(PRIMARY));
	kit.Fine(Column(8, 0.03, 0.026, 0.03).Transform(Mat4::Translation({0.0, 0.0, 0.232})).Paint(STEEL).Gloss(METAL_GLOSS));
	kit.Fine(Plank({-0.2, -0.105, 0.075}, {0.2, -0.1, 0.09}, RUNNING_GEAR));
	return kit.Done();
}

/* A low mound of loose cargo, heaped higher in the middle. */
static ModelMesh Heap(double half_length, double half_width, double base, double rise)
{
	std::array<LatheRing, 4> mound = {{{1.0, 0.0}, {0.78, 0.45}, {0.35, 0.85}, {0.0, 1.0}}};
	return Lathe(mound, 8).Transform(Mat4::Translation({0.0, 0.0, base}) * Mat4::Scaling({half_length, half_width, rise})).Displace(HEAP_SPREAD, HEAP_SEED).Facet().Paint(CARGO);
}

static ModelMesh OpenWagon(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Underframe(kit, 0.11);
	kit.Body(Plank({-0.225, -0.125, FLOOR}, {0.225, 0.125, 0.075}, DARK_PRIMARY));
	ModelMesh side = Plank({-0.225, 0.115, 0.075}, {0.225, 0.125, 0.17}, MUTED_PRIMARY);
	kit.Body(side.Append(Mirrored(side)));
	for (double x : {-0.225, 0.215}) kit.Body(Plank({x, -0.115, 0.075}, {x + 0.01, 0.115, 0.17}, MUTED_PRIMARY));
	kit.Body(Plank({-0.215, -0.115, 0.075}, {0.215, 0.115, 0.15}, CARGO));
	kit.Body(Heap(0.2, 0.105, 0.15, 0.045));
	for (double x : {-0.15, -0.075, 0.0, 0.075, 0.15}) kit.Fine(SideBand(x - 0.005, x + 0.005, 0.125, 0.075, 0.17, DARK_PRIMARY));
	return kit.Done();
}

static ModelMesh CoveredHopper(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Underframe(kit, 0.1);
	kit.Body(Hull({0.122, 0.105, 0.215, 0.05}, -0.215, 0.215).Paint(MUTED_PRIMARY).Gloss(PAINT_GLOSS));
	for (double x : {-0.13, 0.0, 0.13}) kit.Body(Hull({0.07, FLOOR, 0.105, 0.0}, x - 0.05, x + 0.05).Paint(DARK_PRIMARY));
	kit.Fine(Plank({-0.2, -0.02, 0.215}, {0.2, 0.02, 0.222}, ROOF_GREY));
	kit.Fine(SideBand(-0.215, 0.215, 0.122, 0.15, 0.162, SECONDARY));
	return kit.Done();
}

static ModelMesh FlatWagon(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Underframe(kit, 0.1);
	kit.Body(Plank({-0.225, -0.125, FLOOR}, {0.225, 0.125, 0.078}, TIMBER_DECK));
	kit.Body(SideBand(-0.225, 0.225, 0.125, 0.058, 0.074, DARK_PRIMARY));
	for (double y : {-0.07, 0.0, 0.07}) kit.Body(Barrel(-0.2, 0.2, 0.035, 0.113, 7).Transform(Mat4::Translation({0.0, y, 0.0})).Paint(CARGO));
	for (double y : {-0.035, 0.035}) kit.Fine(Barrel(-0.19, 0.19, 0.033, 0.175, 7).Transform(Mat4::Translation({0.0, y, 0.0})).Paint(BRIGHT_CARGO));
	for (double x : {-0.18, -0.06, 0.06, 0.18}) {
		ModelMesh stake = Plank({x - 0.006, 0.112, 0.078}, {x + 0.006, 0.124, 0.15}, RUNNING_GEAR);
		kit.Fine(stake.Append(Mirrored(stake)));
	}
	return kit.Done();
}

static ModelMesh LivestockVan(VehicleDetail detail)
{
	VehicleKit kit(detail);
	VanShell(kit, MUTED_PRIMARY);
	for (double z : {0.09, 0.13, 0.17}) kit.Fine(SideBand(-0.21, 0.21, VAN_BODY.half_width, z, z + 0.012, SLAT_GAP));
	kit.Fine(SideBand(-0.04, 0.04, VAN_BODY.half_width + 0.002, 0.07, 0.205, DARK_SECONDARY));
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
