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

static constexpr uint64_t PRUNE_INTERVAL_MASK = 0xFF;
static constexpr uint64_t STALE_FRAMES = PRUNE_INTERVAL_MASK + 1;
static constexpr int64_t MAX_GLIDE_DISTANCE = 2 * TILE_SIZE;
static constexpr int64_t MAX_STRIDE_PER_TICK = TILE_SIZE;
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

void VehicleMotion::Advance(double delta_ms)
{
	this->frame_ms = delta_ms;
	this->tick = TimerGameTick::counter;
	if ((++this->frames & PRUNE_INTERVAL_MASK) == 0) {
		std::erase_if(this->snapshots, [this](const auto &entry) { return std::max(entry.second.seen, entry.second.turned) + STALE_FRAMES < this->frames; });
	}
}

/* Returns the display position in tiles and height levels. A unit not seen the frame before, new or out of sight until
 * now, or one that jumped further than it could have moved, starts its trail over where it stands instead of streaking. */
WorldPoint VehicleMotion::Position(const Vehicle *v)
{
	Snapshot &e = this->snapshots[v->index.base()];
	TickTrail::Spot spot = SpotOf(v, this->tick);
	e.trail.Note(spot, e.seen + 1 >= this->frames && !e.trail.Empty() && Glides(e.trail.Latest(), spot));
	e.seen = this->frames;
	TickTrail::Point at = e.trail.At(_tick_clock.Shown());
	return Grounded(v, {at.x / TILE_SIZE, at.y / TILE_SIZE, at.z / TILE_HEIGHT});
}

/* A unit eases toward the way it faces once a frame; a turn of more than three quarters of a half turn is a reversal and snaps,
 * and a unit not turned in the frame before, new or out of sight until now, faces its way at once. */
VehicleMotion::Snapshot &VehicleMotion::Turned(const Vehicle *v)
{
	Snapshot &e = this->snapshots[v->index.base()];
	double target = BearingOf(v->direction);
	if (e.turned == 0 || e.turned + 1 < this->frames) {
		e.bearing = target;
		e.turn_rate = 0.0;
	} else if (e.turned != this->frames) {
		double turn = TurnBetween(e.bearing, target);
		double eased = std::abs(turn) > SNAP_TURN ? turn : turn * (1.0 - std::exp(-this->frame_ms / TURN_EASE_MS));
		e.bearing += eased;
		e.turn_rate = std::abs(turn) > SNAP_TURN || this->frame_ms <= 0.0 ? 0.0 : eased * MS_PER_SECOND / this->frame_ms;
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

TickTrail::Spot VehicleMotion::SpotOf(const Vehicle *v, uint64_t tick)
{
	return {tick, v->x_pos, v->y_pos, v->z_pos};
}

bool VehicleMotion::Glides(const TickTrail::Spot &from, const TickTrail::Spot &to)
{
	if (to.tick < from.tick) return false;
	int64_t reach = std::max<int64_t>(MAX_GLIDE_DISTANCE, static_cast<int64_t>(to.tick - from.tick) * MAX_STRIDE_PER_TICK);
	return std::abs(static_cast<int64_t>(to.x) - from.x) <= reach && std::abs(static_cast<int64_t>(to.y) - from.y) <= reach;
}

void VehicleMotion::Clear()
{
	this->snapshots.clear();
}
