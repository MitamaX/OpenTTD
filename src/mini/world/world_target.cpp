/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file world_target.cpp The framebuffer the 3D world is drawn into: high range colour and a depth texture later passes can read. */

#include "../../stdafx.h"
#include "world_target.h"

#include "../../debug.h"
#include "../gpu/gl_api.h"

#include "../../safeguards.h"

static uint32_t TargetTexture(GLint internal_format, GLenum format, GLenum type, Dimension size)
{
	GLuint texture = 0;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, static_cast<GLsizei>(size.width), static_cast<GLsizei>(size.height), 0, format, type, nullptr);
	return texture;
}

bool WorldTarget::Bind(Dimension size)
{
	if ((this->framebuffer == 0 || this->size != size) && !this->Build(size)) return false;

	glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer);
	glViewport(0, 0, static_cast<GLsizei>(size.width), static_cast<GLsizei>(size.height));
	return true;
}

void WorldTarget::Release()
{
	if (this->framebuffer != 0) glDeleteFramebuffers(1, &this->framebuffer);
	if (this->colour != 0) glDeleteTextures(1, &this->colour);
	if (this->depth != 0) glDeleteTextures(1, &this->depth);
	*this = {};
}

bool WorldTarget::Build(Dimension size)
{
	this->Release();
	this->size = size;
	ResetPixelUnpack();
	this->colour = TargetTexture(GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, size);
	this->depth = TargetTexture(GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, size);

	glGenFramebuffers(1, &this->framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->colour, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, this->depth, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) return true;

	Debug(misc, 0, "[mini] world framebuffer incomplete at {}x{}", size.width, size.height);
	this->Release();
	return false;
}
