/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file screen_target.cpp The framebuffer the game paints its own screen into while the mini UI is up. */

#include "../../stdafx.h"
#include "screen_target.h"

#include "../../debug.h"
#include "gl_api.h"

#include "../../safeguards.h"

/* The game's shaders leave alpha undefined, so it is cleared opaque and masked off while the game paints. */
bool ScreenTarget::Bind(Dimension size)
{
	if ((this->framebuffer == 0 || this->size != size) && !this->Build(size)) return false;

	glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
	return true;
}

void ScreenTarget::Unbind() const
{
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/* The framebuffer and the window both run their rows bottom up, so a rectangle copies straight across; what lies off the screen is left out. */
void ScreenTarget::Blit(const Rect &rect) const
{
	int height = static_cast<int>(this->size.height);
	int left = std::max(rect.left, 0);
	int right = std::min(rect.right + 1, static_cast<int>(this->size.width));
	int bottom = std::max(height - rect.bottom - 1, 0);
	int top = std::min(height - rect.top, height);
	if (left >= right || bottom >= top) return;

	glBindFramebuffer(GL_READ_FRAMEBUFFER, this->framebuffer);
	glBlitFramebuffer(left, bottom, right, top, left, bottom, right, top, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

void ScreenTarget::Release()
{
	glDeleteFramebuffers(1, &this->framebuffer);
	_textures.Remove(this->texture);
	*this = {};
}

bool ScreenTarget::Build(Dimension size)
{
	this->Release();
	this->size = size;
	this->texture = _textures.AddTarget(size);

	glGenFramebuffers(1, &this->framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _textures.Name(this->texture), 0);
	bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (complete) return true;

	Debug(misc, 0, "[mini] screen framebuffer incomplete at {}x{}", size.width, size.height);
	this->Release();
	return false;
}
