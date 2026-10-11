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
#include <cmath>

#include "../../landscape.h"
#include "../../settings_type.h"
#include "../core/sunlight.h"
#include "../core/tick_clock.h"
#include "../gpu/frame_capture.h"
#include "../gpu/gl_api.h"
#include "../map/world_tiles.h"
#include "seabed.h"

#include "../../safeguards.h"

static constexpr double FOG_START_SHARE = 5.0;
static constexpr double SHADOW_REACH_SHARE = 6.0;
static constexpr double CLOCK_PERIOD_SECONDS = 3600.0;
static constexpr double NO_SNOW_LINE = 1e6;

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
	std::array<float, 4> climate;
};

/* Seconds since the first frame by when each frame began, so everything moved by it moves alike however long the frame took to set up. */
static double SecondsSinceFirstFrame()
{
	static const TickClock::Clock::time_point start = _tick_clock.FrameAt();
	std::chrono::duration<double> elapsed = _tick_clock.FrameAt() - start;
	return elapsed.count();
}

static double Clock()
{
	return std::fmod(_frame_capture.SceneSeconds().value_or(SecondsSinceFirstFrame()), CLOCK_PERIOD_SECONDS);
}

static double SnowLine()
{
	return _settings_game.game_creation.landscape == LandscapeType::Arctic ? GetSnowLine() : NO_SNOW_LINE;
}

SceneView SceneView::Of(const Camera &camera)
{
	SceneView view;
	view.view = camera.ViewMatrix();
	view.projection = camera.ProjectionMatrix();
	view.view_projection = view.projection * view.view;
	view.frustum = FrustumOf(view.view_projection);
	view.eye = camera.Eye();
	view.sun = Sun();
	view.focal = camera.Focal();
	view.focus_pixels = camera.Ppt();
	view.near = camera.Near();
	view.far = camera.Far();
	view.focus = camera.FocusDistance();
	view.fog_start = std::min(view.focus * FOG_START_SHARE, view.far);
	view.shadow_reach = std::min(view.focus * SHADOW_REACH_SHARE, view.far);
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

double SceneView::Reach() const
{
	return this->far * std::hypot(1.0, this->viewport.width * 0.5 / this->focal, this->viewport.height * 0.5 / this->focal);
}

Frustum SceneView::Slice(double near, double far) const
{
	return FrustumOf(Mat4::Perspective(2.0 * this->focal / this->viewport.width, 2.0 * this->focal / this->viewport.height, near, far) * this->view);
}

std::array<Vec3, 8> SceneView::SliceCorners(double near, double far) const
{
	Vec3 right = {this->view.At(0, 0), this->view.At(0, 1), this->view.At(0, 2)};
	Vec3 up = {this->view.At(1, 0), this->view.At(1, 1), this->view.At(1, 2)};
	Vec3 across = right * (this->viewport.width * 0.5 / this->focal);
	Vec3 down = up * (this->viewport.height * 0.5 / this->focal);
	std::array<Vec3, 8> corners;
	auto corner = corners.begin();
	for (double depth : {near, far}) {
		for (const Vec3 &side : {across, across * -1.0}) {
			for (const Vec3 &edge : {down, down * -1.0}) *corner++ = this->eye + (side + edge - this->Back()) * depth;
		}
	}
	return corners;
}

/* How far across the map a shadow lands from beneath what casts it, for each unit of height the caster stands above where it lands. */
static MapVector ShadowFall(const Vec3 &sun)
{
	double rise = SunRise(sun);
	return {-sun.x / rise, -sun.y / rise};
}

std::pair<Vec3, Vec3> SunSwept(const Vec3 &sun, const Vec3 &low, const Vec3 &high)
{
	double floor = -DEEPEST_SINK * LevelRise();
	MapVector fall = ShadowFall(sun);
	double drop = high.z - floor;
	return {
		{low.x + std::min(fall.x * drop, 0.0), low.y + std::min(fall.y * drop, 0.0), std::min(low.z, floor)},
		{high.x + std::max(fall.x * drop, 0.0), high.y + std::max(fall.y * drop, 0.0), high.z},
	};
}

bool Sight::Sees(const Vec3 &low, const Vec3 &high) const
{
	if (!BoxMeets(this->frustum, low, high)) return false;
	if (this->decided == nullptr) return true;
	auto [shadow_low, shadow_high] = SunSwept(this->sun, low, high);
	return BoxMeets(*this->decided, shadow_low, shadow_high);
}

/* A box lying wholly in what a cascade decides casts its shadow from inside it. */
bool Sight::SeesWhole(const Vec3 &low, const Vec3 &high) const
{
	return BoxWithin(this->frustum, low, high) && (this->decided == nullptr || BoxWithin(*this->decided, low, high));
}

void SceneUniforms::Upload(const SceneView &view)
{
	auto floats = [](double x, double y, double z, double w) { return std::array<float, 4>{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(w)}; };
	Dimension map = _world_tiles.Size();
	SceneBlock block = {
		view.view.Floats(),
		view.projection.Floats(),
		view.view_projection.Floats(),
		floats(view.eye.x, view.eye.y, view.eye.z, 1.0),
		floats(view.sun.x, view.sun.y, view.sun.z, 1.0 / SunRise(view.sun)),
		floats(view.near, view.far, view.fog_start, view.focus),
		floats(map.width, map.height, _world_tiles.Peak(), LevelRise()),
		floats(view.viewport.width, view.viewport.height, view.clock, view.phase),
		floats(SnowLine(), 0.0, 0.0, 0.0),
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
