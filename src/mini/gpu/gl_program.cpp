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
	if (this->name != 0) this->LearnUniforms();
	return this->name != 0;
}

/* An array is named by its first element, which also answers to the array's bare name. */
void GlProgram::LearnUniforms()
{
	static constexpr std::string_view FIRST_ELEMENT = "[0]";
	GLint count = 0;
	GLint longest = 0;
	glGetProgramiv(this->name, GL_ACTIVE_UNIFORMS, &count);
	glGetProgramiv(this->name, GL_ACTIVE_UNIFORM_MAX_LENGTH, &longest);
	std::string buffer(static_cast<size_t>(std::max(longest, 1)), '\0');
	for (GLint index = 0; index < count; index++) {
		GLsizei length = 0;
		GLint size = 0;
		GLenum type = 0;
		glGetActiveUniform(this->name, static_cast<GLuint>(index), static_cast<GLsizei>(buffer.size()), &length, &size, &type, buffer.data());
		std::string uniform(buffer.data(), static_cast<size_t>(length));
		int location = glGetUniformLocation(this->name, uniform.c_str());
		if (uniform.ends_with(FIRST_ELEMENT)) uniform.resize(uniform.size() - FIRST_ELEMENT.size());
		this->uniforms.emplace(std::move(uniform), location);
	}
}

void GlProgram::Release()
{
	if (this->name != 0) glDeleteProgram(this->name);
	this->name = 0;
	this->uniforms.clear();
}

void GlProgram::Use() const
{
	glUseProgram(this->name);
}

/* A uniform the program does not use has no location, which GL quietly ignores. */
int GlProgram::Uniform(const char *uniform) const
{
	auto found = this->uniforms.find(std::string_view(uniform));
	return found == this->uniforms.end() ? -1 : found->second;
}

/* A program that declares no such block keeps what it has. */
void GlProgram::BindBlock(const char *block, uint32_t binding) const
{
	GLuint index = glGetUniformBlockIndex(this->name, block);
	if (index != GL_INVALID_INDEX) glUniformBlockBinding(this->name, index, binding);
}
