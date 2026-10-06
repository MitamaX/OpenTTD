/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tree_models.h The low poly trees of every kind, in a few shapes each and at each level of detail, built once in code. */

#ifndef MINI_WORLD_TREE_MODELS_H
#define MINI_WORLD_TREE_MODELS_H

#include <array>
#include <vector>

#include "../map/world_tiles.h"
#include "../model/model_mesh.h"

/** How finely a tree is drawn, from near to far; past the last the ground's forest tint stands in for it. */
enum class TreeDetail : uint8_t {
	Full,
	Simple,
	Crude,
	End,
};

inline constexpr size_t TREE_DETAILS = static_cast<size_t>(TreeDetail::End);
inline constexpr size_t TREE_SHAPES = 3;
inline constexpr size_t TREE_KINDS = static_cast<size_t>(TreeKind::End);

/* The tile pixels below which each detail hands over to the next, crossfading over a band either side of it. */
inline constexpr std::array<double, TREE_DETAILS> TREE_DETAIL_FLOORS = {40.0, 12.0, 3.0};
inline constexpr double TREE_CROSSFADE_OCTAVES = 0.25;

/* How big a tree stands at each age, as a share of its model. */
inline constexpr std::array<double, static_cast<size_t>(TreeAge::End)> TREE_AGE_SCALES = {0.45, 0.85, 1.3, 1.25};

/* The tallest a tree stands above its ground, in tiles, and the height its sway is measured against. */
inline constexpr double TREE_TALLEST = 0.9;
inline constexpr double TREE_SWAY_HEIGHT = 0.5;

/* The colour a canopy of each kind shows from afar, as the ground's forest tint paints it. */
inline constexpr std::array<uint32_t, TREE_KINDS> TREE_CANOPY_TONES = {
	0x557F37, 0x3A6837, 0x3A752E, 0x5E923C, 0x5E8A4A, 0xE07EA8,
};

/* A tree's shape: one of its kind's few. Meshes are listed shape by shape, each at every detail, then again welded for casting shadows. */
struct TreeShape {
	TreeKind kind;
	uint8_t variant;

	size_t Index() const { return static_cast<size_t>(this->kind) * TREE_SHAPES + this->variant; }
};

inline constexpr size_t TREE_SHAPE_COUNT = TREE_KINDS * TREE_SHAPES;
inline constexpr size_t TREE_DRAWN_MESHES = TREE_SHAPE_COUNT * TREE_DETAILS;

constexpr size_t TreeMeshIndex(size_t shape, TreeDetail detail)
{
	return shape * TREE_DETAILS + static_cast<size_t>(detail);
}

constexpr TreeDetail TreeDetailOf(size_t mesh)
{
	return static_cast<TreeDetail>(mesh % TREE_DETAILS);
}

/* The mesh a tree casts its shadow with: welded, and one detail coarser than it is drawn with, as a shadow's soft edge hides the difference. */
constexpr size_t TreeCasterMesh(size_t mesh)
{
	return TREE_DRAWN_MESHES + (TreeDetailOf(mesh) == TreeDetail::Crude ? mesh : mesh + 1);
}

std::vector<ModelMesh> BuildTreeModels();

#endif /* MINI_WORLD_TREE_MODELS_H */
