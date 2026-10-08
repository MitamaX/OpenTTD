/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tick_clock.h The game's ticks laid on the time they ran at, so frames show the game at a steady pace between them. */

#ifndef MINI_CORE_TICK_CLOCK_H
#define MINI_CORE_TICK_CLOCK_H

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>

/* Each frame shows the game as it stood a little over a tick before the frame began, a point of the tick count moving on
 * with the time between frames at the pace ticks run and drawn gently toward when the ticks drawing saw really ran. */
class TickClock {
public:
	using Clock = std::chrono::steady_clock;

	void Stamp(uint64_t tick, Clock::time_point at, Clock::duration interval);
	void BeginFrame(Clock::time_point at);
	double Advance(uint64_t tick);

private:
	using Milliseconds = std::chrono::duration<double, std::milli>;

	struct TickStamp {
		uint64_t tick;
		Clock::time_point at;
	};

	double IntervalMs() const;

	std::deque<TickStamp> stamps;
	Clock::duration interval{};
	Clock::time_point frame_at{};
	Clock::time_point advanced_at{};
	double shown = 0.0;
	bool synced = false;
};

/* Where a unit stood at the last few ticks frames saw it at, to place it anywhere on the tick count between them. */
class TickTrail {
public:
	struct Spot {
		uint64_t tick;
		int32_t x;
		int32_t y;
		int32_t z;
	};

	struct Point {
		double x;
		double y;
		double z;
	};

	void Note(const Spot &spot, bool continues);
	bool Empty() const { return this->count == 0; }
	const Spot &Latest() const { return this->spots[this->count - 1]; }
	Point At(double tick) const;

private:
	static constexpr size_t LENGTH = 8;

	std::array<Spot, LENGTH> spots;
	size_t count = 0;
};

extern TickClock _tick_clock;

#endif /* MINI_CORE_TICK_CLOCK_H */
