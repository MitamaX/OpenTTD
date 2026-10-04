/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file command_probe.h Asking the game what commands would do without doing them. */

#ifndef MINI_TOOLS_COMMAND_PROBE_H
#define MINI_TOOLS_COMMAND_PROBE_H

#include "../../economy_type.h"

/* While a probe is open, commands go out with the game's estimate switch held
 * down, so nothing is built and the game answers what it would charge and
 * what it would refuse. A refusal is the answer, not news for the player. */
class CommandProbe {
public:
	CommandProbe();
	~CommandProbe();
	CommandProbe(const CommandProbe &) = delete;
	CommandProbe &operator=(const CommandProbe &) = delete;

	bool TakeVerdict();
	Money Cost() const { return this->cost; }

	static bool Open() { return current != nullptr; }
	static bool Catch(Money cost);

private:
	static inline CommandProbe *current = nullptr;

	CommandProbe *outer;
	bool shift;
	bool caught = false;
	Money cost = 0;
};

/* Everything a probe depends on, folded into one number, so the probe only
 * runs again once the player has changed what it would ask about. Zero marks
 * "no probe yet", so a real key never lands on it. */
class ProbeKey {
public:
	ProbeKey &Add(uint64_t v)
	{
		this->h ^= v + GOLDEN_GAMMA + (this->h << 6) + (this->h >> 2);
		return *this;
	}

	uint64_t Value() const { return this->h | 1; }

private:
	static constexpr uint64_t GOLDEN_GAMMA = 0x9E3779B97F4A7C15ULL;

	uint64_t h = 0;
};

#endif /* MINI_TOOLS_COMMAND_PROBE_H */
