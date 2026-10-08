/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mesh_buffer.h An indexed triangle mesh living on the GPU, with the layout of its vertices. */

#ifndef MINI_GPU_MESH_BUFFER_H
#define MINI_GPU_MESH_BUFFER_H

#include <cstddef>
#include <span>
#include <vector>

enum class AttributeType : uint8_t {
	Float,
	NormalisedByte,
	NormalisedUnsignedByte,
};

struct VertexAttribute {
	uint32_t location;
	int components;
	AttributeType type;
	size_t offset;
};

/* Points the bound vertex array's attributes into the bound array buffer, from a byte offset on. */
void PointAttributes(std::span<const VertexAttribute> layout, size_t stride, size_t offset = 0);

/* Triangles over a list of vertices, as built on the CPU before upload. */
template <class Vertex>
struct TriangleList {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	uint32_t Add(const Vertex &vertex)
	{
		this->vertices.push_back(vertex);
		return static_cast<uint32_t>(this->vertices.size() - 1);
	}

	void Triangle(uint32_t a, uint32_t b, uint32_t c)
	{
		this->indices.insert(this->indices.end(), {a, b, c});
	}

	void Quad(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
	{
		this->Triangle(a, b, c);
		this->Triangle(a, c, d);
	}
};

class MeshBuffer {
public:
	template <class Vertex>
	void Upload(std::span<const Vertex> vertices, std::span<const VertexAttribute> layout, std::span<const uint32_t> indices)
	{
		this->Upload(std::as_bytes(vertices), sizeof(Vertex), layout, indices);
	}

	template <class Vertex>
	void Upload(const TriangleList<Vertex> &mesh, std::span<const VertexAttribute> layout)
	{
		this->Upload<Vertex>(mesh.vertices, layout, mesh.indices);
	}

	void Upload(std::span<const std::byte> vertices, size_t stride, std::span<const VertexAttribute> layout, std::span<const uint32_t> indices);
	void Draw() const;
	void Release();

	bool Empty() const { return this->index_count == 0; }
	size_t Bytes() const { return this->bytes; }

private:
	uint32_t vertex_array = 0;
	uint32_t vertex_buffer = 0;
	uint32_t index_buffer = 0;
	int index_count = 0;
	size_t bytes = 0;
};

#endif /* MINI_GPU_MESH_BUFFER_H */
