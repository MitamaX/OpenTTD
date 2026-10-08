/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mesh_buffer.cpp An indexed triangle mesh living on the GPU, with the layout of its vertices. */

#include "../../stdafx.h"
#include "mesh_buffer.h"

#include <array>

#include "frame_profile.h"
#include "gl_api.h"

#include "../../safeguards.h"

static constexpr std::array<uint32_t, 3> ONE_CORNER = {0, 0, 0};

static GLenum ComponentType(AttributeType type)
{
	switch (type) {
		case AttributeType::Float: return GL_FLOAT;
		case AttributeType::NormalisedByte: return GL_BYTE;
		default: return GL_UNSIGNED_BYTE;
	}
}

void PointAttributes(std::span<const VertexAttribute> layout, size_t stride, size_t offset)
{
	for (const VertexAttribute &attribute : layout) {
		glEnableVertexAttribArray(attribute.location);
		GLboolean normalised = attribute.type == AttributeType::Float ? GL_FALSE : GL_TRUE;
		glVertexAttribPointer(attribute.location, attribute.components, ComponentType(attribute.type), normalised, static_cast<GLsizei>(stride), reinterpret_cast<const void *>(offset + attribute.offset));
	}
}

void DrawBlankTriangle(std::span<const VertexAttribute> layout, size_t stride)
{
	std::vector<std::byte> vertex(stride);
	MeshBuffer triangle;
	triangle.Upload(vertex, stride, layout, ONE_CORNER);
	triangle.Draw();
	triangle.Release();
}

void MeshBuffer::Upload(std::span<const std::byte> vertices, size_t stride, std::span<const VertexAttribute> layout, std::span<const uint32_t> indices)
{
	if (this->vertex_array == 0) {
		glGenVertexArrays(1, &this->vertex_array);
		glGenBuffers(1, &this->vertex_buffer);
		glGenBuffers(1, &this->index_buffer);
	}

	_frame_profile.Count("mesh_uploads");
	_frame_profile.Count("mesh_bytes", vertices.size() + indices.size_bytes());
	glBindVertexArray(this->vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, this->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size()), vertices.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->index_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size_bytes()), indices.data(), GL_STATIC_DRAW);
	PointAttributes(layout, stride);
	glBindVertexArray(0);
	this->index_count = static_cast<int>(indices.size());
	this->bytes = vertices.size() + indices.size_bytes();
}

void MeshBuffer::Draw() const
{
	if (this->Empty()) return;
	glBindVertexArray(this->vertex_array);
	glDrawElements(GL_TRIANGLES, this->index_count, GL_UNSIGNED_INT, nullptr);
}

void MeshBuffer::Release()
{
	if (this->vertex_array != 0) {
		glDeleteVertexArrays(1, &this->vertex_array);
		glDeleteBuffers(1, &this->vertex_buffer);
		glDeleteBuffers(1, &this->index_buffer);
	}
	*this = {};
}
