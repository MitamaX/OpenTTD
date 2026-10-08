/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file train_route.cpp A train's way over the game's map along its pieces of track: between where it stood at two ticks, and on ahead of a point or back behind it. */

#include "../../stdafx.h"
#include "train_route.h"

#include <algorithm>

#include "../../map_func.h"
#include "tile_shapes.h"
#include "track_pieces.h"

#include "../../safeguards.h"

/* A train runs on into a tile along the straightest piece it is known to take there, or else along the straightest of all. */
static std::optional<Piece> OnwardAt(const TileCoord &tile, const Joint &joint, const MapVector &heading, const TrackTaken &taken)
{
	if (!OnMap(tile.first, tile.second)) return std::nullopt;
	std::optional<Piece> known = StraightestAt(tile, joint, heading, taken(TileXY(tile.first, tile.second)));
	return known.has_value() ? known : StraightestAt(tile, joint, heading);
}

MapVector TrainAhead(const MapVector &from, MapVector heading, double reach, const TrackTaken &taken)
{
	TileCoord tile = TileOf(from);
	std::optional<Track> under = TrackUnder(tile, from);
	if (!under.has_value()) return from + heading * reach;

	Piece piece = PieceOf(tile.first, tile.second, *under);
	Joint exit = Dot(piece.to.Point() - piece.from.Point(), heading) >= 0.0 ? piece.to : piece.from;
	MapVector at = from;
	for (;;) {
		heading = Unit(exit.Point() - piece.Other(exit).Point());
		double left = std::max(Dot(exit.Point() - at, heading), 0.0);
		if (reach <= left) break;
		reach -= left;
		at = exit.Point();
		tile = exit.Beyond(tile);
		std::optional<Piece> onward = OnwardAt(tile, exit, heading, taken);
		if (!onward.has_value()) break;
		piece = *onward;
		exit = piece.Other(exit);
	}
	return at + heading * reach;
}

TrackRoute::TrackRoute(const MapVector &from, const MapVector &to)
{
	this->points[this->count++] = from;
	if (!this->Cross(from, to)) this->count = 1;
	this->points[this->count++] = to;
}

/* From the piece a train starts on it leaves by the end nearer where it is bound, and on through each tile by the piece leading nearest there. */
bool TrackRoute::Cross(const MapVector &from, const MapVector &to)
{
	TileCoord tile = TileOf(from);
	TileCoord bound = TileOf(to);
	if (tile == bound) return true;
	std::optional<Track> under = TrackUnder(tile, from);
	if (!under.has_value()) return false;

	auto nearness = [&to](const Joint &joint) { return -Length(to - joint.Point()); };
	Piece piece = PieceOf(tile.first, tile.second, *under);
	Joint exit = nearness(piece.from) >= nearness(piece.to) ? piece.from : piece.to;
	for (size_t side = 0; side < MOST_SIDES; side++) {
		this->points[this->count++] = exit.Point();
		tile = exit.Beyond(tile);
		if (tile == bound) return true;
		std::optional<Piece> next = BestPieceAt(tile, exit, [&](const Piece &onward) { return nearness(onward.Other(exit)); });
		if (!next.has_value()) return false;
		exit = next->Other(exit);
	}
	return false;
}

std::span<const MapVector> TrackRoute::Sides() const
{
	return std::span(this->points).subspan(1, this->count - 2);
}

MapVector TrackRoute::At(double share) const
{
	std::span<const MapVector> path = std::span(this->points).first(this->count);
	double length = 0.0;
	for (size_t point = 1; point < path.size(); point++) length += Length(path[point] - path[point - 1]);
	ArcPlace place = PlaceAlong(path, share * length);
	return path[place.from] + (path[place.to] - path[place.from]) * place.share;
}
