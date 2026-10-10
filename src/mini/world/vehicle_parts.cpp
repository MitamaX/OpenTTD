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
#include <functional>
#include <iterator>
#include <numbers>
#include <vector>

#include "../../safeguards.h"

static constexpr Vec3 X_AXIS = {1.0, 0.0, 0.0};
static constexpr Vec3 Y_AXIS = {0.0, 1.0, 0.0};
static constexpr int NOSE_RINGS = 4;
static constexpr double QUARTER_TURN = std::numbers::pi * 0.5;
static constexpr int WHEEL_SIDES = 8;
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

static double BevelOf(const HullProfile &profile)
{
	return std::min(profile.bevel, std::min(profile.half_width, profile.high - profile.low) * 0.5);
}

static std::array<MapVector, 6> ChamferSection(const HullProfile &profile)
{
	double w = profile.half_width;
	double bevel = BevelOf(profile);
	return {{
		{-w, profile.low}, {w, profile.low}, {w, profile.high - bevel},
		{w - bevel, profile.high}, {-w + bevel, profile.high}, {-w, profile.high - bevel},
	}};
}

static std::vector<LoftRing> TaperRings(double from, double to, const NoseShape &shape)
{
	std::vector<LoftRing> rings;
	for (int ring = 0; ring <= NOSE_RINGS; ring++) {
		double along = static_cast<double>(ring) / NOSE_RINGS;
		double taper = std::pow(along, shape.curve);
		rings.push_back({std::lerp(from, to, along), std::lerp(1.0, shape.width, taper), std::lerp(1.0, shape.height, taper)});
	}
	return rings;
}

static void RoundCorner(std::vector<MapVector> &section, const MapVector &centre, double radius, double from, int arc)
{
	if (radius <= 0.0) {
		section.insert(section.end(), 2, centre);
		return;
	}
	for (int step = 0; step <= arc; step++) {
		double angle = from + QUARTER_TURN * step / arc;
		section.push_back({centre.x + radius * std::cos(angle), centre.y + radius * std::sin(angle)});
	}
}

static std::vector<MapVector> RoundedSection(const HullProfile &profile, int arc)
{
	double w = profile.half_width;
	double radius = BevelOf(profile);
	std::vector<MapVector> section(2, {-w, profile.low});
	section.insert(section.end(), 2, {w, profile.low});
	RoundCorner(section, {w - radius, profile.high - radius}, radius, 0.0, arc);
	RoundCorner(section, {radius - w, profile.high - radius}, radius, QUARTER_TURN, arc);
	return section;
}

static std::vector<LoftRing> EndRings(double tip, double inward, const BodyEnd &end, ShellSteps steps)
{
	if (end.reach <= 0.0) return {{tip, 1.0, 1.0}};
	if (end.curve == EndCurve::Tapered) {
		std::vector<LoftRing> rings = TaperRings(tip + inward * end.reach, tip, end.shape);
		std::ranges::reverse(rings);
		return rings;
	}
	if (steps.end <= 0) return {{tip, 1.0, 1.0}};
	std::vector<LoftRing> rings;
	for (int step = 0; step <= steps.end; step++) {
		double angle = QUARTER_TURN * step / steps.end;
		double rounded = std::sin(angle);
		rings.push_back({tip + inward * end.reach * (1.0 - std::cos(angle)), std::lerp(end.shape.width, 1.0, rounded), std::lerp(end.shape.height, 1.0, rounded)});
	}
	return rings;
}

BodyRun::BodyRun(const HullProfile &profile, double back, double front, const BodyEnd &back_end, const BodyEnd &front_end, ShellSteps steps) :
	BodyRun(profile, back, front, back_end, front_end, profile.low, steps)
{
}

BodyRun::BodyRun(const HullProfile &profile, double back, double front, const BodyEnd &back_end, const BodyEnd &front_end, double foot, ShellSteps steps) :
	profile(profile), foot(foot), steps(steps), rings(EndRings(back, 1.0, back_end, steps))
{
	std::vector<LoftRing> front_rings = EndRings(front, -1.0, front_end, steps);
	this->rings.insert(this->rings.end(), front_rings.rbegin(), front_rings.rend());
	assert(std::ranges::is_sorted(this->rings, std::less<>{}, &LoftRing::x));
}

LoftRing BodyRun::RingAt(double x) const
{
	auto above = std::ranges::upper_bound(this->rings, x, std::less<>{}, &LoftRing::x);
	if (above == this->rings.begin()) return {x, above->across, above->up};
	auto below = std::prev(above);
	if (above == this->rings.end()) return {x, below->across, below->up};
	double share = (x - below->x) / (above->x - below->x);
	return {x, std::lerp(below->across, above->across, share), std::lerp(below->up, above->up, share)};
}

std::vector<LoftRing> BodyRun::RingsOver(double from, double to) const
{
	std::vector<LoftRing> over = {this->RingAt(from)};
	for (const LoftRing &ring : this->rings) {
		if (ring.x > from && ring.x < to) over.push_back(ring);
	}
	over.push_back(this->RingAt(to));
	return over;
}

ModelMesh BodyRun::Lofted(const HullProfile &section, std::span<const LoftRing> rings) const
{
	return Loft(RoundedSection(section, this->steps.arc), this->foot, rings);
}

ModelMesh BodyRun::Laid(const HullProfile &section, double from, double to) const
{
	return this->Lofted(section, this->RingsOver(from, to));
}

ModelMesh BodyRun::Over(const HullProfile &section, double from, double to) const
{
	std::vector<LoftRing> over = this->RingsOver(from, to);
	if (from <= this->Back()) over.front().x -= PROUD;
	if (to >= this->Front()) over.back().x += PROUD;
	return this->Lofted(section, over);
}

ModelMesh BodyRun::Shell() const
{
	return this->Laid(this->profile, this->Back(), this->Front());
}

ModelMesh BodyRun::Stripe(const Band &band, uint32_t tone) const
{
	return this->Stripe(band, this->Back(), this->Front(), tone);
}

ModelMesh BodyRun::Stripe(const Band &band, double from, double to, uint32_t tone) const
{
	assert(band.low >= this->profile.low && band.high <= this->profile.high - BevelOf(this->profile));
	return this->Over({this->profile.half_width + PROUD, band.low, band.high, 0.0}, from, to).Paint(tone);
}

double BodyRun::HalfWidthAt(double x) const
{
	return this->profile.half_width * this->RingAt(x).across;
}

ModelMesh Hull(const HullProfile &profile, double back, double front)
{
	return Extrusion(ChamferSection(profile), front - back).Transform(Mat4::Translation({back, 0.0, 0.0}) * LyingAlongX());
}

ModelMesh SideBand(double back, double front, double half_width, double low, double high, uint32_t tone)
{
	return Hull({half_width + PROUD - 0.0005, low, high, 0.0}, back, front).Paint(tone);
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

ModelMesh Upright(std::span<const MapVector> outline, double thickness)
{
	return Extrusion(outline, thickness).Transform(Mat4::Translation({0.0, thickness * 0.5, 0.0}) * Mat4::Turn(X_AXIS, std::numbers::pi / 2.0));
}

static ModelMesh Disc(double x, double y, double axle, double radius, double width)
{
	return Prism(WHEEL_SIDES, radius, width).Transform(Mat4::Translation({x, y + width * 0.5, axle}) * Mat4::Turn(X_AXIS, std::numbers::pi / 2.0));
}

ModelMesh Wheel(double x, double y, double radius, double width)
{
	return Disc(x, y, radius, radius, width).Paint(TYRE);
}

ModelMesh Hub(double x, double y, double axle, double radius, double width)
{
	return Disc(x, y, axle, radius, width).Paint(STEEL).Gloss(METAL_GLOSS);
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

double WindowSpan::Pitch() const
{
	return (this->front - this->back) / this->count;
}

double WindowSpan::Post() const
{
	return this->Pitch() * (1.0 - WINDOW_SHARE);
}

double WindowSpan::GlassBack() const
{
	return this->back + this->Post() * 0.5;
}

double WindowSpan::GlassFront() const
{
	return this->front - this->Post() * 0.5;
}

ModelMesh WindowGlass(const WindowSpan &span, const Band &band, double half_width)
{
	return SideBand(span.GlassBack(), span.GlassFront(), half_width, band.low, band.high, GLASS).Gloss(GLASS_GLOSS);
}

ModelMesh WindowPosts(const WindowSpan &span, const Band &band, double half_width, uint32_t tone)
{
	ModelMesh posts;
	double half = span.Post() * 0.5;
	for (int post = 1; post < span.count; post++) {
		double at = span.back + span.Pitch() * post;
		posts.Append(SideBand(at - half, at + half, half_width, band.low, band.high, tone));
	}
	return posts;
}

ModelMesh Ribs(std::span<const double> at, double half_width, const Band &band, double width, uint32_t tone)
{
	ModelMesh ribs;
	for (double x : at) ribs.Append(SideBand(x - width * 0.5, x + width * 0.5, half_width, band.low, band.high, tone));
	return ribs;
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
