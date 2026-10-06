/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file volume_light.h The daylight on a building's faces, lit by the same sun and relief as the ground. */

#ifndef MINI_MAP_VOLUME_LIGHT_H
#define MINI_MAP_VOLUME_LIGHT_H

#include "../core/camera.h"
#include "../core/sunlight.h"
#include "volume_geometry.h"

class Daylight {
public:
	static Daylight Now();
	double Light(const Vec3 &unit_normal) const;
	double Glass(double light) const;

private:
	double SkyFill(const Vec3 &unit_normal) const;

	Vec3 sun{};
	MapVector heading{};
	double relief = 1.0;
};

#endif /* MINI_MAP_VOLUME_LIGHT_H */
