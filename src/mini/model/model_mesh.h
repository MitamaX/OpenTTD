/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file model_mesh.h A coloured triangle mesh of a model built in code, and the steps that shape, shade and combine its parts. */

#ifndef MINI_MODEL_MODEL_MESH_H
#define MINI_MODEL_MODEL_MESH_H

#include <array>
#include <cstddef>

#include "../core/space.h"
#include "../gpu/mesh_buffer.h"

/* Positions in tiles; the normal's last byte is a trait of the surface, the colour is picked on screen and its last byte is how open the surface lies to the sky. */
struct ModelVertex {
	float x;
	float y;
	float z;
	std::array<int8_t, 4> normal;
	std::array<uint8_t, 4> colour;

	Vec3 Position() const { return {this->x, this->y, this->z}; }
	Vec3 Normal() const;
	void Place(const Vec3 &position);
	void Face(const Vec3 &normal);
	void Paint(uint32_t rgb);
	void Occlude(double openness);
	void Trait(double share);
};

inline constexpr std::array<VertexAttribute, 3> MODEL_LAYOUT = {{
	{0, 3, AttributeType::Float, offsetof(ModelVertex, x)},
	{1, 4, AttributeType::NormalisedByte, offsetof(ModelVertex, normal)},
	{2, 4, AttributeType::NormalisedUnsignedByte, offsetof(ModelVertex, colour)},
}};

inline constexpr uint32_t MODEL_WHITE = 0xFFFFFF;

/* A part starts welded, white, open to the sky and opaque. Shaping steps run on the welded part so displaced corners stay joined;
 * Facet then gives every triangle corners of its own with the triangle's normal, after which each triangle can be shaded apart.
 * Weld merges corners at one spot whatever their shading, leaving the fewest corners to draw the shape depth only. */
struct ModelMesh : TriangleList<ModelVertex> {
	uint32_t Point(const Vec3 &position, const Vec3 &normal = {0.0, 0.0, 1.0});

	ModelMesh &Append(const ModelMesh &part);
	ModelMesh &Transform(const Mat4 &transform);
	ModelMesh &Move(const Vec3 &offset);
	ModelMesh &Displace(double reach, uint32_t seed);
	ModelMesh &Facet();
	ModelMesh &Smooth();
	ModelMesh &Round(const Vec3 &centre, double share);
	ModelMesh &Paint(uint32_t rgb);
	ModelMesh &Vary(double spread, uint32_t seed);
	ModelMesh &Weld();

	/* The trait is how much light foliage lets through, how glossy a solid is, or how brightly a lamp glows, which reads as a negative share. */
	ModelMesh &Translucent(double share) { return this->Trait(share); }
	ModelMesh &Gloss(double share) { return this->Trait(share); }
	ModelMesh &Glow(double share) { return this->Trait(-share); }

	template <class Tone>
	ModelMesh &PaintBy(Tone tone)
	{
		for (ModelVertex &vertex : this->vertices) vertex.Paint(tone(vertex.Position()));
		return *this;
	}

	template <class Openness>
	ModelMesh &Occlude(Openness openness)
	{
		for (ModelVertex &vertex : this->vertices) vertex.Occlude(openness(vertex.Position()));
		return *this;
	}

private:
	ModelMesh &Trait(double share);
};

#endif /* MINI_MODEL_MODEL_MESH_H */
