/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file scene_view.h How the camera sees the 3D world this frame, and the uniform block every world shader reads it from. */

#ifndef MINI_WORLD_SCENE_VIEW_H
#define MINI_WORLD_SCENE_VIEW_H

#include "../../core/geometry_type.hpp"
#include "../core/camera.h"
#include "../core/space.h"

inline constexpr uint32_t SCENE_BINDING = 0;

/* Render space runs along map X and Y in tiles, with heights raised by the level rise. */
struct SceneView {
	Mat4 view;
	Mat4 projection;
	Mat4 view_projection;
	Frustum frustum;
	Vec3 eye;
	double focal;
	double near;
	double far;
	double fog_start;
	Dimension viewport;

	static SceneView Of(const Camera &camera);

	bool Sees(const Vec3 &low, const Vec3 &high) const;
	double TilePixelsAt(double distance) const;
};

class SceneUniforms {
public:
	void Upload(const SceneView &view);
	void Release();

private:
	uint32_t buffer = 0;
};

#endif /* MINI_WORLD_SCENE_VIEW_H */
