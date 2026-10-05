/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file gpu_frame.cpp How a mini UI frame reaches the screen through the game's OpenGL back-end. */

#include "../../stdafx.h"
#include "gpu_frame.h"

#include <utility>

#include "gl_api.h"

#include "../../safeguards.h"

GpuFrame _gpu;

/* Every paint of an OpenGL back-end passes here; a captured one lands in the screen texture instead of the window. */
bool GpuFrame::BeginPaint(Dimension screen, bool capture)
{
	this->available = true;
	return capture && LoadGl() && this->target.Bind(screen);
}

/* Bottom to top: the RmlUi layer with the map at its foot, then the native windows that float free of any panel. */
void GpuFrame::Compose(std::span<const Rect> floating)
{
	Dimension size = this->target.Size();
	this->target.Unbind();
	ResetPixelUnpack();
	glViewport(0, 0, static_cast<GLsizei>(size.width), static_cast<GLsizei>(size.height));
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	if (this->layer != nullptr) this->layer->Render(size);
	for (const Rect &rect : floating) this->target.Blit(rect);
}

/* The back-end is going away with its context; everything made in it goes first. */
void GpuFrame::Release()
{
	this->available = false;
	if (!GlLoaded()) return;

	if (this->layer != nullptr) std::exchange(this->layer, nullptr)->Detach();
	this->target.Release();
	_textures.Release();
	UnloadGl();
}

GlImage GpuFrame::Screen() const
{
	return _textures.Image(this->target.Texture());
}
