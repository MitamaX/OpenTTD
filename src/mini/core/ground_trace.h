/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ground_trace.h Where a sight line through the 3D world first meets the map's ground. */

#ifndef MINI_CORE_GROUND_TRACE_H
#define MINI_CORE_GROUND_TRACE_H

#include <optional>

#include "space.h"

/* The origin and the direction are in render space, where a level stands rise tiles high; the hit comes back in levels. */
struct SightLine {
	Vec3 origin;
	Vec3 direction;
	double rise;

	Vec3 At(double t) const { return this->origin + this->direction * t; }
};

struct GroundHit {
	double x;
	double y;
	double level;
};

/* A wall between two tiles belongs to the higher one, so a sight line meeting it lands on that tile's edge. */
std::optional<GroundHit> TraceGround(const SightLine &sight, double reach);

#endif /* MINI_CORE_GROUND_TRACE_H */
