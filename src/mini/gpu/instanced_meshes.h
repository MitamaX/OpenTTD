/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file instanced_meshes.h A set of meshes on the GPU drawn many times over, each copy placed by attributes of its own streamed in per batch. */

#ifndef MINI_GPU_INSTANCED_MESHES_H
#define MINI_GPU_INSTANCED_MESHES_H

#include <cstddef>
#include <span>
#include <vector>

#include "mesh_buffer.h"

/* Every mesh shares one vertex layout and one pair of buffers; a mesh is named by its place in the list it was uploaded from.
 * Instance attributes follow the vertex attributes and advance once per copy. */
class InstancedMeshes {
public:
	template <class Mesh>
	void Upload(std::span<const Mesh> meshes, std::span<const VertexAttribute> vertex_layout, std::span<const VertexAttribute> instance_layout, size_t instance_stride)
	{
		using Vertex = typename decltype(Mesh::vertices)::value_type;
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		this->ranges.clear();
		for (const Mesh &mesh : meshes) {
			this->ranges.push_back({indices.size(), static_cast<int>(mesh.indices.size()), static_cast<int>(vertices.size())});
			vertices.insert(vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
			indices.insert(indices.end(), mesh.indices.begin(), mesh.indices.end());
		}
		this->Upload(std::as_bytes(std::span<const Vertex>(vertices)), sizeof(Vertex), vertex_layout, indices, instance_layout, instance_stride);
	}

	void Stream(std::span<const std::byte> instances);
	void Draw(size_t mesh, size_t first_instance, size_t instance_count) const;
	void Release();

	bool Ready() const { return this->vertex_array != 0; }

private:
	struct Range {
		size_t first_index;
		int index_count;
		int base_vertex;
	};

	void Upload(std::span<const std::byte> vertices, size_t vertex_stride, std::span<const VertexAttribute> vertex_layout, std::span<const uint32_t> indices, std::span<const VertexAttribute> instance_layout, size_t instance_stride);

	std::vector<Range> ranges;
	std::vector<VertexAttribute> instance_layout;
	size_t instance_stride = 0;
	uint32_t vertex_array = 0;
	uint32_t vertex_buffer = 0;
	uint32_t index_buffer = 0;
	uint32_t instance_buffer = 0;
};

/* The copies one view draws, gathered per mesh and streamed in together; each mesh's copies are drawn in one call. */
template <class Instance>
class InstanceBatch {
public:
	void Clear(size_t meshes)
	{
		this->buckets.resize(meshes);
		for (std::vector<Instance> &bucket : this->buckets) bucket.clear();
	}

	void Add(size_t mesh, const Instance &instance)
	{
		this->buckets[mesh].push_back(instance);
	}

	void Add(size_t mesh, std::span<const Instance> instances)
	{
		this->buckets[mesh].insert(this->buckets[mesh].end(), instances.begin(), instances.end());
	}

	/* Before each mesh's copies are drawn the caller is told which mesh they were gathered for, sets what differs per mesh, and names the mesh to draw them with. */
	template <class ChooseMesh>
	void Draw(InstancedMeshes &meshes, ChooseMesh choose_mesh)
	{
		this->staged.clear();
		for (const std::vector<Instance> &bucket : this->buckets) this->staged.insert(this->staged.end(), bucket.begin(), bucket.end());
		if (this->staged.empty()) return;

		meshes.Stream(std::as_bytes(std::span<const Instance>(this->staged)));
		size_t first = 0;
		for (size_t mesh = 0; mesh < this->buckets.size(); mesh++) {
			size_t count = this->buckets[mesh].size();
			if (count == 0) continue;
			meshes.Draw(choose_mesh(mesh), first, count);
			first += count;
		}
	}

private:
	std::vector<std::vector<Instance>> buckets;
	std::vector<Instance> staged;
};

#endif /* MINI_GPU_INSTANCED_MESHES_H */
