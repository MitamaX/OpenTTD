/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file draw_pass.cpp The GL program that puts a draw list on the bound framebuffer. */

#include "../../stdafx.h"
#include "draw_pass.h"

#include <cstddef>

#include "../../debug.h"
#include "gl_api.h"

#include "../../safeguards.h"

static constexpr GLuint POSITION_ATTRIBUTE = 0;
static constexpr GLuint UV_ATTRIBUTE = 1;
static constexpr GLuint COLOUR_ATTRIBUTE = 2;
static constexpr uint32_t WHITE_PIXEL = 0xFFFFFFFFU;

static constexpr const char VERTEX_SHADER[] = R"(#version 330
uniform vec2 screen;
in vec2 position;
in vec2 uv;
in vec4 colour;
out vec2 frag_uv;
out vec4 frag_colour;
void main()
{
	frag_uv = uv;
	frag_colour = colour;
	vec2 ndc = position / screen * 2.0 - 1.0;
	gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
)";

static constexpr const char FRAGMENT_SHADER[] = R"(#version 330
uniform sampler2D image;
in vec2 frag_uv;
in vec4 frag_colour;
out vec4 pixel;
void main()
{
	pixel = texture(image, frag_uv) * frag_colour;
}
)";

static GLuint CompileShader(GLenum type, const char *source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	GLint compiled = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled == GL_TRUE) return shader;

	std::string log(1024, '\0');
	glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
	Debug(misc, 0, "[mini] shader rejected: {}", log.c_str());
	glDeleteShader(shader);
	return 0;
}

static GLuint LinkProgram()
{
	GLuint vertex = CompileShader(GL_VERTEX_SHADER, VERTEX_SHADER);
	GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
	if (vertex == 0 || fragment == 0) {
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		return 0;
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glBindAttribLocation(program, POSITION_ATTRIBUTE, "position");
	glBindAttribLocation(program, UV_ATTRIBUTE, "uv");
	glBindAttribLocation(program, COLOUR_ATTRIBUTE, "colour");
	glLinkProgram(program);
	glDeleteShader(vertex);
	glDeleteShader(fragment);

	GLint linked = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (linked == GL_TRUE) return program;

	Debug(misc, 0, "[mini] shader program failed to link");
	glDeleteProgram(program);
	return 0;
}

static void Attribute(GLuint index, GLint components, GLenum type, GLboolean normalised, size_t offset)
{
	glEnableVertexAttribArray(index);
	glVertexAttribPointer(index, components, type, normalised, sizeof(DrawVertex), reinterpret_cast<const void *>(offset));
}

void DrawPass::Draw(const DrawList &list, TextureStore &textures, Dimension screen)
{
	if (list.Batches().empty() || !this->Prepare()) return;

	std::span<const DrawVertex> vertices = list.Vertices();
	glUseProgram(this->program);
	glUniform2f(this->screen_uniform, static_cast<float>(screen.width), static_cast<float>(screen.height));
	glBindVertexArray(this->vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, this->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size_bytes()), vertices.data(), GL_STREAM_DRAW);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glActiveTexture(GL_TEXTURE0);

	for (const DrawBatch &batch : list.Batches()) {
		GLuint name = batch.texture == NO_TEXTURE ? this->white : textures.Name(batch.texture);
		if (name == 0) continue;
		glBindTexture(GL_TEXTURE_2D, name);
		glDrawArrays(GL_TRIANGLES, static_cast<GLint>(batch.first), static_cast<GLsizei>(batch.count));
	}

	glBindVertexArray(0);
	glUseProgram(0);
}

void DrawPass::Release()
{
	glDeleteProgram(this->program);
	glDeleteVertexArrays(1, &this->vertex_array);
	glDeleteBuffers(1, &this->vertex_buffer);
	glDeleteTextures(1, &this->white);
	*this = {};
}

/* A program the driver refused once is not compiled again every frame. */
bool DrawPass::Prepare()
{
	if (this->program != 0) return true;
	if (this->broken) return false;

	this->program = LinkProgram();
	this->broken = this->program == 0;
	if (this->broken) return false;

	this->screen_uniform = glGetUniformLocation(this->program, "screen");
	glUseProgram(this->program);
	glUniform1i(glGetUniformLocation(this->program, "image"), 0);

	glGenVertexArrays(1, &this->vertex_array);
	glGenBuffers(1, &this->vertex_buffer);
	glBindVertexArray(this->vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, this->vertex_buffer);
	Attribute(POSITION_ATTRIBUTE, 2, GL_FLOAT, GL_FALSE, offsetof(DrawVertex, x));
	Attribute(UV_ATTRIBUTE, 2, GL_FLOAT, GL_FALSE, offsetof(DrawVertex, u));
	Attribute(COLOUR_ATTRIBUTE, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(DrawVertex, rgba));
	glBindVertexArray(0);

	ResetPixelUnpack();
	glGenTextures(1, &this->white);
	glBindTexture(GL_TEXTURE_2D, this->white);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &WHITE_PIXEL);
	return true;
}
