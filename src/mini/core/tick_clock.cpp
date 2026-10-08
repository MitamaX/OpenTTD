/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tick_clock.cpp The game's ticks laid on the time they ran at, so frames show the game at a steady pace between them. */

#include "../../stdafx.h"
#include "tick_clock.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <span>

#include "../../safeguards.h"

static constexpr std::chrono::milliseconds MEASURED_SPAN{500};
static constexpr size_t MIN_MEASURED_STAMPS = 8;
static constexpr std::chrono::milliseconds STALL{250};
static constexpr double SHOWN_DELAY_TICKS = 2.0;
static constexpr double MIN_SHOWN_DELAY_MS = 20.0;
static constexpr double SYNC_MS = 250.0;
static constexpr double RESYNC_MS = 200.0;

TickClock _tick_clock;

void TickClock::Stamp(uint64_t tick, Clock::time_point at, Clock::duration interval)
{
	if (!this->stamps.empty()) {
		const TickStamp &latest = this->stamps.back();
		if (tick == latest.tick) return;
		if (tick < latest.tick || at - latest.at > STALL) this->stamps.clear();
	}
	this->interval = interval;
	this->stamps.push_back({tick, at});
	while (at - this->stamps.front().at > MEASURED_SPAN) this->stamps.pop_front();
}

void TickClock::BeginFrame(Clock::time_point at)
{
	this->frame_ms = Milliseconds(at - this->frame_at).count();
	this->frame_at = at;
}

/* Ticks run at the pace the game asks for, or slower when the game cannot keep up with it. */
double TickClock::IntervalMs() const
{
	double nominal = Milliseconds(this->interval).count();
	if (this->stamps.size() < MIN_MEASURED_STAMPS) return nominal;
	const TickStamp &first = this->stamps.front();
	const TickStamp &last = this->stamps.back();
	return std::max(nominal, Milliseconds(last.at - first.at).count() / (last.tick - first.tick));
}

/* The shown tick never goes back nor past the game's own; a frame far from where it should be, after a pause or a load, starts over there. */
void TickClock::Advance(uint64_t tick)
{
	double interval_ms = this->IntervalMs();
	if (this->stamps.empty() || this->stamps.back().tick != tick || interval_ms <= 0.0) {
		this->synced = false;
		this->shown = static_cast<double>(tick);
		return;
	}

	const TickStamp &latest = this->stamps.back();
	double delay_ms = std::max(SHOWN_DELAY_TICKS * interval_ms, MIN_SHOWN_DELAY_MS);
	double wanted = latest.tick + (Milliseconds(this->frame_at - latest.at).count() - delay_ms) / interval_ms;
	if (!this->synced || std::abs(wanted - this->shown) * interval_ms > RESYNC_MS) {
		this->shown = wanted;
		this->synced = true;
	} else {
		double drawn_in = (wanted - this->shown) * (1.0 - std::exp(-this->frame_ms / SYNC_MS));
		this->shown = std::max(this->shown, this->shown + this->frame_ms / interval_ms + drawn_in);
	}
	this->shown = std::min(this->shown, static_cast<double>(tick));
}

void TickTrail::Note(const Spot &spot, bool continues)
{
	if (!continues) this->count = 0;
	if (this->count > 0 && this->Latest().tick == spot.tick) {
		this->spots[this->count - 1] = spot;
		return;
	}
	if (this->count == LENGTH) {
		std::shift_left(this->spots.begin(), this->spots.end(), 1);
		this->count--;
	}
	this->spots[this->count++] = spot;
}

/* Between two spots a unit moves straight from one to the other; before the first or after the last it stands there. */
TickTrail::Point TickTrail::At(double tick) const
{
	auto noted = std::span(this->spots).first(this->count);
	auto later = std::ranges::find_if(noted, [tick](const Spot &spot) { return spot.tick > tick; });
	const Spot &to = later == noted.end() ? noted.back() : *later;
	const Spot &from = later == noted.begin() ? to : *std::prev(later);
	double share = from.tick == to.tick ? 0.0 : (tick - from.tick) / (to.tick - from.tick);
	return {from.x + (to.x - from.x) * share, from.y + (to.y - from.y) * share, from.z + (to.z - from.z) * share};
}
