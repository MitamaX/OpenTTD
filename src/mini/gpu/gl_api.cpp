/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file gl_api.cpp The OpenGL entry points the mini UI draws with, loaded into the game's context. */

#include "../../stdafx.h"
#include "gl_api.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

#include <RmlUi_Renderer_GL3.h>

#include "../../safeguards.h"

static bool _gl_loaded = false;

/* The anisotropic filtering extensions name these, the same values in either. */
static constexpr GLenum TEXTURE_MAX_ANISOTROPY = 0x84FE;
static constexpr GLenum MAX_TEXTURE_MAX_ANISOTROPY = 0x84FF;
static constexpr std::array<std::string_view, 2> ANISOTROPY_EXTENSIONS = {"GL_ARB_texture_filter_anisotropic", "GL_EXT_texture_filter_anisotropic"};
static constexpr GLfloat WANTED_ANISOTROPY = 8.0f;

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

static GLfloat MostAnisotropy()
{
	GLint count = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &count);
	for (GLint index = 0; index < count; index++) {
		std::string_view name = reinterpret_cast<const char *>(glGetStringi(GL_EXTENSIONS, index));
		if (std::ranges::find(ANISOTROPY_EXTENSIONS, name) == ANISOTROPY_EXTENSIONS.end()) continue;
		GLfloat most = 1.0f;
		glGetFloatv(MAX_TEXTURE_MAX_ANISOTROPY, &most);
		return most;
	}
	return 1.0f;
}

void FilterAnisotropically()
{
	static const GLfloat most = MostAnisotropy();
	if (most > 1.0f) glTexParameterf(GL_TEXTURE_2D, TEXTURE_MAX_ANISOTROPY, std::min(most, WANTED_ANISOTROPY));
}
