/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file command_probe.cpp Asking the game what commands would do without doing them. */

#include "../../stdafx.h"
#include "command_probe.h"

#include <utility>

#include "../../gfx_func.h"

#include "../../safeguards.h"

CommandProbe::CommandProbe() : outer(current), shift(_shift_pressed)
{
	_shift_pressed = true;
	current = this;
}

CommandProbe::~CommandProbe()
{
	current = this->outer;
	_shift_pressed = this->shift;
}

bool CommandProbe::TakeVerdict()
{
	return std::exchange(this->caught, false);
}

bool CommandProbe::Catch(Money cost)
{
	if (current == nullptr) return false;

	current->cost += cost;
	current->caught = true;
	return true;
}
