/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file track_models.h Railway track as 3D pieces: ballast, sleepers and rails, the guideways of monorail and maglev, rails set into a road, and catenary. */

#ifndef MINI_WORLD_TRACK_MODELS_H
#define MINI_WORLD_TRACK_MODELS_H

#include <functional>

#include "../map/network_style.h"
#include "tile_track.h"
#include "way_shapes.h"

inline constexpr double RAIL_HALF = 0.011;
inline constexpr double ARM_LOW = 0.41;

double RideHeight(RailLook look);
void LayTrack(ModelMesh &mesh, const TileTrack &track, const Footing &footing);
using MastBan = std::function<bool(const MapVector &foot)>;

/* No mast stands with its foot where the ban holds, as by a tunnel's mouth, into which the wires run on into the dark. */
void LayCatenary(ModelMesh &mesh, const TileTrack &track, const Footing &footing, const MastBan &banned);

#endif /* MINI_WORLD_TRACK_MODELS_H */
