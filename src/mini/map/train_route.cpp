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

/* A train takes in a tile the piece picked from among those it is known to take there, or else from among all. */
template <typename Pick>
static auto PickAmongTaken(const TileCoord &tile, const TrackTaken &taken, Pick pick)
{
	using Picked = decltype(pick(TRACK_BIT_ALL));
	if (!OnMap(tile.first, tile.second)) return Picked{};
	Picked known = pick(taken(TileXY(tile.first, tile.second)));
	return known.has_value() ? known : pick(TRACK_BIT_ALL);
}

static std::optional<Track> StandingOn(const TileCoord &tile, const MapVector &point, const TrackTaken &taken)
{
	return PickAmongTaken(tile, taken, [&](TrackBits among) { return TrackUnder(tile, point, among); });
}

static std::optional<Piece> StraightestOnwardAt(const TileCoord &tile, const Joint &joint, const MapVector &heading, const TrackTaken &taken)
{
	return PickAmongTaken(tile, taken, [&](TrackBits among) { return StraightestAt(tile, joint, heading, among); });
}

static double Nearness(const Joint &joint, const MapVector &bound)
{
	return -Length(bound - joint.Point());
}

static std::optional<Piece> NearestOnwardAt(const TileCoord &tile, const Joint &joint, const MapVector &bound, const TrackTaken &taken)
{
	auto leading = [&](const Piece &onward) { return Nearness(onward.Other(joint), bound); };
	return PickAmongTaken(tile, taken, [&](TrackBits among) { return BestPieceAt(tile, joint, leading, among); });
}

MapVector TrainAhead(const MapVector &from, MapVector heading, double reach, const TrackTaken &taken)
{
	TileCoord tile = TileOf(from);
	std::optional<Track> under = StandingOn(tile, from, taken);
	if (!under.has_value()) return from + heading * reach;

	Piece piece = PieceOf(tile.first, tile.second, *under);
	Joint exit = Dot(piece.to.Point() - piece.from.Point(), heading) >= 0.0 ? piece.to : piece.from;
	MapVector at = from;
	for (;;) {
		heading = piece.From(exit) * -1.0;
		double left = std::max(Dot(exit.Point() - at, heading), 0.0);
		if (reach <= left) break;
		reach -= left;
		at = exit.Point();
		tile = exit.Beyond(tile);
		std::optional<Piece> onward = StraightestOnwardAt(tile, exit, heading, taken);
		if (!onward.has_value()) break;
		piece = *onward;
		exit = piece.Other(exit);
	}
	return at + heading * reach;
}

TrackRoute::TrackRoute(const MapVector &from, const MapVector &to, const TrackTaken &taken)
{
	this->points[this->count++] = from;
	if (!this->Cross(from, to, taken)) this->count = 1;
	this->points[this->count++] = to;
}

/* A train leaves each piece by the end nearer where it is bound. */
bool TrackRoute::Cross(const MapVector &from, const MapVector &to, const TrackTaken &taken)
{
	TileCoord tile = TileOf(from);
	TileCoord bound = TileOf(to);
	if (tile == bound) return true;
	std::optional<Track> under = StandingOn(tile, from, taken);
	if (!under.has_value()) return false;

	Piece piece = PieceOf(tile.first, tile.second, *under);
	Joint exit = Nearness(piece.from, to) >= Nearness(piece.to, to) ? piece.from : piece.to;
	for (size_t side = 0; side < MOST_SIDES; side++) {
		this->points[this->count++] = exit.Point();
		tile = exit.Beyond(tile);
		if (tile == bound) return true;
		std::optional<Piece> next = NearestOnwardAt(tile, exit, to, taken);
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
