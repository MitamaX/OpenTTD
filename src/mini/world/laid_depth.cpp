/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file laid_depth.cpp Shading over the depth a pass laid before any pass drew, so each pixel left in sight is shaded once. */

#include "../../stdafx.h"
#include "laid_depth.h"

#include "../gpu/gl_api.h"

#include "../../safeguards.h"

LaidDepthScope::LaidDepthScope()
{
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);
}

LaidDepthScope::~LaidDepthScope()
{
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
}
