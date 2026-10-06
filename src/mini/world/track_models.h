/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file track_models.h Railway track as 3D pieces: ballast, sleepers and rails, the guideways of monorail and maglev, rails set into a road, and catenary. */

#ifndef MINI_WORLD_TRACK_MODELS_H
#define MINI_WORLD_TRACK_MODELS_H

#include "../../track_type.h"
#include "../map/world_tiles.h"
#include "way_shapes.h"

/* The top of the rails above whatever the track is laid on, in tiles. */
inline constexpr double RAIL_TOP = 0.068;

/* A tile's track and how it is laid. Track on the ground meets its neighbours' pieces in mitres and ends in a sloped face where none carries on;
 * track on a bridge just runs from edge to edge. */
struct TrackSite {
	int tx;
	int ty;
	TrackBits bits;
	RailLook look;
	bool wired;
	bool grounded;
	WayDetail detail;
};

double RideHeight(RailLook look);
void LayTrack(ModelMesh &mesh, const TrackSite &site, const Footing &footing);
void LayCrossingRails(ModelMesh &mesh, const TrackSite &site, const Footing &footing);
void LayCatenary(ModelMesh &mesh, const TrackSite &site, const Footing &footing);

#endif /* MINI_WORLD_TRACK_MODELS_H */
