/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sunlight.cpp The sun over the mini map, standing at one bearing over the map. */

#include "../../stdafx.h"
#include "sunlight.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "camera.h"
#include "tuning.h"

#include "../../safeguards.h"

static constexpr double LOWEST_SUN_RISE = 0.1;

Vec3 Sun()
{
	double elevation = _tuning.sun_elevation * std::numbers::pi / 180.0;
	Vec3 level = Bearing(_tuning.sun_bearing) * std::cos(elevation);
	return {level.x, level.y, std::sin(elevation)};
}

double SunRise(const Vec3 &sun)
{
	return std::max(sun.z, LOWEST_SUN_RISE);
}
