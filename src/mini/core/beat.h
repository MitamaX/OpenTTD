/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file beat.h A slow, steady pace for work that need not follow every frame. */

#ifndef MINI_CORE_BEAT_H
#define MINI_CORE_BEAT_H

#include <chrono>

class Beat {
public:
	explicit constexpr Beat(std::chrono::milliseconds interval) : interval(interval) {}

	bool Due()
	{
		Clock::time_point now = Clock::now();
		if (now - this->struck_at < this->interval) return false;
		this->struck_at = now;
		return true;
	}

private:
	using Clock = std::chrono::steady_clock;

	std::chrono::milliseconds interval;
	Clock::time_point struck_at{};
};

#endif /* MINI_CORE_BEAT_H */
