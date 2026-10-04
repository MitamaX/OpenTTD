/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file press_owner.cpp Which layer a mouse press belongs to. */

#include "../../stdafx.h"
#include "press_owner.h"

#include "../../gfx_func.h"

#include "../../safeguards.h"

static bool AnyButtonDown()
{
	return _left_button_down || _right_button_down || _middle_button_down;
}

PressSide PressOwner::Held()
{
	if (!AnyButtonDown()) this->side = PressSide::None;
	return this->side;
}

void PressOwner::Claim(PressSide side)
{
	if (AnyButtonDown() && this->side == PressSide::None) this->side = side;
}
