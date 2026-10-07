/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file shadow_map.cpp The sun's shadows over the world: a depth map per cascade of distance from the camera, drawn by every pass that casts. */

#include "../../stdafx.h"
#include "shadow_map.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "../../debug.h"
#include "../core/sunlight.h"
#include "../gpu/frame_profile.h"
#include "../gpu/gl_api.h"
#include "../map/world_tiles.h"
#include "frame_units.h"
#include "seabed.h"

#include "../../safeguards.h"

static constexpr const char *CASTER_DEFINES = "mini_ui/shaders/caster.glsl";
static constexpr std::array<const char *, 1> CASTER_FRAGMENT_SOURCES = {
	"mini_ui/shaders/caster.frag",
};
static constexpr double LOG_SPLIT_SHARE = 0.75;
static constexpr double FADE_START_SHARE = 0.85;
static constexpr double CASTER_HEADROOM_LEVELS = 24.0;
static constexpr double LOWEST_SUN_RISE = 0.1;
static constexpr double DEPTH_MARGIN = 1.0;
static constexpr float SLOPE_OFFSET = 2.0f;
static constexpr float CONSTANT_OFFSET = 2.0f;
static constexpr Vec3 SKY = {0.0, 0.0, 1.0};

/* The layout of the Shadows block in scene.glsl, std140. */
struct ShadowsBlock {
	std::array<std::array<float, 16>, ShadowMap::CASCADES> cascades;
	std::array<float, 16> caster;
	std::array<float, 4> cascade_far;
	std::array<float, 4> cascade_texel;
	std::array<float, 4> fade;
};

ShaderProgram CasterProgram(std::span<const char *const> vertex_sources)
{
	return CasterProgram(vertex_sources, CASTER_FRAGMENT_SOURCES);
}

ShaderProgram CasterProgram(std::span<const char *const> vertex_sources, std::span<const char *const> fragment_sources)
{
	std::vector<const char *> sources = {CASTER_DEFINES};
	sources.insert(sources.end(), vertex_sources.begin(), vertex_sources.end());
	return ShaderProgram(sources, fragment_sources);
}

ShaderProgram DepthProgram(std::span<const char *const> vertex_sources)
{
	return ShaderProgram(vertex_sources, CASTER_FRAGMENT_SOURCES);
}

static Vec3 SunWay()
{
	SunVector sun = Sun();
	return Normalised({sun.x, sun.y, sun.z});
}

/* The sun looks down its own rays, with the map's up kept upward on its picture. */
static Mat4 SunView()
{
	Vec3 back = SunWay();
	Vec3 right = Normalised(Cross(SKY, back));
	return Mat4::View({0.0, 0.0, 0.0}, right, Cross(back, right), back);
}

/* Clip space to texture space, where shaders look the cascades up. */
static Mat4 TextureSpace()
{
	Mat4 bias;
	for (int axis = 0; axis < 3; axis++) {
		bias.At(axis, axis) = 0.5;
		bias.At(axis, 3) = 0.5;
	}
	bias.At(3, 3) = 1.0;
	return bias;
}

/* Splits between evenly spaced and evenly scaled distances, so near cascades stay sharp and far ones still reach. */
static double SplitDistance(double near, double far, int index)
{
	double share = static_cast<double>(index) / ShadowMap::CASCADES;
	double scaled = near * std::pow(far / near, share);
	double even = near + (far - near) * share;
	return std::lerp(even, scaled, LOG_SPLIT_SHARE);
}

bool ShadowMap::Build()
{
	if (this->framebuffer != 0) return true;

	glGenTextures(1, &this->texture);
	glBindTexture(GL_TEXTURE_2D_ARRAY, this->texture);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	const std::array<float, 4> unshadowed = {1.0f, 1.0f, 1.0f, 1.0f};
	glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, unshadowed.data());
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, RESOLUTION, RESOLUTION, CASCADES, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);

	glGenFramebuffers(1, &this->framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer);
	glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, this->texture, 0, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) return true;

	Debug(misc, 0, "[mini] shadow framebuffer incomplete");
	this->Release();
	return false;
}

/* Each slice of the view is wrapped in the sphere around it, which keeps its size however the camera turns,
 * and the sphere is moved in whole texels so shadow edges hold still while the camera pans. */
ShadowMap::Cascade ShadowMap::FitSlice(const SceneView &camera, double near, double far) const
{
	double half_x = camera.viewport.width * 0.5 / camera.focal;
	double half_y = camera.viewport.height * 0.5 / camera.focal;
	double spread = half_x * half_x + half_y * half_y;
	double ahead = std::min((near + far) * (1.0 + spread) * 0.5, far);
	double radius = std::sqrt((far - ahead) * (far - ahead) + far * far * spread);

	Mat4 view = SunView();
	Vec3 centre = Transformed(view, camera.eye - camera.Back() * ahead);
	double texel = 2.0 * radius / RESOLUTION;
	centre.x = std::floor(centre.x / texel) * texel;
	centre.y = std::floor(centre.y / texel) * texel;

	double tallest = (_world_tiles.Peak() + CASTER_HEADROOM_LEVELS + DEEPEST_SINK) * LevelRise();
	double toward_sun = tallest / std::max(SunWay().z, LOWEST_SUN_RISE);
	Vec3 low = {centre.x - radius, centre.y - radius, centre.z - radius - DEPTH_MARGIN};
	Vec3 high = {centre.x + radius, centre.y + radius, centre.z + radius + toward_sun};
	return {Mat4::Orthographic(low, high) * view, far, texel};
}

std::array<ShadowMap::Cascade, ShadowMap::CASCADES> ShadowMap::Fit(const SceneView &camera) const
{
	std::array<Cascade, CASCADES> cascades;
	double reach = std::max(camera.shadow_reach, camera.near * 2.0);
	for (int index = 0; index < CASCADES; index++) {
		cascades[index] = this->FitSlice(camera, SplitDistance(camera.near, reach, index), SplitDistance(camera.near, reach, index + 1));
	}
	return cascades;
}

void ShadowMap::Upload(const std::array<Cascade, CASCADES> &cascades, const SceneView &camera)
{
	ShadowsBlock block{};
	Mat4 texture_space = TextureSpace();
	for (int index = 0; index < CASCADES; index++) {
		block.cascades[index] = (texture_space * cascades[index].view_projection).Floats();
		block.cascade_far[index] = static_cast<float>(cascades[index].far);
		block.cascade_texel[index] = static_cast<float>(cascades[index].texel);
	}
	block.fade = {static_cast<float>(camera.shadow_reach * FADE_START_SHARE), static_cast<float>(camera.shadow_reach), 0.0f, 0.0f};

	if (this->buffer == 0) glGenBuffers(1, &this->buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, this->buffer);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(block), &block, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, SHADOWS_BINDING, this->buffer);
}

void ShadowMap::Aim(const Mat4 &view_projection) const
{
	std::array<float, 16> caster = view_projection.Floats();
	glBindBuffer(GL_UNIFORM_BUFFER, this->buffer);
	glBufferSubData(GL_UNIFORM_BUFFER, offsetof(ShadowsBlock, caster), sizeof(caster), caster.data());
}

/* Casters nearer the sun than a cascade's box are pressed onto its near side instead of being cut away. */
void ShadowMap::Render(const SceneView &camera, std::span<const std::unique_ptr<WorldPass>> passes)
{
	if (!this->Build()) return;

	ProfileScope profile("shadows");
	std::array<Cascade, CASCADES> cascades = this->Fit(camera);
	this->Upload(cascades, camera);

	glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer);
	glViewport(0, 0, RESOLUTION, RESOLUTION);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glEnable(GL_DEPTH_CLAMP);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(SLOPE_OFFSET, CONSTANT_OFFSET);
	for (int index = 0; index < CASCADES; index++) {
		glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, this->texture, 0, index);
		glClear(GL_DEPTH_BUFFER_BIT);
		this->Aim(cascades[index].view_projection);
		ShadowView view = {cascades[index].view_projection, FrustumOf(cascades[index].view_projection), camera};
		for (const auto &pass : passes) {
			ProfileScope pass_profile("cast", pass->Name());
			pass->Cast(view);
		}
	}
	glDisable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_DEPTH_CLAMP);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void ShadowMap::Bind() const
{
	glActiveTexture(GL_TEXTURE0 + SHADOW_UNIT);
	glBindTexture(GL_TEXTURE_2D_ARRAY, this->texture);
}

void ShadowMap::Release()
{
	if (this->framebuffer != 0) glDeleteFramebuffers(1, &this->framebuffer);
	if (this->texture != 0) glDeleteTextures(1, &this->texture);
	if (this->buffer != 0) glDeleteBuffers(1, &this->buffer);
	*this = {};
}
