/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file scene_view.cpp How the camera sees the 3D world this frame, and the uniform block every world shader reads it from. */

#include "../../stdafx.h"
#include "scene_view.h"

#include <algorithm>
#include <array>
#include <chrono>

#include "../core/sunlight.h"
#include "../gpu/gl_api.h"
#include "../map/world_tiles.h"

#include "../../safeguards.h"

static constexpr double FOG_START_SHARE = 5.0;
static constexpr double SHADOW_REACH_SHARE = 6.0;
static constexpr double CLOCK_PERIOD_SECONDS = 3600.0;

/* The layout of the Scene block in scene.glsl, std140. */
struct SceneBlock {
	std::array<float, 16> view;
	std::array<float, 16> projection;
	std::array<float, 16> view_projection;
	std::array<float, 4> eye;
	std::array<float, 4> sun;
	std::array<float, 4> lens;
	std::array<float, 4> world;
	std::array<float, 4> screen;
};

static double Clock()
{
	static const auto start = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
	return std::fmod(elapsed.count(), CLOCK_PERIOD_SECONDS);
}

SceneView SceneView::Of(const Camera &camera)
{
	SceneView view;
	view.view = camera.ViewMatrix();
	view.projection = camera.ProjectionMatrix();
	view.view_projection = view.projection * view.view;
	view.frustum = FrustumOf(view.view_projection);
	view.eye = camera.Eye();
	view.focal = camera.Focal();
	view.focus_pixels = camera.Ppt();
	view.near = camera.Near();
	view.far = camera.Far();
	view.fog_start = std::min(camera.FocusDistance() * FOG_START_SHARE, view.far);
	view.shadow_reach = std::min(camera.FocusDistance() * SHADOW_REACH_SHARE, view.far);
	view.clock = Clock();
	view.viewport = {static_cast<uint>(camera.Width()), static_cast<uint>(camera.Height())};
	return view;
}

/* The picture shifts by a fraction of a pixel while culling keeps the camera's own frustum; the phase walks screen noise on with it. */
SceneView SceneView::Jittered(double x_pixels, double y_pixels, uint phase) const
{
	SceneView jittered = *this;
	jittered.projection.At(0, 2) -= 2.0 * x_pixels / this->viewport.width;
	jittered.projection.At(1, 2) -= 2.0 * y_pixels / this->viewport.height;
	jittered.view_projection = jittered.projection * jittered.view;
	jittered.phase = phase;
	return jittered;
}

bool SceneView::Sees(const Vec3 &low, const Vec3 &high) const
{
	return BoxMeets(this->frustum, low, high);
}

double SceneView::TilePixelsAt(double distance) const
{
	return this->focal / std::max(distance, this->near);
}

/* Tile pixels where a box comes nearest the eye. */
double SceneView::NearestTilePixels(const Vec3 &low, const Vec3 &high) const
{
	Vec3 nearest = {std::clamp(this->eye.x, low.x, high.x), std::clamp(this->eye.y, low.y, high.y), std::clamp(this->eye.z, low.z, high.z)};
	return this->TilePixelsAt(Length(this->eye - nearest));
}

void SceneUniforms::Upload(const SceneView &view)
{
	auto floats = [](double x, double y, double z, double w) { return std::array<float, 4>{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(w)}; };
	SunVector sun = Sun();
	Dimension map = _world_tiles.Size();
	SceneBlock block = {
		view.view.Floats(),
		view.projection.Floats(),
		view.view_projection.Floats(),
		floats(view.eye.x, view.eye.y, view.eye.z, 1.0),
		floats(sun.x, sun.y, sun.z, 0.0),
		floats(view.near, view.far, view.fog_start, view.far),
		floats(map.width, map.height, _world_tiles.Peak(), LevelRise()),
		floats(view.viewport.width, view.viewport.height, view.clock, view.phase),
	};

	if (this->buffer == 0) glGenBuffers(1, &this->buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, this->buffer);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(block), &block, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_BINDING, this->buffer);
}

void SceneUniforms::Release()
{
	if (this->buffer != 0) glDeleteBuffers(1, &this->buffer);
	this->buffer = 0;
}
