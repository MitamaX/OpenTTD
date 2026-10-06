/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file network_mesh.h Everything the ways on a block of tiles lay down: a mesh for each overlay layer, and the signals standing beside the track. */

#ifndef MINI_WORLD_NETWORK_MESH_H
#define MINI_WORLD_NETWORK_MESH_H

#include <array>
#include <vector>

#include "../../core/enum_type.hpp"
#include "../../tile_type.h"
#include "../../track_type.h"
#include "../map/map_overlay.h"
#include "way_shapes.h"

inline constexpr size_t NETWORK_LAYERS = to_underlying(MiniLayer::Road) + 1;

/* A signal beside the track: where it stands in render space, the bearing it faces in radians, and the tile and trackdir its state is read from. */
struct SignalSpot {
	Vec3 at;
	double facing;
	TileIndex tile;
	Trackdir trackdir;
};

struct NetworkMeshes {
	std::array<ModelMesh, NETWORK_LAYERS> layers;
	std::vector<SignalSpot> signals;

	ModelMesh &Layer(MiniLayer layer) { return this->layers[to_underlying(layer)]; }
};

NetworkMeshes BuildNetwork(const TileSpan &tiles, WayDetail detail);

#endif /* MINI_WORLD_NETWORK_MESH_H */
