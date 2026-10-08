/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file shadow_map.h The sun's shadows over the world: a depth map per cascade of distance from the camera, drawn by every pass that casts. */

#ifndef MINI_WORLD_SHADOW_MAP_H
#define MINI_WORLD_SHADOW_MAP_H

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include "shader_program.h"
#include "world_pass.h"

/* A depth only program for a pass's meshes, built from the same vertex sources the pass draws them with; fragment sources may cut holes into what casts. */
ShaderProgram CasterProgram(std::span<const char *const> vertex_sources);
ShaderProgram CasterProgram(std::span<const char *const> vertex_sources, std::span<const char *const> fragment_sources);
/* A depth only program for a pass's meshes as the eye sees them. */
ShaderProgram DepthProgram(std::span<const char *const> vertex_sources);
/* How far across the map a shadow lands from beneath what casts it, for each unit of height the caster stands above where it lands. */
MapVector ShadowFall();

class ShadowMap {
public:
	static constexpr int CASCADES = 4;
	static constexpr int RESOLUTION = 2048;

	void Render(const SceneView &camera, std::span<const std::unique_ptr<WorldPass>> passes);
	void Warm(std::span<const std::unique_ptr<WorldPass>> passes) const;
	void Bind() const;
	void Release();

private:
	/* One cascade: how the sun sees it, how far from the camera it reaches and how wide one of its texels is. */
	struct Cascade {
		Mat4 view_projection;
		double far;
		double texel;
	};

	bool Build();
	std::array<Cascade, CASCADES> Fit(const SceneView &camera) const;
	Cascade FitSlice(const SceneView &camera, double near, double far) const;
	void Upload(const std::array<Cascade, CASCADES> &cascades, const SceneView &camera);
	void Aim(int cascade) const;
	void BeginCasting() const;

	uint32_t texture = 0;
	uint32_t framebuffer = 0;
	uint32_t buffer = 0;
	size_t stride = 0;
	std::vector<std::byte> staged;
};

#endif /* MINI_WORLD_SHADOW_MAP_H */
