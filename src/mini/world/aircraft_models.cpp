/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file aircraft_models.cpp Low poly propeller planes, jets and helicopters with the rotor turning over them. */

#include "../../stdafx.h"
#include "vehicle_parts.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "../../safeguards.h"

static constexpr int FUSELAGE_SIDES = 10;
static constexpr int NACELLE_SIDES = 8;
static constexpr int ROTOR_BLADES = 4;
static constexpr double ROTOR_HUB = 0.14;
static constexpr double ROTOR_REACH = 0.22;
static constexpr double TIP_THICKNESS = 0.4;
static constexpr double WING_DIHEDRAL = 0.08;
static constexpr double INTAKE_SHARE = 0.8;
static constexpr double INTAKE_DEPTH = 0.002;
static constexpr double BLADE_HALF_WIDTH = 0.003;
static constexpr double BLADE_DEPTH = 0.003;
static constexpr uint32_t WING = 0xC8CCD0;
static constexpr uint32_t PORT_LAMP = 0xFF3020;
static constexpr uint32_t STARBOARD_LAMP = 0x30FF60;

static constexpr double PROP_AXIS = 0.065;
static constexpr double PROP_WING_FOOT = 0.098;
static constexpr double PROP_WING_SPAN = 0.27;
static constexpr std::array<LatheRing, 6> PROP_FUSELAGE = {{{0.0, -0.25}, {0.02, -0.2}, {0.038, -0.07}, {0.04, 0.12}, {0.032, 0.19}, {0.012, 0.225}}};
static constexpr std::array<LatheRing, 5> PROP_NACELLE = {{{0.004, -0.04}, {0.012, -0.02}, {0.017, 0.01}, {0.017, 0.06}, {0.013, 0.082}}};
static constexpr double PROP_NACELLE_AXIS = 0.088;
static constexpr double PROP_NACELLE_SIDE = 0.1;
static constexpr std::array<LatheRing, 2> SPINNER = {{{0.009, 0.082}, {0.0, 0.098}}};
static constexpr int PROP_BLADES = 3;
static constexpr double PROP_REACH = 0.042;

static constexpr double JET_AXIS = 0.075;
static constexpr double JET_WING_FOOT = 0.055;
static constexpr double JET_WING_SPAN = 0.34;
static constexpr std::array<LatheRing, 7> JET_FUSELAGE = {{{0.0, -0.37}, {0.02, -0.33}, {0.045, -0.24}, {0.05, -0.15}, {0.05, 0.25}, {0.035, 0.32}, {0.012, 0.35}}};
static constexpr std::array<LatheRing, 5> JET_NACELLE = {{{0.012, -0.05}, {0.017, -0.035}, {0.022, 0.0}, {0.022, 0.05}, {0.02, 0.07}}};
static constexpr double JET_NACELLE_AXIS = 0.035;
static constexpr double JET_NACELLE_SIDE = 0.14;

static constexpr double HELI_AXIS = 0.075;
static constexpr std::array<LatheRing, 5> HELI_POD = {{{0.022, -0.09}, {0.04, -0.065}, {0.05, -0.03}, {0.052, 0.01}, {0.05, 0.045}}};
static constexpr std::array<LatheRing, 4> HELI_CANOPY = {{{0.05, 0.045}, {0.043, 0.08}, {0.026, 0.108}, {0.0, 0.12}}};
static constexpr double HELI_POD_WIDTH = 1.1;
static constexpr HullProfile HELI_COWL = {0.024, 0.1, 0.14, 0.014};
static constexpr double HELI_COWL_BACK = -0.075;
static constexpr double HELI_COWL_FRONT = -0.012;
static constexpr BodyEnd HELI_COWL_ENDS = RoundedEnd(0.015, 0.7, 0.8);
static constexpr Vec3 TAIL_ROTOR = {-0.268, 0.012, 0.13};
static constexpr double TAIL_ROTOR_REACH = 0.028;
static constexpr int TAIL_ROTOR_BLADES = 2;
static constexpr int BOOM_SIDES = 6;
static constexpr int HUB_SIDES = 6;

/* A round fuselage in a body colour over a belly in the owner's second colour. */
static ModelMesh Fuselage(std::span<const LatheRing> profile, double axis_z, uint32_t body)
{
	return Turned(profile, FUSELAGE_SIDES, axis_z).PaintBy([axis_z, body](const Vec3 &at) { return at.z < axis_z - 0.01 ? SECONDARY : body; }).Gloss(PAINT_GLOSS);
}

/* A flat outline seen from above, counterclockwise, raised to a slab at a height that thins toward its tip and rises out toward it by the dihedral, and its mirror across the middle. */
static ModelMesh Planform(std::span<const MapVector> outline, double z, double thickness, double dihedral, uint32_t tone)
{
	double span = std::ranges::max(outline, {}, &MapVector::y).y;
	ModelMesh half = Extrusion(outline, thickness).Reshape([&](const Vec3 &at) {
		double out = at.y / span;
		return Vec3{at.x, at.y, z + at.z * std::lerp(1.0, TIP_THICKNESS, out) + at.y * dihedral};
	});
	return Flanked(half.Facet().Paint(tone));
}

/* A flat outline seen from the side, counterclockwise with x along and the second axis up, stood upright on the middle. */
static ModelMesh Fin(std::span<const MapVector> outline, double thickness, uint32_t tone)
{
	return Upright(outline, thickness).Paint(tone).Gloss(PAINT_GLOSS);
}

/* An engine's cowling turned about its own axis out to one side, its intake dark inside the lip at its front, and its mirror across the middle. */
static ModelMesh Nacelle(std::span<const LatheRing> profile, double side, double axis_z)
{
	const LatheRing &lip = profile.back();
	ModelMesh nacelle = Turned(profile, NACELLE_SIDES, axis_z).Paint(PRIMARY).Gloss(METAL_GLOSS);
	nacelle.Append(Barrel(lip.z, lip.z + INTAKE_DEPTH, lip.radius * INTAKE_SHARE, axis_z, NACELLE_SIDES).Paint(RUNNING_GEAR));
	return Flanked(nacelle.Move({0.0, side, 0.0}));
}

/* Blades evenly about a hub, turning about the x axis through the origin, their faces square to it. */
static ModelMesh Propeller(int blades, double reach)
{
	ModelMesh propeller;
	for (int blade = 0; blade < blades; blade++) {
		ModelMesh arm = Plank({-BLADE_DEPTH * 0.5, -BLADE_HALF_WIDTH, 0.0}, {BLADE_DEPTH * 0.5, BLADE_HALF_WIDTH, reach}, TRIM);
		propeller.Append(arm.Transform(Mat4::Turn(X_AXIS, 2.0 * std::numbers::pi * blade / blades)));
	}
	return propeller.Gloss(TRIM_GLOSS);
}

static void WingtipLamps(VehicleKit &kit, double x, double span, double wing_foot)
{
	double z = wing_foot + span * WING_DIHEDRAL;
	kit.Fine(Plank({x - 0.006, span - 0.008, z}, {x + 0.006, span, z + 0.008}, PORT_LAMP).Glow(LAMP_GLOW));
	kit.Fine(Plank({x - 0.006, -span, z}, {x + 0.006, -span + 0.008, z + 0.008}, STARBOARD_LAMP).Glow(LAMP_GLOW));
}

static void Gear(VehicleKit &kit, double nose_x, double main_x, double half_track)
{
	kit.Fine(Wheel(nose_x, 0.0, 0.012, 0.01));
	kit.Fine(WheelPair(main_x, half_track, 0.014, 0.012));
}

static ModelMesh PropPlane(VehicleDetail detail)
{
	VehicleKit kit(detail);
	kit.Body(Fuselage(PROP_FUSELAGE, PROP_AXIS, CREAM));
	std::array<MapVector, 4> wing = {{{-0.02, 0.0}, {0.05, 0.0}, {0.045, PROP_WING_SPAN}, {-0.01, PROP_WING_SPAN}}};
	kit.Body(Planform(wing, PROP_WING_FOOT, 0.008, WING_DIHEDRAL, WING));
	std::array<MapVector, 4> tailplane = {{{-0.245, 0.0}, {-0.2, 0.0}, {-0.215, 0.09}, {-0.245, 0.09}}};
	kit.Body(Planform(tailplane, 0.075, 0.006, 0.0, WING));
	std::array<MapVector, 4> fin = {{{-0.25, 0.07}, {-0.19, 0.07}, {-0.23, 0.16}, {-0.255, 0.16}}};
	kit.Body(Fin(fin, 0.008, PRIMARY));
	kit.Body(Nacelle(PROP_NACELLE, PROP_NACELLE_SIDE, PROP_NACELLE_AXIS));
	kit.Fine(Flanked(Turned(SPINNER, NACELLE_SIDES, PROP_NACELLE_AXIS).Move({0.0, PROP_NACELLE_SIDE, 0.0}).Paint(STEEL).Gloss(METAL_GLOSS)));
	kit.Fine(Flanked(Propeller(PROP_BLADES, PROP_REACH).Move({SPINNER.front().z, PROP_NACELLE_SIDE, PROP_NACELLE_AXIS})));
	kit.Fine(Plank({0.17, -0.022, 0.085}, {0.2, 0.022, 0.1}, GLASS).Gloss(GLASS_GLOSS));
	WingtipLamps(kit, 0.015, PROP_WING_SPAN, PROP_WING_FOOT);
	Gear(kit, 0.17, -0.02, 0.06);
	return kit.Done();
}

static ModelMesh Jet(VehicleDetail detail)
{
	VehicleKit kit(detail);
	kit.Body(Fuselage(JET_FUSELAGE, JET_AXIS, CREAM));
	std::array<MapVector, 4> wing = {{{-0.08, 0.03}, {0.09, 0.03}, {-0.07, JET_WING_SPAN}, {-0.115, JET_WING_SPAN}}};
	kit.Body(Planform(wing, JET_WING_FOOT, 0.01, WING_DIHEDRAL, WING));
	std::array<MapVector, 4> tailplane = {{{-0.36, 0.0}, {-0.28, 0.0}, {-0.34, 0.13}, {-0.37, 0.13}}};
	kit.Body(Planform(tailplane, 0.09, 0.006, WING_DIHEDRAL, WING));
	std::array<MapVector, 4> fin = {{{-0.37, 0.09}, {-0.27, 0.09}, {-0.34, 0.22}, {-0.375, 0.22}}};
	kit.Body(Fin(fin, 0.01, PRIMARY));
	kit.Body(Nacelle(JET_NACELLE, JET_NACELLE_SIDE, JET_NACELLE_AXIS));
	kit.Fine(Flanked(Plank({-0.01, JET_NACELLE_SIDE - 0.003, JET_NACELLE_AXIS + 0.015}, {0.045, JET_NACELLE_SIDE + 0.003, JET_WING_FOOT + JET_NACELLE_SIDE * WING_DIHEDRAL + 0.003}, WING)));
	kit.Fine(Flanked(Plank({-0.25, 0.046, JET_AXIS + 0.012}, {0.26, 0.051, JET_AXIS + 0.02}, GLASS)).Gloss(GLASS_GLOSS));
	kit.Fine(Plank({0.29, -0.025, 0.09}, {0.32, 0.025, 0.105}, GLASS).Gloss(GLASS_GLOSS));
	WingtipLamps(kit, -0.09, JET_WING_SPAN, JET_WING_FOOT);
	Gear(kit, 0.27, -0.06, 0.07);
	return kit.Done();
}

/* A rounded pod glazed all over its nose, a cowling over its engine behind the mast, and a boom out to a fin, a tail rotor and a small tailplane. */
static ModelMesh Helicopter(VehicleDetail detail)
{
	VehicleKit kit(detail);
	Mat4 widened = Mat4::Scaling({1.0, HELI_POD_WIDTH, 1.0});
	kit.Body(Fuselage(HELI_POD, HELI_AXIS, PRIMARY).Transform(widened));
	kit.Body(Turned(HELI_CANOPY, FUSELAGE_SIDES, HELI_AXIS).Transform(widened).Paint(GLASS).Gloss(GLASS_GLOSS));
	kit.Body(BodyRun(HELI_COWL, HELI_COWL_BACK, HELI_COWL_FRONT, HELI_COWL_ENDS, HELI_COWL_ENDS, kit.Steps()).Shell().Paint(PRIMARY).Gloss(PAINT_GLOSS));
	std::array<LatheRing, 2> boom = {{{0.008, -0.27}, {0.018, -0.06}}};
	kit.Body(Turned(boom, BOOM_SIDES, 0.09).Paint(PRIMARY).Gloss(PAINT_GLOSS));
	std::array<MapVector, 4> fin = {{{-0.28, 0.08}, {-0.24, 0.08}, {-0.26, 0.15}, {-0.285, 0.15}}};
	kit.Body(Fin(fin, 0.006, SECONDARY));
	std::array<MapVector, 4> tailplane = {{{-0.225, 0.0}, {-0.2, 0.0}, {-0.205, 0.035}, {-0.222, 0.035}}};
	kit.Fine(Planform(tailplane, 0.088, 0.004, 0.0, SECONDARY));
	kit.Fine(Propeller(TAIL_ROTOR_BLADES, TAIL_ROTOR_REACH).Transform(Mat4::Translation(TAIL_ROTOR) * Mat4::Turn(UP, QUARTER_TURN)));
	for (double y : {-0.045, 0.045}) {
		kit.Fine(Plank({-0.07, y - 0.004, 0.0}, {0.1, y + 0.004, 0.008}, RUNNING_GEAR));
		for (double x : {-0.03, 0.06}) kit.Fine(Plank({x - 0.003, y - 0.003, 0.008}, {x + 0.003, y + 0.003, 0.04}, RUNNING_GEAR));
	}
	kit.Body(Prism(HUB_SIDES, 0.008, ROTOR_HUB - 0.12).Transform(Mat4::Translation({0.0, 0.0, 0.12})).Paint(RUNNING_GEAR));
	kit.Fine(Plank({-0.27, -0.004, 0.155}, {-0.262, 0.004, 0.163}, PORT_LAMP).Glow(LAMP_GLOW));
	return kit.Done();
}

/* Blades about the hub at the origin's mast height, turned in place by their copy's yaw. */
static ModelMesh Rotor(VehicleDetail detail)
{
	VehicleKit kit(detail);
	ModelMesh blades;
	for (int blade = 0; blade < ROTOR_BLADES; blade++) {
		blades.Append(Plank({0.0, -0.007, ROTOR_HUB}, {ROTOR_REACH, 0.007, ROTOR_HUB + 0.003}, TRIM).Transform(Mat4::Turn(UP, 2.0 * std::numbers::pi * blade / ROTOR_BLADES)));
	}
	kit.Body(blades);
	kit.Fine(Prism(HUB_SIDES, 0.014, 0.012).Transform(Mat4::Translation({0.0, 0.0, ROTOR_HUB - 0.004})).Paint(RUNNING_GEAR));
	return kit.Done();
}

ModelMesh AircraftModel(VehicleLook look, VehicleDetail detail)
{
	switch (look) {
		case VehicleLook::PropPlane: return PropPlane(detail);
		case VehicleLook::Jet: return Jet(detail);
		case VehicleLook::Helicopter: return Helicopter(detail);
		default: return Rotor(detail);
	}
}
