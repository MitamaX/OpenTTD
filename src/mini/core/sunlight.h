/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sunlight.h The sun over the mini map, standing still in the sky whichever way the view faces. */

#ifndef MINI_CORE_SUNLIGHT_H
#define MINI_CORE_SUNLIGHT_H

struct SunVector {
	double x;
	double y;
	double z;
};

/* The unit way to the sun along map X, map Y and straight up: an afternoon sun low enough for steep hills to cast long shadows, lighting a north facing view from ahead and to the left. */
inline constexpr double SUN_X = -0.263;
inline constexpr double SUN_Y = -0.835;
inline constexpr double SUN_UP = 0.485;

SunVector Sun();

#endif /* MINI_CORE_SUNLIGHT_H */
