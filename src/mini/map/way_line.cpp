/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file way_line.cpp The middle lines ways are laid along over the map, straight or bending from one heading to another. */

#include "../../stdafx.h"
#include "way_line.h"

#include <algorithm>
#include <cmath>

#include "../../safeguards.h"

static constexpr int SPAN_STEPS = 8;
static constexpr int SHARE_PROBES = 8;
static constexpr int SHARE_REFINES = 3;

/* How long the end tangents stand, so that an even turn follows the circle through both ends. */
static double TangentLength(double chord, const MapVector &leaving, const MapVector &arriving)
{
	double turn = std::acos(std::clamp(Dot(leaving, arriving), -1.0, 1.0));
	double quarter = std::cos(turn * 0.25);
	return chord / (quarter * quarter);
}

/* The curve is kept as a cubic in its share, so a point, a heading or a turn along it costs a few products. */
Bend::Bend(const MapVector &from, const MapVector &to, const MapVector &leaving, const MapVector &arriving) : from(from), to(to)
{
	MapVector out = Unit(leaving);
	MapVector in = Unit(arriving);
	double length = TangentLength(std::hypot(to.x - from.x, to.y - from.y), out, in);
	this->leaving = out * length;
	this->arriving = in * length;
	this->cubic = (from - to) * 2.0 + this->leaving + this->arriving;
	this->square = (to - from) * 3.0 - this->leaving * 2.0 - this->arriving;
}

MapVector Bend::Centre(double share) const
{
	return ((this->cubic * share + this->square) * share + this->leaving) * share + this->from;
}

MapVector Bend::Velocity(double share) const
{
	return (this->cubic * (3.0 * share) + this->square * 2.0) * share + this->leaving;
}

MapVector Bend::Acceleration(double share) const
{
	return this->cubic * (6.0 * share) + this->square * 2.0;
}

MapVector Bend::Along(double share) const
{
	return Unit(this->Velocity(share));
}

MapVector Bend::At(double share, double lateral) const
{
	return this->Centre(share) + RightOf(this->Along(share)) * lateral;
}

double Bend::Span() const
{
	double span = 0.0;
	MapVector previous = this->from;
	for (int step = 1; step <= SPAN_STEPS; step++) {
		MapVector next = this->Centre(static_cast<double>(step) / SPAN_STEPS);
		span += std::hypot(next.x - previous.x, next.y - previous.y);
		previous = next;
	}
	return span;
}

/* The nearest of a few probes along the line, refined by Newton's steps toward where the line runs square to the point. */
double Bend::ShareOf(const MapVector &point) const
{
	auto distance = [&](double share) {
		MapVector off = this->Centre(share) - point;
		return Dot(off, off);
	};
	double share = 0.0;
	double nearest = distance(share);
	for (int probe = 1; probe <= SHARE_PROBES; probe++) {
		double candidate = static_cast<double>(probe) / SHARE_PROBES;
		double reach = distance(candidate);
		if (reach >= nearest) continue;
		share = candidate;
		nearest = reach;
	}
	for (int refine = 0; refine < SHARE_REFINES; refine++) {
		MapVector off = this->Centre(share) - point;
		MapVector velocity = this->Velocity(share);
		double slope = Dot(velocity, velocity) + Dot(off, this->Acceleration(share));
		if (slope <= 0.0) break;
		share = std::clamp(share - Dot(off, velocity) / slope, 0.0, 1.0);
	}
	return share;
}
