/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file gl_api.cpp The OpenGL entry points the mini UI draws with, loaded into the game's context. */

#include "../../stdafx.h"
#include "gl_api.h"

#include <utility>

#include <RmlUi_Renderer_GL3.h>

#include "../../safeguards.h"

static bool _gl_loaded = false;

/* RmlUi's renderer carries the loader, and every mini UI GL caller shares the entry points it fills. */
bool LoadGl()
{
	if (!_gl_loaded) _gl_loaded = RmlGL3::Initialize();
	return _gl_loaded;
}

bool GlLoaded()
{
	return _gl_loaded;
}

void UnloadGl()
{
	if (std::exchange(_gl_loaded, false)) RmlGL3::Shutdown();
}

/* The game's back-end leaves its pixel buffer bound with the screen pitch as row length; client uploads must not read through them. */
void ResetPixelUnpack()
{
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}
