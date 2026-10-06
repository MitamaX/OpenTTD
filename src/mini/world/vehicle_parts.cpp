/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_parts.cpp The parts vehicle models share: bodies laid along their length, noses, wheels, bogies, windows and lamps. */

#include "../../stdafx.h"
#include "vehicle_parts.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include "../../safeguards.h"

static constexpr Vec3 X_AXIS = {1.0, 0.0, 0.0};
static constexpr Vec3 Y_AXIS = {0.0, 1.0, 0.0};
static constexpr int NOSE_RINGS = 4;
static constexpr int WHEEL_SIDES = 8;
static constexpr double PROUD = 0.003;
static constexpr double PANE_DEPTH = 0.004;
static constexpr double WINDOW_SHARE = 0.72;
static constexpr double BOGIE_WHEELBASE = 0.034;
static constexpr double BOGIE_WIDTH = 0.012;
static constexpr double LAMP_DEPTH = 0.004;

/* Turns a solid built upright along z to lie along x, keeping its section's first axis across and its second upright. */
static Mat4 LyingAlongX()
{
	Mat4 turn = Mat4::Identity();
	turn.At(0, 0) = 0.0;
	turn.At(1, 0) = 1.0;
	turn.At(2, 1) = 1.0;
	turn.At(1, 1) = 0.0;
	turn.At(0, 2) = 1.0;
	turn.At(2, 2) = 0.0;
	return turn;
}

ModelMesh Hull(const HullProfile &profile, double back, double front)
{
	double w = profile.half_width;
	double bevel = std::min(profile.bevel, std::min(w, profile.high - profile.low) * 0.5);
	std::array<MapVector, 6> section = {{
		{-w, profile.low}, {w, profile.low}, {w, profile.high - bevel},
		{w - bevel, profile.high}, {-w + bevel, profile.high}, {-w, profile.high - bevel},
	}};
	return Extrusion(section, front - back).Transform(Mat4::Translation({back, 0.0, 0.0}) * LyingAlongX());
}

ModelMesh SideBand(double back, double front, double half_width, double low, double high, uint32_t tone)
{
	return Hull({half_width + PROUD - 0.0005, low, high, 0.0}, back, front).Paint(tone);
}

ModelMesh Nose(const HullProfile &profile, double from, double to, const NoseShape &shape, double base)
{
	ModelMesh nose;
	for (int ring = 0; ring < NOSE_RINGS; ring++) {
		nose.Append(Hull(profile, std::lerp(from, to, static_cast<double>(ring) / NOSE_RINGS), std::lerp(from, to, static_cast<double>(ring + 1) / NOSE_RINGS)));
	}
	for (ModelVertex &vertex : nose.vertices) {
		Vec3 at = vertex.Position();
		double t = std::pow(std::clamp((at.x - from) / (to - from), 0.0, 1.0), shape.curve);
		vertex.Place({at.x, at.y * std::lerp(1.0, shape.width, t), base + (at.z - base) * std::lerp(1.0, shape.height, t)});
	}
	return nose.Facet();
}

ModelMesh Turned(std::span<const LatheRing> profile, int sides, double axis_z)
{
	std::vector<LatheRing> upright(profile.begin(), profile.end());
	return Lathe(upright, sides).Transform(Mat4::Translation({0.0, 0.0, axis_z}) * Mat4::Turn(Y_AXIS, std::numbers::pi / 2.0));
}

ModelMesh Barrel(double back, double front, double radius, double axis_z, int sides)
{
	std::array<LatheRing, 2> profile = {{{radius, back}, {radius, front}}};
	return Turned(profile, sides, axis_z);
}

ModelMesh Plank(const Vec3 &low, const Vec3 &high, uint32_t tone)
{
	return Box(low, high).Paint(tone);
}

ModelMesh Wheel(double x, double y, double radius, double width)
{
	return Prism(WHEEL_SIDES, radius, width).Transform(Mat4::Translation({x, y + width * 0.5, radius}) * Mat4::Turn(X_AXIS, std::numbers::pi / 2.0)).Paint(TYRE);
}

ModelMesh WheelPair(double x, double half_track, double radius, double width)
{
	ModelMesh pair = Wheel(x, half_track, radius, width);
	pair.Append(Wheel(x, -half_track, radius, width));
	return pair;
}

ModelMesh Bogie(double x, double half_track, double radius)
{
	double reach = BOGIE_WHEELBASE + radius;
	ModelMesh bogie = Plank({x - reach, half_track + BOGIE_WIDTH * 0.5, radius * 0.5}, {x + reach, half_track + BOGIE_WIDTH * 1.5, radius * 1.5}, RUNNING_GEAR);
	bogie.Append(Mirrored(bogie));
	bogie.Append(Plank({x - radius * 0.5, -half_track, radius * 1.2}, {x + radius * 0.5, half_track, radius * 2.0}, RUNNING_GEAR));
	for (double axle : {x - BOGIE_WHEELBASE, x + BOGIE_WHEELBASE}) bogie.Append(WheelPair(axle, half_track, radius, BOGIE_WIDTH));
	return bogie;
}

ModelMesh WindowRow(double back, double front, double low, double high, double half_width, int count)
{
	ModelMesh row;
	double pitch = (front - back) / count;
	double half = pitch * WINDOW_SHARE * 0.5;
	for (int window = 0; window < count; window++) {
		double middle = back + pitch * (window + 0.5);
		row.Append(Box({middle - half, half_width - PANE_DEPTH, low}, {middle + half, half_width + PROUD, high}));
	}
	row.Append(Mirrored(row));
	return row.Paint(GLASS).Gloss(GLASS_GLOSS);
}

ModelMesh Windscreen(double x, double half_width, double low, double high)
{
	return Box({x - PANE_DEPTH, -half_width, low}, {x + PROUD, half_width, high}).Paint(GLASS).Gloss(GLASS_GLOSS);
}

ModelMesh LampPair(double x, double half_spacing, double z, double half, uint32_t tone)
{
	double facing = x >= 0.0 ? 1.0 : -1.0;
	double outer = x + facing * LAMP_DEPTH;
	ModelMesh lamp = Box({std::min(x, outer), half_spacing - half, z - half}, {std::max(x, outer), half_spacing + half, z + half});
	lamp.Append(Mirrored(lamp));
	return lamp.Paint(tone).Glow(LAMP_GLOW);
}

ModelMesh Mirrored(const ModelMesh &part)
{
	ModelMesh mirrored = part;
	return mirrored.Transform(Mat4::Scaling({1.0, -1.0, 1.0}));
}
