/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_motion.h Smooth vehicle positions between game ticks. */

#ifndef MINI_MAP_VEHICLE_MOTION_H
#define MINI_MAP_VEHICLE_MOTION_H

#include <unordered_map>

#include "../core/camera.h"
#include "../core/tick_clock.h"

struct Vehicle;

/* Vehicles only move on game ticks while drawing runs at render rate, so
 * raw positions stutter. Each frame places a unit along the positions it was
 * seen at on the ticks before, at the point of the tick count the tick clock shows. */
class VehicleMotion {
public:
	void Advance(double delta_ms);
	WorldPoint Position(const Vehicle *v);
	/* The way a unit faces in radians from map X toward map Y, eased through its turns, and how fast it is turning. */
	double Bearing(const Vehicle *v);
	double TurnRate(const Vehicle *v);
	static WorldPoint Grounded(const Vehicle *v, const WorldPoint &point);
	void Clear();

private:
	struct Snapshot {
		TickTrail trail;
		uint64_t seen;
		double bearing;
		double turn_rate;
		uint64_t turned;
	};

	static TickTrail::Spot SpotOf(const Vehicle *v, uint64_t tick);
	static bool Glides(const TickTrail::Spot &from, const TickTrail::Spot &to);
	Snapshot &Turned(const Vehicle *v);

	std::unordered_map<uint32_t, Snapshot> snapshots;
	uint64_t tick = 0;
	uint64_t frames = 0;
	double frame_ms = 0.0;
};

extern VehicleMotion _vehicle_motion;

#endif /* MINI_MAP_VEHICLE_MOTION_H */
