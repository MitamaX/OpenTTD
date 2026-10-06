/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file gl_program.cpp A vertex and fragment shader linked into one GL program. */

#include "../../stdafx.h"
#include "gl_program.h"

#include <string>

#include "../../debug.h"
#include "gl_api.h"

#include "../../safeguards.h"

static std::string InfoLog(GLuint object, PFNGLGETSHADERIVPROC query, PFNGLGETSHADERINFOLOGPROC read)
{
	GLint length = 0;
	query(object, GL_INFO_LOG_LENGTH, &length);
	std::string log(static_cast<size_t>(std::max(length, 1)), '\0');
	GLsizei written = 0;
	read(object, static_cast<GLsizei>(log.size()), &written, log.data());
	log.resize(static_cast<size_t>(written));
	return log;
}

static GLuint Compile(GLenum stage, std::string_view source)
{
	GLuint shader = glCreateShader(stage);
	const GLchar *text = source.data();
	GLint length = static_cast<GLint>(source.size());
	glShaderSource(shader, 1, &text, &length);
	glCompileShader(shader);

	GLint compiled = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled == GL_TRUE) return shader;

	Debug(misc, 0, "[mini] shader does not compile: {}", InfoLog(shader, glGetShaderiv, glGetShaderInfoLog));
	glDeleteShader(shader);
	return 0;
}

static GLuint Link(GLuint vertex, GLuint fragment)
{
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);
	glDetachShader(program, vertex);
	glDetachShader(program, fragment);

	GLint linked = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (linked == GL_TRUE) return program;

	Debug(misc, 0, "[mini] shader program does not link: {}", InfoLog(program, glGetProgramiv, glGetProgramInfoLog));
	glDeleteProgram(program);
	return 0;
}

bool GlProgram::Build(std::string_view vertex_source, std::string_view fragment_source)
{
	this->Release();
	GLuint vertex = Compile(GL_VERTEX_SHADER, vertex_source);
	GLuint fragment = Compile(GL_FRAGMENT_SHADER, fragment_source);
	if (vertex != 0 && fragment != 0) this->name = Link(vertex, fragment);
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	return this->name != 0;
}

void GlProgram::Release()
{
	if (this->name != 0) glDeleteProgram(this->name);
	this->name = 0;
}

void GlProgram::Use() const
{
	glUseProgram(this->name);
}

int GlProgram::Uniform(const char *uniform) const
{
	return glGetUniformLocation(this->name, uniform);
}
