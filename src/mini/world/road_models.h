/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file road_models.h Roads as 3D surfaces: asphalt with rounded junctions and bends, kerbs and pavements in town, lane markings, tram tracks and one way arrows. */

#ifndef MINI_WORLD_ROAD_MODELS_H
#define MINI_WORLD_ROAD_MODELS_H

#include "../../road_type.h"
#include "way_shapes.h"

/* The top of the asphalt above whatever the road is laid on, in tiles. */
inline constexpr double ASPHALT_TOP = 0.012;
inline constexpr double PAVEMENT_TOP = 0.036;

/* A tile's road and tram ends and how they are laid; a road over a rail crossing carries no markings across the rails, and only a street of its own has room for cars at its kerbs. */
struct RoadSite {
	int tx;
	int ty;
	RoadBits road;
	RoadBits tram;
	bool kerbed;
	bool crossing;
	DisallowedRoadDirections one_way;
	WayDetail detail;
	bool kerbside = false;
};

void LayRoad(ModelMesh &mesh, const RoadSite &site, const Footing &footing);

#endif /* MINI_WORLD_ROAD_MODELS_H */
