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

/* The list is recorded anew every frame, so each batch becomes a fresh mesh in the slot the last frame's batch held. */
void MapView::OnRender()
{
	Rml::RenderManager *render_manager = this->GetRenderManager();
	if (render_manager == nullptr) return;

	std::span<const DrawBatch> batches = _map_draw.Batches();
	this->batches.resize(batches.size());
	for (size_t i = 0; i < batches.size(); i++) this->Draw(*render_manager, this->batches[i], batches[i]);
}

/* The map is laid out in screen pixels wherever the element sits. */
void MapView::Draw(Rml::RenderManager &render_manager, Rml::Geometry &geometry, const DrawBatch &batch) const
{
	Rml::Mesh mesh = geometry.Release(Rml::Geometry::ReleaseMode::ClearMesh);
	for (const DrawVertex &vertex : _map_draw.Vertices().subspan(batch.first, batch.count)) {
		mesh.indices.push_back(static_cast<int>(mesh.vertices.size()));
		mesh.vertices.push_back(RmlVertex(vertex));
	}
	geometry = render_manager.MakeGeometry(std::move(mesh));

	Rml::Texture texture = batch.texture == NO_TEXTURE ? Rml::Texture() : render_manager.LoadTexture(RmlRenderer::TextureSource(batch.texture));
	geometry.Render(SCREEN_ORIGIN, texture);
}
