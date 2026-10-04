/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_motion.cpp Smooth vehicle positions between game ticks. */

#include "../../stdafx.h"
#include "vehicle_motion.h"

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

/* Returns the display position in tile units. Entries older than one tick
 * and jumps wider than two tiles snap instead of streaking. */
TilePoint VehicleMotion::Position(const Vehicle *v)
{
	Snapshot &e = this->snapshots[v->index.base()];
	if (e.tick != this->tick) {
		if (e.tick + 1 == this->tick) {
			e.px = e.cx;
			e.py = e.cy;
		} else {
			e.px = v->x_pos;
			e.py = v->y_pos;
		}
		e.cx = v->x_pos;
		e.cy = v->y_pos;
		e.tick = this->tick;
	}
	if (std::abs(e.cx - e.px) > MAX_GLIDE_DISTANCE || std::abs(e.cy - e.py) > MAX_GLIDE_DISTANCE) {
		e.px = e.cx;
		e.py = e.cy;
	}
	return {(e.px + (e.cx - e.px) * this->alpha) / TILE_SIZE, (e.py + (e.cy - e.py) * this->alpha) / TILE_SIZE};
}

void VehicleMotion::Clear()
{
	this->snapshots.clear();
}
