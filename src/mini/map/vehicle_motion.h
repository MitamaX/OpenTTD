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

struct Vehicle;

/* Vehicles only move on game ticks while drawing runs at render rate, so
 * raw positions stutter. Each frame interpolates between a unit's previous
 * and current tick position; the fraction comes from a smoothed measure of
 * the real tick interval, which also absorbs fast forward. */
class VehicleMotion {
public:
	void Advance(uint delta_ms);
	WorldPoint Position(const Vehicle *v);
	static WorldPoint Grounded(const Vehicle *v, const WorldPoint &point);
	void Clear();

private:
	struct TickPosition {
		int32_t x;
		int32_t y;
		int32_t z;
	};

	struct Snapshot {
		TickPosition previous;
		TickPosition current;
		uint64_t tick;
	};

	static TickPosition PositionOf(const Vehicle *v);
	double Interpolated(int32_t previous, int32_t current) const;

	std::unordered_map<uint32_t, Snapshot> snapshots;
	uint64_t tick = 0;
	uint64_t frames = 0;
	double since = 0.0;
	double interval = 30.0;
	double alpha = 1.0;
};

extern VehicleMotion _vehicle_motion;

#endif /* MINI_MAP_VEHICLE_MOTION_H */
