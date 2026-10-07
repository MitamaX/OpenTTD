/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file shader_program.h A program built from the mini UI's shader files, behind the header that names the world's codes, sizes and tones. */

#ifndef MINI_WORLD_SHADER_PROGRAM_H
#define MINI_WORLD_SHADER_PROGRAM_H

#include <span>
#include <vector>

#include "../../landscape_type.h"
#include "../gpu/gl_program.h"

/* The sources are read anew after every reload, and a program that fails to build stays missing until the next one.
 * Every program finds the scene and shadow blocks and the frame's shared samplers bound. */
class ShaderProgram {
public:
	ShaderProgram(std::span<const char *const> vertex_sources, std::span<const char *const> fragment_sources);

	void Reload() { this->attempted = false; }
	bool Ready();
	void Release();

	void Use() const { this->program.Use(); }
	int Uniform(const char *uniform) const { return this->program.Uniform(uniform); }
	void BindSampler(const char *sampler, uint unit) const;

private:
	void BindShared() const;

	std::vector<const char *> vertex_sources;
	std::vector<const char *> fragment_sources;
	GlProgram program;
	LandscapeType landscape = LandscapeType::Temperate;
	bool attempted = false;
};

/* Hands the overlay's shown layer and how far the rest sinks to a program reading overlay.glsl. */
void UploadOverlay(const ShaderProgram &program);

#endif /* MINI_WORLD_SHADER_PROGRAM_H */
