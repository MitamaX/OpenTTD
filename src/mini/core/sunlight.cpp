/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sunlight.cpp The sun over the mini map, kept at the screen's upper left whichever way the view faces. */

#include "../../stdafx.h"
#include "sunlight.h"

#include "camera.h"
#include "tuning.h"

#include "../../safeguards.h"

SunVector Sun()
{
	MapVector right = _camera.Right();
	MapVector toward = _camera.Toward();
	return {right.x * SUN_ACROSS + toward.x * SUN_ALONG, right.y * SUN_ACROSS + toward.y * SUN_ALONG, SUN_UP};
}

double ReliefShare()
{
	return _tuning.relief_strength / RELIEF_FULL;
}
