/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_motion.cpp Smooth vehicle positions between game ticks. */

#include "../../stdafx.h"
#include "vehicle_motion.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../../map_func.h"
#include "../../tile_map.h"
#include "../../timer/timer_game_tick.h"
#include "../../vehicle_base.h"
#include "way_course.h"

#include "../../safeguards.h"

static constexpr double MIN_TICK_MS = 5.0;
static constexpr double MAX_TICK_MS = 200.0;
static constexpr double TICK_SMOOTHING = 0.3;
static constexpr uint64_t PRUNE_INTERVAL_MASK = 0xFF;
static constexpr uint64_t STALE_TICKS = 64;
static constexpr int32_t MAX_GLIDE_DISTANCE = 2 * TILE_SIZE;
static constexpr double TURN_EASE_MS = 110.0;
static constexpr double SNAP_TURN = 0.75 * std::numbers::pi;
static constexpr double MS_PER_SECOND = 1000.0;

VehicleMotion _vehicle_motion;

/* Inside a tunnel or on a bridge the game keeps one of its ends as the vehicle's tile. */
static bool RidesDrawnGround(const Vehicle *v)
{
	return v->IsGroundVehicle() && !IsTileType(v->tile, MP_TUNNELBRIDGE);
}

/* The bearing a direction points along, from map X toward map Y. */
static double BearingOf(Direction direction)
{
	TileIndexDiffC step = TileIndexDiffCByDir(direction);
	return std::atan2(step.y, step.x);
}

/* The smaller signed angle turning from one bearing to another. */
static double TurnBetween(double from, double to)
{
	return std::remainder(to - from, 2.0 * std::numbers::pi);
}

void VehicleMotion::Advance(uint delta_ms)
{
	this->frame_ms = delta_ms;
	this->since += delta_ms;
	uint64_t now = TimerGameTick::counter;
	if (now != this->tick) {
		double per = this->since / static_cast<double>(now - this->tick);
		if (per >= MIN_TICK_MS && per <= MAX_TICK_MS) this->interval = this->interval * (1.0 - TICK_SMOOTHING) + per * TICK_SMOOTHING;
		this->tick = now;
		this->since = 0.0;
	}
	this->alpha = std::min(this->since / this->interval, 1.0);
	if ((++this->frames & PRUNE_INTERVAL_MASK) == 0) {
		std::erase_if(this->snapshots, [this](const auto &entry) { return entry.second.tick + STALE_TICKS < this->tick; });
	}
}

/* Returns the display position in tiles and height levels. Entries older
 * than one tick and jumps wider than two tiles snap instead of streaking. */
WorldPoint VehicleMotion::Position(const Vehicle *v)
{
	Snapshot &e = this->snapshots[v->index.base()];
	if (e.tick != this->tick) {
		e.previous = (e.tick + 1 == this->tick) ? e.current : PositionOf(v);
		e.current = PositionOf(v);
		e.tick = this->tick;
	}
	if (std::abs(e.current.x - e.previous.x) > MAX_GLIDE_DISTANCE || std::abs(e.current.y - e.previous.y) > MAX_GLIDE_DISTANCE) {
		e.previous = e.current;
	}
	double x = this->Interpolated(e.previous.x, e.current.x) / TILE_SIZE;
	double y = this->Interpolated(e.previous.y, e.current.y) / TILE_SIZE;
	return Grounded(v, {x, y, this->Interpolated(e.previous.z, e.current.z) / TILE_HEIGHT});
}

/* A unit eases toward the way it faces once a frame; a turn of more than three quarters of a half turn is a reversal and snaps. */
VehicleMotion::Snapshot &VehicleMotion::Turned(const Vehicle *v)
{
	Snapshot &e = this->snapshots[v->index.base()];
	double target = BearingOf(v->direction);
	if (e.turned == 0) {
		e.bearing = target;
		e.turn_rate = 0.0;
	} else if (e.turned != this->frames) {
		double turn = TurnBetween(e.bearing, target);
		double eased = std::abs(turn) > SNAP_TURN ? turn : turn * (1.0 - std::exp(-static_cast<double>(this->frame_ms) / TURN_EASE_MS));
		e.bearing += eased;
		e.turn_rate = std::abs(turn) > SNAP_TURN || this->frame_ms == 0 ? 0.0 : eased * MS_PER_SECOND / this->frame_ms;
	}
	e.turned = this->frames;
	return e;
}

double VehicleMotion::Bearing(const Vehicle *v)
{
	return this->Turned(v).bearing;
}

double VehicleMotion::TurnRate(const Vehicle *v)
{
	return this->Turned(v).turn_rate;
}

WorldPoint VehicleMotion::Grounded(const Vehicle *v, const WorldPoint &point)
{
	if (!RidesDrawnGround(v)) return point;
	return v->type == VEH_TRAIN ? TrackPoint(point.x, point.y) : RoadPoint(point.x, point.y);
}

VehicleMotion::TickPosition VehicleMotion::PositionOf(const Vehicle *v)
{
	return {v->x_pos, v->y_pos, v->z_pos};
}

double VehicleMotion::Interpolated(int32_t previous, int32_t current) const
{
	return previous + (current - previous) * this->alpha;
}

void VehicleMotion::Clear()
{
	this->snapshots.clear();
}
