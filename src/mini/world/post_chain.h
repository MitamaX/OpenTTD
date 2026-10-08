/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file post_chain.h The world's finishing: occlusion, sky and haze, antialiasing, bloom, depth of field and the tone curve that brings it to the screen. */

#ifndef MINI_WORLD_POST_CHAIN_H
#define MINI_WORLD_POST_CHAIN_H

#include <array>
#include <vector>

#include "../ui/shader_painter.h"
#include "post_target.h"
#include "scene_view.h"
#include "shader_program.h"
#include "world_target.h"

/** How the world's edges are smoothed. */
enum class Antialiasing : uint8_t {
	Off,
	Edges,
	Temporal,
};

class PostChain {
public:
	PostChain();

	void Reload();
	bool Ready();
	void Release();

	/* The view the world is drawn through this frame, nudged by a fraction of a pixel when frames are blended. */
	SceneView Jitter(const SceneView &view);
	void Finish(const WorldTarget &target, const SceneView &view);
	void Present(const ShaderArea &area, Dimension layer, const WorldTarget &target);

private:
	bool Fit(Dimension size);
	void Occlude(const WorldTarget &target);
	void Shade(const WorldTarget &target);
	const PostTarget &Resolve(const WorldTarget &target);
	void Bloom(const PostTarget &source);
	void DrawQuad() const;

	std::vector<ShaderProgram *> Programs();

	ShaderProgram occlusion;
	ShaderProgram shade;
	ShaderProgram temporal;
	ShaderProgram edges;
	ShaderProgram bloom_down;
	ShaderProgram bloom_up;
	ShaderProgram composite;

	PostTarget ambient;
	PostTarget lit;
	std::array<PostTarget, 2> history;
	std::vector<PostTarget> bloom;
	const PostTarget *resolved = nullptr;

	Mat4 previous_view_projection{};
	bool history_valid = false;
	uint frame = 0;
	uint current = 0;
	uint32_t quad = 0;
};

#endif /* MINI_WORLD_POST_CHAIN_H */
