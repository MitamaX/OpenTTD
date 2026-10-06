/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file aircraft_models.cpp Low poly propeller planes, jets and helicopters with the rotor turning over them. */

#include "../../stdafx.h"
#include "vehicle_parts.h"

#include <array>
#include <numbers>

#include "../../safeguards.h"

static constexpr Vec3 X_AXIS = {1.0, 0.0, 0.0};
static constexpr Vec3 Z_AXIS = {0.0, 0.0, 1.0};
static constexpr int FUSELAGE_SIDES = 10;
static constexpr int ROTOR_BLADES = 4;
static constexpr double ROTOR_HUB = 0.14;
static constexpr double ROTOR_REACH = 0.22;
static constexpr uint32_t WING = 0xC8CCD0;
static constexpr uint32_t PORT_LAMP = 0xFF3020;
static constexpr uint32_t STARBOARD_LAMP = 0x30FF60;
static constexpr uint32_t BLADE = 0x2E3034;

/* A round fuselage, white over a belly in the owner's second colour. */
static ModelMesh Fuselage(std::span<const LatheRing> profile, double axis_z)
{
	return Turned(profile, FUSELAGE_SIDES, axis_z).PaintBy([axis_z](const Vec3 &at) { return at.z < axis_z - 0.01 ? SECONDARY : CREAM; }).Gloss(PAINT_GLOSS);
}

/* A flat outline seen from above, counterclockwise, raised to a thin slab at a height, and its mirror across the middle. */
static ModelMesh Planform(std::span<const MapVector> outline, double z, double thickness, uint32_t tone)
{
	ModelMesh half = Extrusion(outline, thickness).Transform(Mat4::Translation({0.0, 0.0, z})).Paint(tone);
	return half.Append(Mirrored(half));
}

/* A flat outline seen from the side, counterclockwise with x along and the second axis up, stood upright on the middle. */
static ModelMesh Fin(std::span<const MapVector> outline, double thickness, uint32_t tone)
{
	return Extrusion(outline, thickness).Transform(Mat4::Translation({0.0, thickness * 0.5, 0.0}) * Mat4::Turn(X_AXIS, std::numbers::pi / 2.0)).Paint(tone).Gloss(PAINT_GLOSS);
}

static void WingtipLamps(VehicleKit &kit, double x, double span, double z)
{
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
	constexpr double AXIS = 0.065;
	std::array<LatheRing, 6> body = {{{0.0, -0.25}, {0.02, -0.2}, {0.038, -0.07}, {0.04, 0.12}, {0.032, 0.19}, {0.012, 0.225}}};
	kit.Body(Fuselage(body, AXIS));
	std::array<MapVector, 4> wing = {{{-0.02, 0.0}, {0.05, 0.0}, {0.045, 0.27}, {-0.01, 0.27}}};
	kit.Body(Planform(wing, 0.098, 0.008, WING));
	std::array<MapVector, 4> tailplane = {{{-0.245, 0.0}, {-0.2, 0.0}, {-0.215, 0.09}, {-0.245, 0.09}}};
	kit.Body(Planform(tailplane, 0.075, 0.006, WING));
	std::array<MapVector, 4> fin = {{{-0.25, 0.07}, {-0.19, 0.07}, {-0.23, 0.16}, {-0.255, 0.16}}};
	kit.Body(Fin(fin, 0.008, PRIMARY));
	for (double y : {-0.1, 0.1}) {
		kit.Body(Barrel(-0.02, 0.085, 0.017, 0.088, 8).Transform(Mat4::Translation({0.0, y, 0.0})).Paint(PRIMARY).Gloss(PAINT_GLOSS));
		kit.Fine(Plank({0.086, y - 0.04, 0.085}, {0.09, y + 0.04, 0.091}, BLADE));
		kit.Fine(Plank({0.086, y - 0.003, 0.048}, {0.09, y + 0.003, 0.128}, BLADE));
	}
	kit.Fine(Plank({0.17, -0.022, 0.085}, {0.2, 0.022, 0.1}, GLASS).Gloss(GLASS_GLOSS));
	WingtipLamps(kit, 0.015, 0.27, 0.098);
	Gear(kit, 0.17, -0.02, 0.06);
	return kit.Done();
}

static ModelMesh Jet(VehicleDetail detail)
{
	VehicleKit kit(detail);
	constexpr double AXIS = 0.075;
	std::array<LatheRing, 7> body = {{{0.0, -0.37}, {0.02, -0.33}, {0.045, -0.24}, {0.05, -0.15}, {0.05, 0.25}, {0.035, 0.32}, {0.012, 0.35}}};
	kit.Body(Fuselage(body, AXIS));
	std::array<MapVector, 4> wing = {{{-0.08, 0.03}, {0.09, 0.03}, {-0.07, 0.34}, {-0.115, 0.34}}};
	kit.Body(Planform(wing, 0.055, 0.01, WING));
	std::array<MapVector, 4> tailplane = {{{-0.36, 0.0}, {-0.28, 0.0}, {-0.34, 0.13}, {-0.37, 0.13}}};
	kit.Body(Planform(tailplane, 0.09, 0.006, WING));
	std::array<MapVector, 4> fin = {{{-0.37, 0.09}, {-0.27, 0.09}, {-0.34, 0.22}, {-0.375, 0.22}}};
	kit.Body(Fin(fin, 0.01, PRIMARY));
	for (double y : {-0.14, 0.14}) {
		kit.Body(Barrel(-0.04, 0.07, 0.022, 0.035, 8).Transform(Mat4::Translation({0.0, y, 0.0})).Paint(PRIMARY).Gloss(METAL_GLOSS));
		kit.Fine(Plank({-0.01, y - 0.003, 0.05}, {0.04, y + 0.003, 0.06}, WING));
	}
	ModelMesh windows = Plank({-0.25, 0.046, AXIS + 0.012}, {0.26, 0.051, AXIS + 0.02}, GLASS);
	kit.Fine(windows.Append(Mirrored(windows)).Gloss(GLASS_GLOSS));
	kit.Fine(Plank({0.29, -0.025, 0.09}, {0.32, 0.025, 0.105}, GLASS).Gloss(GLASS_GLOSS));
	WingtipLamps(kit, -0.09, 0.34, 0.055);
	Gear(kit, 0.27, -0.06, 0.07);
	return kit.Done();
}

static ModelMesh Helicopter(VehicleDetail detail)
{
	VehicleKit kit(detail);
	kit.Body(Icosphere(1).Transform(Mat4::Translation({0.02, 0.0, 0.075}) * Mat4::Scaling({0.1, 0.055, 0.05})).Facet().Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(Icosphere(1).Transform(Mat4::Translation({0.075, 0.0, 0.085}) * Mat4::Scaling({0.05, 0.045, 0.04})).Facet().Paint(GLASS).Gloss(GLASS_GLOSS));
	std::array<LatheRing, 2> boom = {{{0.018, -0.06}, {0.008, -0.27}}};
	kit.Body(Turned(boom, 6, 0.09).Paint(PRIMARY));
	std::array<MapVector, 4> fin = {{{-0.28, 0.08}, {-0.24, 0.08}, {-0.26, 0.15}, {-0.285, 0.15}}};
	kit.Body(Fin(fin, 0.006, SECONDARY));
	kit.Fine(Plank({-0.275, 0.006, 0.09}, {-0.255, 0.01, 0.14}, BLADE));
	for (double y : {-0.045, 0.045}) {
		kit.Fine(Plank({-0.07, y - 0.004, 0.0}, {0.1, y + 0.004, 0.008}, RUNNING_GEAR));
		for (double x : {-0.03, 0.06}) kit.Fine(Plank({x - 0.003, y - 0.003, 0.008}, {x + 0.003, y + 0.003, 0.04}, RUNNING_GEAR));
	}
	kit.Body(Prism(6, 0.008, ROTOR_HUB - 0.12).Transform(Mat4::Translation({0.0, 0.0, 0.12})).Paint(RUNNING_GEAR));
	kit.Fine(Plank({-0.27, -0.004, 0.155}, {-0.262, 0.004, 0.163}, PORT_LAMP).Glow(LAMP_GLOW));
	return kit.Done();
}

/* Blades about the hub at the origin's mast height, turned in place by their copy's yaw. */
static ModelMesh Rotor(VehicleDetail detail)
{
	VehicleKit kit(detail);
	ModelMesh blades;
	for (int blade = 0; blade < ROTOR_BLADES; blade++) {
		blades.Append(Plank({0.0, -0.007, ROTOR_HUB}, {ROTOR_REACH, 0.007, ROTOR_HUB + 0.003}, BLADE).Transform(Mat4::Turn(Z_AXIS, 2.0 * std::numbers::pi * blade / ROTOR_BLADES)));
	}
	kit.Body(blades);
	kit.Fine(Prism(6, 0.014, 0.012).Transform(Mat4::Translation({0.0, 0.0, ROTOR_HUB - 0.004})).Paint(RUNNING_GEAR));
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
