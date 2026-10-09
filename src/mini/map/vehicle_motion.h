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
#include "train_route.h"

struct Vehicle;

/* Vehicles only move on game ticks while drawing runs at render rate, so
 * raw positions stutter. Each frame places a unit along the positions it was
 * seen at on the ticks before, at the point of the tick count the tick clock shows. */
class VehicleMotion {
public:
	void Advance(double delta_ms);
	/* Where a unit stands on the game's own map, in tiles and height levels, and where it is drawn. */
	WorldPoint Gliding(const Vehicle *v);
	WorldPoint Position(const Vehicle *v);
	/* Where a unit's path over the game's map runs to a reach ahead of a point, or behind it for a negative reach:
	 * along its track for a train on the ground, else straight on the way it faces. */
	WorldPoint Onward(const Vehicle *v, const WorldPoint &from, double reach);
	/* The way a unit faces in radians from map X toward map Y, eased through its turns, and how fast it is turning. */
	double Bearing(const Vehicle *v);
	double TurnRate(const Vehicle *v);
	/* Whether a unit runs along the track as it is drawn: a train out of tunnels and off bridges. */
	static bool FollowsTrack(const Vehicle *v);
	static WorldPoint Grounded(const Vehicle *v, const WorldPoint &point);
	/* The pieces of a tile's track a train's units stand on or stood on last before, and any the game has reserved there for a train's way ahead. */
	TrackTaken TakenBy(const Vehicle *v) const;
	void Clear();

private:
	/* A tile a train's unit stood on and the pieces it took there. */
	struct Passage {
		TileIndex tile = INVALID_TILE;
		TrackBits track = TRACK_BIT_NONE;
	};

	struct Snapshot {
		TickTrail trail;
		uint64_t seen;
		bool turned_about;
		double bearing;
		double turn_rate;
		uint64_t turned;
		Passage on;
		Passage left;

		void NotePassage(const Passage &now, bool continues);
	};

	static TickTrail::Spot SpotOf(const Vehicle *v, uint64_t tick);
	static bool Glides(const TickTrail::Spot &from, const TickTrail::Spot &to);
	bool SeenLately(const Snapshot &e) const;
	Passage LeftBy(const Vehicle *unit) const;
	Snapshot &Turned(const Vehicle *v);

	std::unordered_map<uint32_t, Snapshot> snapshots;
	uint64_t tick = 0;
	uint64_t frames = 0;
	double frame_ms = 0.0;
};

extern VehicleMotion _vehicle_motion;

#endif /* MINI_MAP_VEHICLE_MOTION_H */
