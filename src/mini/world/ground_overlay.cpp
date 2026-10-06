/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ground_overlay.cpp Shapes laid on the world's ground, drawn over the finished world against its depth, so what stands in front of them hides them. */

#include "../../stdafx.h"
#include "ground_overlay.h"

#include <array>
#include <cstddef>
#include <vector>

#include "../gpu/gl_api.h"

#include "../../safeguards.h"

static constexpr std::array<const char *, 1> OVERLAY_VERTEX = {"mini_ui/shaders/ground_overlay.vert"};
static constexpr std::array<const char *, 3> OVERLAY_FRAGMENT = {"mini_ui/shaders/scene.glsl", "mini_ui/shaders/post.glsl", "mini_ui/shaders/ground_overlay.frag"};
static constexpr uint DEPTH_UNIT = 0;

static constexpr std::array<VertexAttribute, 3> OVERLAY_LAYOUT = {{
	{0, 2, AttributeType::Float, offsetof(DrawVertex, x)},
	{1, 4, AttributeType::NormalisedUnsignedByte, offsetof(DrawVertex, rgba)},
	{2, 1, AttributeType::Float, offsetof(DrawVertex, ahead)},
}};

GroundOverlay::GroundOverlay() : program(OVERLAY_VERTEX, OVERLAY_FRAGMENT)
{
}

void GroundOverlay::Release()
{
	this->program.Release();
	this->mesh.Release();
}

/* Each batch counts its indices from its own first vertex; laid end to end they count from the list's start. */
void GroundOverlay::Upload(const DrawList &list)
{
	std::vector<uint32_t> indices;
	indices.reserve(list.Indices().size());
	for (const DrawBatch &batch : list.Batches()) {
		for (uint32_t index : list.Indices().subspan(batch.first_index, batch.index_count)) indices.push_back(batch.first_vertex + index);
	}
	this->mesh.Upload(list.Vertices(), OVERLAY_LAYOUT, indices);
}

/* Drawn in RmlUi's layer with its blending, in the same pixels as the world laid there under it. */
void GroundOverlay::Draw(const WorldTarget &target) const
{
	if (this->mesh.Empty()) return;

	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	this->program.Use();
	glUniform2f(this->program.Uniform("u_viewport"), static_cast<float>(viewport[2]), static_cast<float>(viewport[3]));
	this->program.BindSampler("u_depth", DEPTH_UNIT);
	glActiveTexture(GL_TEXTURE0 + DEPTH_UNIT);
	glBindTexture(GL_TEXTURE_2D, target.Depth());
	this->mesh.Draw();
}
