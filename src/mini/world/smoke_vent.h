/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file smoke_vent.h Where smoke or steam leaves a stack or a funnel. */

#ifndef MINI_WORLD_SMOKE_VENT_H
#define MINI_WORLD_SMOKE_VENT_H

#include "../core/space.h"

/* The mouth of a stack or funnel in render space, how wide it is, how fast it moves along in render units a second, and the seed that times its puffs. */
struct SmokeVent {
	Vec3 at;
	double radius;
	Vec3 motion;
	uint32_t seed;
};

#endif /* MINI_WORLD_SMOKE_VENT_H */
