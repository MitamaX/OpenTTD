/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file span_mesh.cpp What stands clear of the ground over a bridge's span, whose thin members keep a pixel's width however far off they are seen. */

#include "../../stdafx.h"
#include "span_mesh.h"

#include "../../safeguards.h"

static constexpr std::array<float, 4> OFF_MEMBERS = {0.0f, 0.0f, 0.0f, 0.0f};

SpanMesh &SpanMesh::Append(const SpanMesh &part)
{
	uint32_t base = static_cast<uint32_t>(this->vertices.size());
	this->vertices.insert(this->vertices.end(), part.vertices.begin(), part.vertices.end());
	for (uint32_t index : part.indices) this->indices.push_back(base + index);
	return *this;
}

SpanMesh &SpanMesh::Append(const ModelMesh &part)
{
	SpanMesh span;
	for (const ModelVertex &vertex : part.vertices) span.vertices.push_back({vertex, OFF_MEMBERS});
	span.indices = part.indices;
	return this->Append(span);
}

SpanMesh &SpanMesh::AppendMember(const ModelMesh &member, const Vec3 &from, const Vec3 &to, double half)
{
	Vec3 axis = Normalised(to - from);
	SpanMesh span;
	for (const ModelVertex &vertex : member.vertices) {
		Vec3 off = vertex.Position() - from;
		Vec3 across = (off - axis * Dot(off, axis)) * (1.0 / half);
		span.vertices.push_back({vertex, {static_cast<float>(across.x), static_cast<float>(across.y), static_cast<float>(across.z), static_cast<float>(half)}});
	}
	span.indices = member.indices;
	return this->Append(span);
}

SpanMesh &SpanMesh::Raise(double height)
{
	for (SpanVertex &vertex : this->vertices) {
		Vec3 at = vertex.model.Position();
		vertex.model.Place({at.x, at.y, at.z + height});
	}
	return *this;
}
