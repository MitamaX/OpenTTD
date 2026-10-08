/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file model_mesh.cpp A coloured triangle mesh of a model built in code, and the steps that shape, shade and combine its parts. */

#include "../../stdafx.h"
#include "model_mesh.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

#include "../core/seed.h"
#include "../gpu/draw_list.h"

#include "../../safeguards.h"

static constexpr double NORMAL_SCALE = 127.0;
static constexpr double WELD_GRID = 1.0e4;
static constexpr std::array<uint32_t, 3> AXIS_SALTS = {0x9E3779B9U, 0x85EBCA6BU, 0xC2B2AE35U};

/* Halves round away from zero, as std::lround rounds them. */
static int RoundedAway(double value)
{
	return static_cast<int>(value + std::copysign(0.5, value));
}

static int8_t SignedByte(double share)
{
	return static_cast<int8_t>(RoundedAway(std::clamp(share, -1.0, 1.0) * NORMAL_SCALE));
}

static uint8_t UnsignedByte(double share)
{
	return static_cast<uint8_t>(RoundedAway(std::clamp(share, 0.0, 1.0) * CHANNEL_MAX));
}

Vec3 ModelVertex::Normal() const
{
	return Vec3{static_cast<double>(this->normal[0]), static_cast<double>(this->normal[1]), static_cast<double>(this->normal[2])} * (1.0 / NORMAL_SCALE);
}

void ModelVertex::Place(const Vec3 &position)
{
	this->x = static_cast<float>(position.x);
	this->y = static_cast<float>(position.y);
	this->z = static_cast<float>(position.z);
}

void ModelVertex::Face(const Vec3 &normal)
{
	Vec3 unit = Normalised(normal);
	this->normal = {SignedByte(unit.x), SignedByte(unit.y), SignedByte(unit.z), this->normal[3]};
}

void ModelVertex::Paint(uint32_t rgb)
{
	this->colour = {static_cast<uint8_t>(Red(rgb)), static_cast<uint8_t>(Green(rgb)), static_cast<uint8_t>(Blue(rgb)), this->colour[3]};
}

void ModelVertex::Occlude(double openness)
{
	this->colour[3] = UnsignedByte(openness);
}

void ModelVertex::Trait(double share)
{
	this->normal[3] = SignedByte(share);
}

uint32_t ModelMesh::Point(const Vec3 &position, const Vec3 &normal)
{
	ModelVertex vertex{};
	vertex.Place(position);
	vertex.Face(normal);
	vertex.Paint(MODEL_WHITE);
	vertex.Occlude(1.0);
	return this->Add(vertex);
}

ModelMesh &ModelMesh::Append(const ModelMesh &part)
{
	uint32_t base = static_cast<uint32_t>(this->vertices.size());
	this->vertices.insert(this->vertices.end(), part.vertices.begin(), part.vertices.end());
	for (uint32_t index : part.indices) this->indices.push_back(base + index);
	return *this;
}

ModelMesh &ModelMesh::Transform(const Mat4 &transform)
{
	for (ModelVertex &vertex : this->vertices) {
		vertex.Place(Transformed(transform, vertex.Position()));
		vertex.Face(TransformedNormal(transform, vertex.Normal()));
	}
	return *this;
}

/* Moving a part leaves which way its faces look as they were. */
ModelMesh &ModelMesh::Move(const Vec3 &offset)
{
	for (ModelVertex &vertex : this->vertices) vertex.Place(vertex.Position() + offset);
	return *this;
}

/* The spot a corner lies on, the same for corners whose positions differ only by rounding. */
static std::array<int32_t, 3> Spot(const Vec3 &at)
{
	auto cell = [](double coordinate) { return static_cast<int32_t>(std::lround(coordinate * WELD_GRID)); };
	return {cell(at.x), cell(at.y), cell(at.z)};
}

/* Corners at the same spot move alike, whichever triangles they were built for. */
static Vec3 Wobble(const Vec3 &at, double reach, uint32_t seed)
{
	auto [x, y, z] = Spot(at);
	uint32_t spot = Hash32(seed ^ Hash32(static_cast<uint32_t>(x) ^ Hash32(static_cast<uint32_t>(y) ^ Hash32(static_cast<uint32_t>(z)))));
	auto offset = [&](int axis) { return (SeedShare(SubSeed(spot, AXIS_SALTS[axis]), 0, SeedDice::SHARE_BITS) * 2.0 - 1.0) * reach; };
	return {offset(0), offset(1), offset(2)};
}

ModelMesh &ModelMesh::Displace(double reach, uint32_t seed)
{
	for (ModelVertex &vertex : this->vertices) vertex.Place(vertex.Position() + Wobble(vertex.Position(), reach, seed));
	return *this;
}

static Vec3 TriangleNormal(const ModelVertex &a, const ModelVertex &b, const ModelVertex &c)
{
	return Cross(b.Position() - a.Position(), c.Position() - a.Position());
}

ModelMesh &ModelMesh::Facet()
{
	std::vector<ModelVertex> corners;
	corners.reserve(this->indices.size());
	for (size_t first = 0; first + 2 < this->indices.size(); first += 3) {
		std::array<ModelVertex, 3> triangle = {this->vertices[this->indices[first]], this->vertices[this->indices[first + 1]], this->vertices[this->indices[first + 2]]};
		Vec3 normal = TriangleNormal(triangle[0], triangle[1], triangle[2]);
		for (ModelVertex &corner : triangle) {
			corner.Face(normal);
			corners.push_back(corner);
		}
	}
	this->vertices = std::move(corners);
	for (size_t index = 0; index < this->indices.size(); index++) this->indices[index] = static_cast<uint32_t>(index);
	return *this;
}

/* Each corner takes the normals of the triangles sharing it, weighed by their areas. */
ModelMesh &ModelMesh::Smooth()
{
	std::vector<Vec3> sums(this->vertices.size(), Vec3{0.0, 0.0, 0.0});
	for (size_t first = 0; first + 2 < this->indices.size(); first += 3) {
		Vec3 normal = TriangleNormal(this->vertices[this->indices[first]], this->vertices[this->indices[first + 1]], this->vertices[this->indices[first + 2]]);
		for (size_t corner = 0; corner < 3; corner++) sums[this->indices[first + corner]] = sums[this->indices[first + corner]] + normal;
	}
	for (size_t index = 0; index < this->vertices.size(); index++) this->vertices[index].Face(sums[index]);
	return *this;
}

/* Normals lean outward from a centre, so a cluster of facets shades like the one round mass it stands for. */
ModelMesh &ModelMesh::Round(const Vec3 &centre, double share)
{
	for (ModelVertex &vertex : this->vertices) {
		Vec3 outward = Normalised(vertex.Position() - centre);
		vertex.Face(vertex.Normal() * (1.0 - share) + outward * share);
	}
	return *this;
}

ModelMesh &ModelMesh::Paint(uint32_t rgb)
{
	for (ModelVertex &vertex : this->vertices) vertex.Paint(rgb);
	return *this;
}

/* Every triangle is brightened or darkened by its own share, so a faceted surface reads facet by facet. */
ModelMesh &ModelMesh::Vary(double spread, uint32_t seed)
{
	for (size_t first = 0; first + 2 < this->indices.size(); first += 3) {
		double factor = 1.0 + spread * (SeedShare(SubSeed(seed, static_cast<uint32_t>(first)), 0, SeedDice::SHARE_BITS) * 2.0 - 1.0);
		for (size_t corner = 0; corner < 3; corner++) {
			std::array<uint8_t, 4> &colour = this->vertices[this->indices[first + corner]].colour;
			for (size_t channel = 0; channel < 3; channel++) colour[channel] = UnsignedByte(colour[channel] * factor / CHANNEL_MAX);
		}
	}
	return *this;
}

ModelMesh &ModelMesh::Trait(double share)
{
	for (ModelVertex &vertex : this->vertices) vertex.Trait(share);
	return *this;
}

ModelMesh &ModelMesh::Weld()
{
	std::map<std::array<int32_t, 3>, uint32_t> spots;
	std::vector<ModelVertex> corners;
	for (uint32_t &index : this->indices) {
		auto [found, fresh] = spots.try_emplace(Spot(this->vertices[index].Position()), static_cast<uint32_t>(corners.size()));
		if (fresh) corners.push_back(this->vertices[index]);
		index = found->second;
	}
	this->vertices = std::move(corners);
	return *this;
}
