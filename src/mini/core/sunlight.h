/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sunlight.h The sun over the mini map, standing at one bearing over the map. */

#ifndef MINI_CORE_SUNLIGHT_H
#define MINI_CORE_SUNLIGHT_H

#include "space.h"

/* The unit way to the sun along map X, map Y and straight up, from the elevation and the bearing from map north the tuning names. */
Vec3 Sun();
double SunRise(const Vec3 &sun);

#endif /* MINI_CORE_SUNLIGHT_H */
