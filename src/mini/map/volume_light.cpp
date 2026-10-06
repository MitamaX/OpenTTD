/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file volume_light.cpp The daylight on a building's faces, lit by the same sun and relief as the ground. */

#include "../../stdafx.h"
#include "volume_light.h"

#include <algorithm>
#include <cmath>

#include "../../safeguards.h"

static constexpr double WALL_BOUNCE = 0.12;
static constexpr double GLASS_SKY_REFLECT = 0.35;

Daylight Daylight::Now()
{
	SunVector sun = Sun();
	double across = std::hypot(sun.x, sun.y);
	Daylight daylight;
	daylight.sun = {sun.x, sun.y, sun.z};
	daylight.heading = {sun.x / across, sun.y / across};
	daylight.relief = ReliefShare();
	return daylight;
}

/* The ground shader's sunlight, with the sky's fill on upright faces so the two walls in view part by how far they turn toward the sun. */
double Daylight::Light(const Vec3 &unit_normal) const
{
	double direct = std::min(std::max(Dot(unit_normal, this->sun), 0.0) / this->sun.z, SUNLIT_CEILING);
	double lit = AMBIENT_LIGHT + this->SkyFill(unit_normal) + (1.0 - AMBIENT_LIGHT) * direct;
	return std::max(std::lerp(1.0, lit, this->relief), 0.0);
}

double Daylight::SkyFill(const Vec3 &unit_normal) const
{
	if (!IsUpright(unit_normal)) return 0.0;
	return WALL_BOUNCE * (1.0 + unit_normal.x * this->heading.x + unit_normal.y * this->heading.y);
}

double Daylight::Glass(double light) const
{
	return std::lerp(light, 1.0, GLASS_SKY_REFLECT);
}
