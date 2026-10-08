/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file train_route.h A train's way over the game's map along its pieces of track: between where it stood at two ticks, and on ahead of a point or back behind it. */

#ifndef MINI_MAP_TRAIN_ROUTE_H
#define MINI_MAP_TRAIN_ROUTE_H

#include <array>
#include <functional>
#include <span>

#include "../../tile_type.h"
#include "../../track_type.h"
#include "../core/camera.h"

/* A train's way over the game's map from one point to another a few tile sides on, through the middles of the tile sides its track crosses, or straight where none leads there. */
class TrackRoute {
public:
	TrackRoute(const MapVector &from, const MapVector &to);

	/* The middles of the tile sides crossed, in order. */
	std::span<const MapVector> Sides() const;
	/* The point a share of the way along. */
	MapVector At(double share) const;

private:
	static constexpr size_t MOST_SIDES = 4;

	bool Cross(const MapVector &from, const MapVector &to);

	std::array<MapVector, MOST_SIDES + 2> points{};
	size_t count = 0;
};

/* Which of a tile's pieces of track a train is known to take. */
using TrackTaken = std::function<TrackBits(TileIndex tile)>;
/* Where a train's path over the game's map runs to a reach on from a point the way it heads, along the game's pieces of track. */
MapVector TrainAhead(const MapVector &from, MapVector heading, double reach, const TrackTaken &taken);

#endif /* MINI_MAP_TRAIN_ROUTE_H */
