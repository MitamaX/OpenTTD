/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file pointer_router.cpp Which layer a mouse event belongs to. */

#include "../../stdafx.h"
#include "pointer_router.h"

#include "../../gfx_func.h"

#include "../../safeguards.h"

PointerRouter _pointer;

static bool AnyButtonDown()
{
	return _left_button_down || _right_button_down || _middle_button_down;
}

/* The release goes to the owner too, and only then is the mouse free again. */
PointerLayer PointerRouter::Route(PointerLayer under)
{
	bool down = AnyButtonDown();
	if (!this->owner.has_value() && down) this->owner = under;
	this->current = this->owner.value_or(under);
	if (!down) this->owner.reset();
	return this->current;
}

/* The map follows the pointer only while nothing above it has the pointer. */
bool PointerRouter::OnMap() const
{
	return _cursor.in_window && this->current == PointerLayer::Map;
}
