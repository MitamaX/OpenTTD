/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file post_chain.cpp The world's finishing: occlusion, sky and haze, antialiasing, bloom, depth of field and the tone curve that brings it to the screen. */

#include "../../stdafx.h"
#include "post_chain.h"

#include <algorithm>
#include <cmath>

#include "../core/camera.h"
#include "../core/tuning.h"
#include "../gpu/frame_profile.h"
#include "../gpu/gl_api.h"

#include "../../safeguards.h"

static constexpr std::array<const char *, 1> POST_VERTEX = {"mini_ui/shaders/post.vert"};
static constexpr std::array<const char *, 3> OCCLUSION_SOURCES = {"mini_ui/shaders/scene.glsl", "mini_ui/shaders/post.glsl", "mini_ui/shaders/occlusion.frag"};
static constexpr std::array<const char *, 10> SHADE_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/noise.glsl",
	"mini_ui/shaders/common.glsl",
	"mini_ui/shaders/sky.glsl",
	"mini_ui/shaders/cloud_field.glsl",
	"mini_ui/shaders/clouds.glsl",
	"mini_ui/shaders/shadow.glsl",
	"mini_ui/shaders/lighting.glsl",
	"mini_ui/shaders/post.glsl",
	"mini_ui/shaders/shade.frag",
};
static constexpr std::array<const char *, 3> TEMPORAL_SOURCES = {"mini_ui/shaders/scene.glsl", "mini_ui/shaders/post.glsl", "mini_ui/shaders/temporal.frag"};
static constexpr std::array<const char *, 3> EDGES_SOURCES = {"mini_ui/shaders/scene.glsl", "mini_ui/shaders/post.glsl", "mini_ui/shaders/edges.frag"};
static constexpr std::array<const char *, 3> BLOOM_DOWN_SOURCES = {"mini_ui/shaders/scene.glsl", "mini_ui/shaders/post.glsl", "mini_ui/shaders/bloom_down.frag"};
static constexpr std::array<const char *, 1> BLOOM_UP_SOURCES = {"mini_ui/shaders/bloom_up.frag"};
static constexpr std::array<const char *, 1> SCREEN_VERTEX = {"mini_ui/shaders/screen.vert"};
static constexpr std::array<const char *, 3> COMPOSITE_SOURCES = {"mini_ui/shaders/scene.glsl", "mini_ui/shaders/post.glsl", "mini_ui/shaders/composite.frag"};

static constexpr size_t BLOOM_LEVELS = 6;
static constexpr float BLOOM_STRENGTH = 0.05f;
static constexpr float BLOOM_SPREAD = 0.7f;
static constexpr float OCCLUSION_STRENGTH = 1.0f;
static constexpr float TEMPORAL_HISTORY = 0.9f;
static constexpr float TEMPORAL_SHARPEN = 0.3f;
static constexpr double BASE_EXPOSURE = 0.82;
static constexpr double BLUR_SHARE_OF_HEIGHT = 0.012;
static constexpr uint JITTER_PHASES = 8;
static constexpr int FULL_SCREEN_CORNERS = 3;
static constexpr int AREA_CORNERS = 4;

/** The textures each finishing step reads, as their samplers name them. */
enum PostUnit : uint8_t {
	SOURCE_UNIT,
	DEPTH_UNIT,
	HISTORY_UNIT,
	EXTRA_UNIT,
};

/* A point of the low discrepancy sequence along one prime base, spreading samples evenly over the pixel. */
static double Halton(uint index, uint base)
{
	double fraction = 1.0;
	double result = 0.0;
	for (uint i = index; i > 0; i /= base) {
		fraction /= base;
		result += fraction * (i % base);
	}
	return result;
}

static Dimension Halved(Dimension size)
{
	return Dimension(std::max(size.width / 2, 1u), std::max(size.height / 2, 1u));
}

static void BindDepth(const WorldTarget &target, uint unit)
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, target.Depth());
}

static Antialiasing ChosenAntialiasing()
{
	return static_cast<Antialiasing>(_tuning.anti_aliasing);
}

PostChain::PostChain() :
	occlusion(POST_VERTEX, OCCLUSION_SOURCES),
	shade(POST_VERTEX, SHADE_SOURCES),
	temporal(POST_VERTEX, TEMPORAL_SOURCES),
	edges(POST_VERTEX, EDGES_SOURCES),
	bloom_down(POST_VERTEX, BLOOM_DOWN_SOURCES),
	bloom_up(POST_VERTEX, BLOOM_UP_SOURCES),
	composite(SCREEN_VERTEX, COMPOSITE_SOURCES),
	ambient(GL_RG16F, TargetSampling::Nearest),
	lit(GL_RGBA16F, TargetSampling::Linear),
	history({PostTarget(GL_RGBA16F, TargetSampling::Linear), PostTarget(GL_RGBA16F, TargetSampling::Linear)})
{
	this->bloom.assign(BLOOM_LEVELS, PostTarget(GL_R11F_G11F_B10F, TargetSampling::Linear));
}

std::vector<ShaderProgram *> PostChain::Programs()
{
	return {&this->occlusion, &this->shade, &this->temporal, &this->edges, &this->bloom_down, &this->bloom_up, &this->composite};
}

void PostChain::Reload()
{
	for (ShaderProgram *program : this->Programs()) program->Reload();
	this->history_valid = false;
}

bool PostChain::Ready()
{
	bool ready = true;
	for (ShaderProgram *program : this->Programs()) ready &= program->Ready();
	if (ready && this->quad == 0) glGenVertexArrays(1, &this->quad);
	return ready;
}

void PostChain::Release()
{
	for (ShaderProgram *program : this->Programs()) program->Release();
	this->ambient.Release();
	this->lit.Release();
	for (PostTarget &target : this->history) target.Release();
	for (PostTarget &target : this->bloom) target.Release();
	if (this->quad != 0) glDeleteVertexArrays(1, &this->quad);
	this->quad = 0;
	this->resolved = nullptr;
	this->history_valid = false;
}

SceneView PostChain::Jitter(const SceneView &view)
{
	this->frame++;
	if (ChosenAntialiasing() != Antialiasing::Temporal) return view;
	uint phase = this->frame % JITTER_PHASES + 1;
	return view.Jittered(Halton(phase, 2) - 0.5, Halton(phase, 3) - 0.5, phase);
}

bool PostChain::Fit(Dimension size)
{
	Dimension half = Halved(size);
	bool fitted = this->ambient.Fit(half) && this->lit.Fit(size);
	for (PostTarget &target : this->history) {
		if (target.Size() != size) this->history_valid = false;
		fitted = fitted && target.Fit(size);
	}
	for (PostTarget &target : this->bloom) {
		fitted = fitted && target.Fit(half);
		half = Halved(half);
	}
	return fitted;
}

/* Every step after the world is drawn reads the target's depth, and none of them tests or writes depth of its own. */
void PostChain::Finish(const WorldTarget &target, const SceneView &view)
{
	if (!this->Fit(target.Size())) {
		this->resolved = nullptr;
		return;
	}

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_BLEND);
	glBindVertexArray(this->quad);
	if (_tuning.ambient_occlusion != 0) this->Occlude(target);
	this->Shade(target);
	this->resolved = &this->Resolve(target);
	if (_tuning.bloom != 0) this->Bloom(*this->resolved);
	this->previous_view_projection = view.view_projection;
}

void PostChain::Occlude(const WorldTarget &target)
{
	ProfileScope profile("post", "occlude");
	this->ambient.Bind();
	this->occlusion.Use();
	this->occlusion.BindSampler("u_depth", DEPTH_UNIT);
	BindDepth(target, DEPTH_UNIT);
	this->DrawQuad();
}

void PostChain::Shade(const WorldTarget &target)
{
	ProfileScope profile("post", "shade");
	this->lit.Bind();
	this->shade.Use();
	this->shade.BindSampler("u_colour", SOURCE_UNIT);
	this->shade.BindSampler("u_depth", DEPTH_UNIT);
	this->shade.BindSampler("u_occlusion", EXTRA_UNIT);
	glUniform1f(this->shade.Uniform("u_occlusion_strength"), _tuning.ambient_occlusion != 0 ? OCCLUSION_STRENGTH : 0.0f);
	glActiveTexture(GL_TEXTURE0 + SOURCE_UNIT);
	glBindTexture(GL_TEXTURE_2D, target.Colour());
	BindDepth(target, DEPTH_UNIT);
	this->ambient.BindTexture(EXTRA_UNIT);
	this->DrawQuad();
}

/* Temporal antialiasing blends this frame into the last one's picture, so the two history targets take turns; edge antialiasing smooths the lit picture once. */
const PostTarget &PostChain::Resolve(const WorldTarget &target)
{
	ProfileScope profile("post", "resolve");
	switch (ChosenAntialiasing()) {
		case Antialiasing::Off:
			this->history_valid = false;
			return this->lit;

		case Antialiasing::Edges:
			this->history_valid = false;
			this->history[0].Bind();
			this->edges.Use();
			this->edges.BindSampler("u_current", SOURCE_UNIT);
			this->lit.BindTexture(SOURCE_UNIT);
			this->DrawQuad();
			return this->history[0];

		case Antialiasing::Temporal:
			break;
	}

	const PostTarget &previous = this->history[this->current];
	this->current ^= 1;
	const PostTarget &next = this->history[this->current];
	next.Bind();
	this->temporal.Use();
	this->temporal.BindSampler("u_current", SOURCE_UNIT);
	this->temporal.BindSampler("u_depth", DEPTH_UNIT);
	this->temporal.BindSampler("u_history", HISTORY_UNIT);
	glUniformMatrix4fv(this->temporal.Uniform("u_reprojection"), 1, GL_FALSE, this->previous_view_projection.Floats().data());
	glUniform1f(this->temporal.Uniform("u_history_weight"), this->history_valid ? TEMPORAL_HISTORY : 0.0f);
	this->lit.BindTexture(SOURCE_UNIT);
	BindDepth(target, DEPTH_UNIT);
	previous.BindTexture(HISTORY_UNIT);
	this->DrawQuad();
	this->history_valid = true;
	return next;
}

/* Each level halves the one above, then from the smallest up each level blends a soft tent of the one below into itself. */
void PostChain::Bloom(const PostTarget &source)
{
	ProfileScope profile("post", "bloom");
	this->bloom_down.Use();
	this->bloom_down.BindSampler("u_source", SOURCE_UNIT);
	const PostTarget *above = &source;
	for (const PostTarget &level : this->bloom) {
		level.Bind();
		glUniform1i(this->bloom_down.Uniform("u_first"), above == &source ? 1 : 0);
		above->BindTexture(SOURCE_UNIT);
		this->DrawQuad();
		above = &level;
	}

	this->bloom_up.Use();
	this->bloom_up.BindSampler("u_source", SOURCE_UNIT);
	glEnable(GL_BLEND);
	glBlendColor(0.0f, 0.0f, 0.0f, BLOOM_SPREAD);
	glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
	for (size_t level = this->bloom.size() - 1; level > 0; level--) {
		this->bloom[level - 1].Bind();
		this->bloom[level].BindTexture(SOURCE_UNIT);
		this->DrawQuad();
	}
	glDisable(GL_BLEND);
}

/* The quad covers the element in RmlUi's pixels over the whole of the layer RmlUi is drawing into. */
void PostChain::Present(const ShaderArea &area, Dimension layer, const WorldTarget &target)
{
	if (this->resolved == nullptr) return;

	ProfileScope profile("post", "present");
	bool bloom = _tuning.bloom != 0;
	bool focus = _tuning.depth_of_field != 0;
	this->composite.Use();
	glUniform4f(this->composite.Uniform("u_area"), area.left, area.top, area.right, area.bottom);
	glUniform2f(this->composite.Uniform("u_viewport"), static_cast<float>(layer.width), static_cast<float>(layer.height));
	glUniform1f(this->composite.Uniform("u_exposure"), static_cast<float>(BASE_EXPOSURE * std::exp2(_tuning.exposure)));
	glUniform1f(this->composite.Uniform("u_bloom_strength"), bloom ? BLOOM_STRENGTH : 0.0f);
	glUniform1f(this->composite.Uniform("u_sharpen"), ChosenAntialiasing() == Antialiasing::Temporal ? TEMPORAL_SHARPEN : 0.0f);
	glUniform1f(this->composite.Uniform("u_focus"), focus ? static_cast<float>(_camera.FocusDistance()) : 0.0f);
	glUniform1f(this->composite.Uniform("u_blur_pixels"), static_cast<float>(BLUR_SHARE_OF_HEIGHT * target.Size().height));
	this->composite.BindSampler("u_resolved", SOURCE_UNIT);
	this->composite.BindSampler("u_depth", DEPTH_UNIT);
	this->composite.BindSampler("u_bloom", EXTRA_UNIT);
	this->resolved->BindTexture(SOURCE_UNIT);
	BindDepth(target, DEPTH_UNIT);
	this->bloom.front().BindTexture(EXTRA_UNIT);
	glBindVertexArray(this->quad);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, AREA_CORNERS);
}

void PostChain::DrawQuad() const
{
	glDrawArrays(GL_TRIANGLES, 0, FULL_SCREEN_CORNERS);
}
