/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file gl_program.h A vertex and fragment shader linked into one GL program. */

#ifndef MINI_GPU_GL_PROGRAM_H
#define MINI_GPU_GL_PROGRAM_H

#include <map>
#include <string>
#include <string_view>

/* Every uniform's location is looked up once, as the program is linked, so drawing never waits on the driver to answer. */
class GlProgram {
public:
	bool Build(std::string_view vertex_source, std::string_view fragment_source);
	void Release();

	explicit operator bool() const { return this->name != 0; }
	void Use() const;
	int Uniform(const char *uniform) const;
	void BindBlock(const char *block, uint32_t binding) const;

private:
	void LearnUniforms();

	uint32_t name = 0;
	std::map<std::string, int, std::less<>> uniforms;
};

#endif /* MINI_GPU_GL_PROGRAM_H */
