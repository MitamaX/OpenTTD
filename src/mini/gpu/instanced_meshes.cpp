/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file instanced_meshes.cpp A set of meshes on the GPU drawn many times over, each copy placed by attributes of its own streamed in per batch. */

#include "../../stdafx.h"
#include "instanced_meshes.h"

#include "gl_api.h"

#include "../../safeguards.h"

static constexpr GLuint PER_INSTANCE = 1;

void InstancedMeshes::Upload(std::span<const std::byte> vertices, size_t vertex_stride, std::span<const VertexAttribute> vertex_layout, std::span<const uint32_t> indices, std::span<const VertexAttribute> instance_layout, size_t instance_stride)
{
	if (this->vertex_array == 0) {
		glGenVertexArrays(1, &this->vertex_array);
		glGenBuffers(1, &this->vertex_buffer);
		glGenBuffers(1, &this->index_buffer);
		glGenBuffers(1, &this->instance_buffer);
	}
	this->instance_layout.assign(instance_layout.begin(), instance_layout.end());
	this->instance_stride = instance_stride;

	glBindVertexArray(this->vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, this->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size()), vertices.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->index_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size_bytes()), indices.data(), GL_STATIC_DRAW);
	PointAttributes(vertex_layout, vertex_stride);
	glBindBuffer(GL_ARRAY_BUFFER, this->instance_buffer);
	PointAttributes(this->instance_layout, this->instance_stride);
	for (const VertexAttribute &attribute : this->instance_layout) glVertexAttribDivisor(attribute.location, PER_INSTANCE);
	glBindVertexArray(0);
}

/* Fresh storage each time, so the driver need not wait for draws still reading the last batch. */
void InstancedMeshes::Stream(std::span<const std::byte> instances)
{
	glBindBuffer(GL_ARRAY_BUFFER, this->instance_buffer);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(instances.size()), instances.data(), GL_STREAM_DRAW);
}

/* GL 3.3 has no base instance, so the instance attributes are pointed at the first copy instead. */
void InstancedMeshes::Draw(size_t mesh, size_t first_instance, size_t instance_count) const
{
	const Range &range = this->ranges[mesh];
	if (range.index_count == 0 || instance_count == 0) return;
	glBindVertexArray(this->vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, this->instance_buffer);
	PointAttributes(this->instance_layout, this->instance_stride, first_instance * this->instance_stride);
	const void *first_index = reinterpret_cast<const void *>(range.first_index * sizeof(uint32_t));
	glDrawElementsInstancedBaseVertex(GL_TRIANGLES, range.index_count, GL_UNSIGNED_INT, first_index, static_cast<GLsizei>(instance_count), range.base_vertex);
}

void InstancedMeshes::Release()
{
	if (this->vertex_array != 0) {
		glDeleteVertexArrays(1, &this->vertex_array);
		glDeleteBuffers(1, &this->vertex_buffer);
		glDeleteBuffers(1, &this->index_buffer);
		glDeleteBuffers(1, &this->instance_buffer);
	}
	*this = {};
}
