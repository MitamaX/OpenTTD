/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file structure_mesh.h Building forms as meshes in render space, every face clad in a material the shader patterns and the windows it draws. */

#ifndef MINI_WORLD_STRUCTURE_MESH_H
#define MINI_WORLD_STRUCTURE_MESH_H

#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "../../core/enum_type.hpp"
#include "../core/camera.h"
#include "../map/building_form.h"
#include "../map/map_overlay.h"
#include "../model/model_mesh.h"
#include "smoke_plumes.h"

/* Forms count a storey an eighth of a tile; standing a sixth of a tile tall, it keeps a building in scale with the ground under it. */
inline constexpr double FORM_HEIGHT_SCALE = 1.3;

enum class StructureDetail : uint8_t { Simple, Full };

/* A facade carries its window grid and a front its entrance; roofs take snow, and decals lie on whatever they cover. */
enum class SurfaceFlag : uint8_t { Facade, Front, Roof, Decal };
using SurfaceFlags = EnumBitSet<SurfaceFlag, uint8_t>;

/* What a face is clad in: its material and window grid, how the shader treats it, and its colour before light. */
struct Coat {
	Material material = Material::Plain;
	WindowGrid grid = WindowGrid::None;
	SurfaceFlags flags{};
	uint32_t tint = MODEL_WHITE;
};

/* The model part lies in render space; the pattern runs across a face and up it in model tiles, along a facade in strips of a tile, or down a roof's slope.
 * The glass colour's last byte seeds the building's windows; the surface is the material, the window grid, the flags and the overlay layer. */
struct StructureVertex {
	ModelVertex model;
	std::array<uint8_t, 4> glass;
	float u;
	float v;
	std::array<uint8_t, 4> surface;
};

inline constexpr std::array<VertexAttribute, 6> STRUCTURE_LAYOUT = {{
	{0, 3, AttributeType::Float, offsetof(StructureVertex, model) + offsetof(ModelVertex, x)},
	{1, 4, AttributeType::NormalisedByte, offsetof(StructureVertex, model) + offsetof(ModelVertex, normal)},
	{2, 4, AttributeType::NormalisedUnsignedByte, offsetof(StructureVertex, model) + offsetof(ModelVertex, colour)},
	{3, 4, AttributeType::NormalisedUnsignedByte, offsetof(StructureVertex, glass)},
	{4, 2, AttributeType::Float, offsetof(StructureVertex, u)},
	{5, 4, AttributeType::NormalisedUnsignedByte, offsetof(StructureVertex, surface)},
}};

/* The part of a building that answers a click: a solid's box in render space and the tiles its form stands on. */
struct StructurePick {
	Vec3 low;
	Vec3 high;
	TileSpan tiles;
};

struct StructureMesh : TriangleList<StructureVertex> {
	void Polygon(std::span<const StructureVertex> corners);
};

/* Everything forms stand as: their mesh, the boxes clicks meet and the stacks smoke rises from. */
struct StructureParts {
	StructureMesh mesh;
	std::vector<StructurePick> picks;
	std::vector<SmokeVent> vents;
};

/* Everything one form stands as, shared by the solids and the fixtures dressing them. */
class FormStyle {
public:
	FormStyle(const BuildingForm &form, MiniLayer layer);

	StructureVertex Vertex(const ModelVertex &model, double u, double v, const Coat &coat, uint32_t glass) const;
	double Floor() const { return this->floor; }
	uint32_t Seed() const { return this->seed; }

private:
	double floor;
	uint8_t layer;
	uint32_t seed;
};

/* Form heights stand scaled up from model tiles, which tilts a face's normal toward upright. */
Vec3 RenderNormalOf(const Vec3 &model_normal);
void BuildStructure(const BuildingForm &form, StructureDetail detail, MiniLayer layer, StructureParts &parts);

#endif /* MINI_WORLD_STRUCTURE_MESH_H */
