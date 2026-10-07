/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file span_mesh.h What stands clear of the ground over a bridge's span, whose thin members keep a pixel's width however far off they are seen. */

#ifndef MINI_WORLD_SPAN_MESH_H
#define MINI_WORLD_SPAN_MESH_H

#include <array>
#include <cstddef>

#include "../model/model_mesh.h"

/* A model's vertex, with how it lies off the axis of the member it belongs to: which way, in the member's half widths, and that half width in tiles.
 * Vertices of no member lie nowhere off one. */
struct SpanVertex {
	ModelVertex model;
	std::array<float, 4> girth;
};

inline constexpr std::array<VertexAttribute, 4> SPAN_LAYOUT = {{
	{0, 3, AttributeType::Float, offsetof(SpanVertex, model) + offsetof(ModelVertex, x)},
	{1, 4, AttributeType::NormalisedByte, offsetof(SpanVertex, model) + offsetof(ModelVertex, normal)},
	{2, 4, AttributeType::NormalisedUnsignedByte, offsetof(SpanVertex, model) + offsetof(ModelVertex, colour)},
	{3, 4, AttributeType::Float, offsetof(SpanVertex, girth)},
}};

struct SpanMesh : TriangleList<SpanVertex> {
	SpanMesh &Append(const ModelMesh &part);
	SpanMesh &Append(const SpanMesh &part);
	/* A member's mesh, around the axis from one point to another at this half width. */
	SpanMesh &AppendMember(const ModelMesh &member, const Vec3 &from, const Vec3 &to, double half);
	SpanMesh &Raise(double height);
};

#endif /* MINI_WORLD_SPAN_MESH_H */
