/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file gl_api.h The OpenGL entry points the mini UI draws with, loaded into the game's context. */

#ifndef MINI_GPU_GL_API_H
#define MINI_GPU_GL_API_H

#include <RmlUi_Include_GL3.h>

bool LoadGl();
bool GlLoaded();
void UnloadGl();
void ResetPixelUnpack();
/* The bound mipmapped texture keeps its detail seen at a glancing angle, where the driver filters anisotropically. */
void FilterAnisotropically();

#endif /* MINI_GPU_GL_API_H */
