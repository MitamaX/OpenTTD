/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file post_target.cpp A colour texture with a framebuffer of its own, which one step of the world's finishing draws into and the next reads. */

#include "../../stdafx.h"
#include "post_target.h"

#include "../../debug.h"
#include "../gpu/gl_api.h"

#include "../../safeguards.h"

bool PostTarget::Fit(Dimension size)
{
	if (this->framebuffer != 0 && this->size == size) return true;

	this->Release();
	ResetPixelUnpack();
	this->texture = TargetTexture(this->format, GL_RGBA, GL_FLOAT, size, this->filter);
	glGenFramebuffers(1, &this->framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->texture, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
		this->size = size;
		return true;
	}

	Debug(misc, 0, "[mini] finishing framebuffer incomplete at {}x{}", size.width, size.height);
	this->Release();
	return false;
}

void PostTarget::Bind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer);
	glViewport(0, 0, static_cast<GLsizei>(this->size.width), static_cast<GLsizei>(this->size.height));
}

void PostTarget::BindTexture(uint unit) const
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, this->texture);
}

void PostTarget::Release()
{
	if (this->framebuffer != 0) glDeleteFramebuffers(1, &this->framebuffer);
	if (this->texture != 0) glDeleteTextures(1, &this->texture);
	this->framebuffer = 0;
	this->texture = 0;
	this->size = {};
}
