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

#include "../../tile_map.h"
#include "../../timer/timer_game_tick.h"
#include "../../vehicle_base.h"

#include "../../safeguards.h"

static constexpr double MIN_TICK_MS = 5.0;
static constexpr double MAX_TICK_MS = 200.0;
static constexpr double TICK_SMOOTHING = 0.3;
static constexpr uint64_t PRUNE_INTERVAL_MASK = 0xFF;
static constexpr uint64_t STALE_TICKS = 64;
static constexpr int32_t MAX_GLIDE_DISTANCE = 2 * TILE_SIZE;

VehicleMotion _vehicle_motion;

/* Inside a tunnel or on a bridge the game keeps one of its ends as the vehicle's tile. */
static bool RidesDrawnGround(const Vehicle *v)
{
	return v->IsGroundVehicle() && !IsTileType(v->tile, MP_TUNNELBRIDGE);
}

void VehicleMotion::Advance(uint delta_ms)
{
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

WorldPoint VehicleMotion::Grounded(const Vehicle *v, const WorldPoint &point)
{
	if (!RidesDrawnGround(v)) return point;
	return {point.x, point.y, GroundLevel(point.x, point.y)};
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
