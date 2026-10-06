/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_view.cpp The map as an RmlUi element: the frame's map draw list, drawn as RmlUi meshes. */

#include "../../stdafx.h"
#include "map_view.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <iterator>

#include "../core/canvas.h"
#include "rml_renderer.h"

#include "../../safeguards.h"

static const Rml::Vector2f SCREEN_ORIGIN(0.0f, 0.0f);

/* RmlUi blends premultiplied colours; the draw list records them straight. */
static Rml::Vertex RmlVertex(const DrawVertex &vertex)
{
	Rml::Colourb colour(vertex.rgba[0], vertex.rgba[1], vertex.rgba[2], vertex.rgba[3]);
	return {Rml::Vector2f(vertex.x, vertex.y), colour.ToPremultiplied(), Rml::Vector2f(vertex.u, vertex.v)};
}

MapView::MapView(const Rml::String &tag) : Rml::Element(tag)
{
}

void MapView::OnRender()
{
	Rml::RenderManager *render_manager = this->GetRenderManager();
	if (render_manager == nullptr) return;

	this->DrawBatches(*render_manager, _map_draw);
}

/* The list is recorded anew every frame, so each batch becomes a fresh mesh in the slot the last frame's batch held. */
void MapView::DrawBatches(Rml::RenderManager &render_manager, const DrawList &list)
{
	std::span<const DrawBatch> batches = list.Batches();
	this->batches.resize(batches.size());
	for (size_t i = 0; i < batches.size(); i++) this->Draw(render_manager, this->batches[i], list, batches[i]);
}

/* The map is laid out in screen pixels wherever the element sits. */
void MapView::Draw(Rml::RenderManager &render_manager, Rml::Geometry &geometry, const DrawList &list, const DrawBatch &batch) const
{
	Rml::Mesh mesh = geometry.Release(Rml::Geometry::ReleaseMode::ClearMesh);
	std::ranges::transform(list.Vertices().subspan(batch.first_vertex, batch.vertex_count), std::back_inserter(mesh.vertices), RmlVertex);
	std::ranges::copy(list.Indices().subspan(batch.first_index, batch.index_count), std::back_inserter(mesh.indices));
	geometry = render_manager.MakeGeometry(std::move(mesh));

	Rml::Texture texture = batch.texture == NO_TEXTURE ? Rml::Texture() : render_manager.LoadTexture(RmlRenderer::TextureSource(batch.texture));
	geometry.Render(SCREEN_ORIGIN, texture);
}
