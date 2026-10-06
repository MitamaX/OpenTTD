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
};

struct VertexAttribute {
	uint32_t location;
	int components;
	AttributeType type;
	size_t offset;
};

class MeshBuffer {
public:
	template <class Vertex>
	void Upload(std::span<const Vertex> vertices, std::span<const VertexAttribute> layout, std::span<const uint32_t> indices)
	{
		this->Upload(std::as_bytes(vertices), sizeof(Vertex), layout, indices);
	}

	void Upload(std::span<const std::byte> vertices, size_t stride, std::span<const VertexAttribute> layout, std::span<const uint32_t> indices);
	void Draw() const;
	void Release();

	bool Empty() const { return this->index_count == 0; }

private:
	uint32_t vertex_array = 0;
	uint32_t vertex_buffer = 0;
	uint32_t index_buffer = 0;
	int index_count = 0;
};

#endif /* MINI_GPU_MESH_BUFFER_H */
