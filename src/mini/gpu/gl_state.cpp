/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file gl_state.cpp The GL state a painter borrows from RmlUi, taken when it starts and handed back when it is done. */

#include "../../stdafx.h"
#include "gl_state.h"

#include "gl_api.h"

#include "../../safeguards.h"

static int Integer(GLenum name)
{
	GLint value = 0;
	glGetIntegerv(name, &value);
	return value;
}

static void Switch(GLenum capability, bool on)
{
	if (on) {
		glEnable(capability);
	} else {
		glDisable(capability);
	}
}

GlStateScope::GlStateScope()
{
	this->switches = {glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE, glIsEnabled(GL_STENCIL_TEST) == GL_TRUE, glIsEnabled(GL_DEPTH_TEST) == GL_TRUE, glIsEnabled(GL_BLEND) == GL_TRUE, glIsEnabled(GL_CULL_FACE) == GL_TRUE};
	this->blending = {Integer(GL_BLEND_SRC_RGB), Integer(GL_BLEND_DST_RGB), Integer(GL_BLEND_SRC_ALPHA), Integer(GL_BLEND_DST_ALPHA), Integer(GL_BLEND_EQUATION_RGB), Integer(GL_BLEND_EQUATION_ALPHA)};
	this->draw_framebuffer = Integer(GL_DRAW_FRAMEBUFFER_BINDING);
	this->read_framebuffer = Integer(GL_READ_FRAMEBUFFER_BINDING);
	glGetIntegerv(GL_VIEWPORT, this->viewport.data());
	glGetIntegerv(GL_SCISSOR_BOX, this->scissor_box.data());
	glGetBooleanv(GL_COLOR_WRITEMASK, this->colour_mask.data());
	glGetFloatv(GL_COLOR_CLEAR_VALUE, this->clear_colour.data());
	glGetDoublev(GL_DEPTH_CLEAR_VALUE, &this->clear_depth);
	glGetBooleanv(GL_DEPTH_WRITEMASK, &this->depth_mask);
	this->depth_func = Integer(GL_DEPTH_FUNC);
	this->program = Integer(GL_CURRENT_PROGRAM);
	this->vertex_array = Integer(GL_VERTEX_ARRAY_BINDING);
	this->array_buffer = Integer(GL_ARRAY_BUFFER_BINDING);
	this->uniform_buffer = Integer(GL_UNIFORM_BUFFER_BINDING);
	this->active_texture = Integer(GL_ACTIVE_TEXTURE);
	for (int unit = 0; unit < TEXTURE_UNITS; unit++) {
		glActiveTexture(GL_TEXTURE0 + unit);
		this->textures[unit] = Integer(GL_TEXTURE_BINDING_2D);
	}
	glActiveTexture(this->active_texture);
}

GlStateScope::~GlStateScope()
{
	this->Restore();
}

void GlStateScope::Restore() const
{
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->draw_framebuffer);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, this->read_framebuffer);
	glViewport(this->viewport[0], this->viewport[1], this->viewport[2], this->viewport[3]);
	glScissor(this->scissor_box[0], this->scissor_box[1], this->scissor_box[2], this->scissor_box[3]);
	Switch(GL_SCISSOR_TEST, this->switches.scissor);
	Switch(GL_STENCIL_TEST, this->switches.stencil);
	Switch(GL_DEPTH_TEST, this->switches.depth);
	Switch(GL_BLEND, this->switches.blend);
	Switch(GL_CULL_FACE, this->switches.cull);
	glBlendFuncSeparate(this->blending.source_rgb, this->blending.destination_rgb, this->blending.source_alpha, this->blending.destination_alpha);
	glBlendEquationSeparate(this->blending.equation_rgb, this->blending.equation_alpha);
	glColorMask(this->colour_mask[0], this->colour_mask[1], this->colour_mask[2], this->colour_mask[3]);
	glClearColor(this->clear_colour[0], this->clear_colour[1], this->clear_colour[2], this->clear_colour[3]);
	glClearDepth(this->clear_depth);
	glDepthMask(this->depth_mask);
	glDepthFunc(this->depth_func);
	glUseProgram(this->program);
	glBindVertexArray(this->vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, this->array_buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, this->uniform_buffer);
	for (int unit = 0; unit < TEXTURE_UNITS; unit++) {
		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_2D, this->textures[unit]);
	}
	glActiveTexture(this->active_texture);
}
