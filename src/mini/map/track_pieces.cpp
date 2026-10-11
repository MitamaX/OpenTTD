/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file track_pieces.cpp The game's pieces of rail track as the mini map follows them, each running across its tile between the middles of two of its sides. */

#include "../../stdafx.h"
#include "track_pieces.h"

#include <algorithm>

#include "../../bridge_map.h"
#include "../../map_func.h"
#include "../../tunnelbridge_map.h"
#include "world_tiles.h"

#include "../../safeguards.h"

static constexpr double STRAIGHT_ON_COSINE = 0.99;

TileCoord TileOf(const MapVector &point)
{
	return {static_cast<int>(std::floor(point.x)), static_cast<int>(std::floor(point.y))};
}

Piece PieceOf(int tx, int ty, Track track)
{
	MapVector origin = {static_cast<double>(tx), static_cast<double>(ty)};
	auto [from, to] = TRACK_ENDS[track];
	return {Joint::At(origin + from), Joint::At(origin + to)};
}

TrackBits TrackBitsOf(int tx, int ty)
{
	TileIndex tile = TileXY(tx, ty);
	if (IsBridgeTile(tile) && GetTunnelBridgeTransportType(tile) == TRANSPORT_RAIL) return AxisToTrackBits(DiagDirToAxis(GetTunnelBridgeDirection(tile)));
	return static_cast<TrackBits>(_world_tiles.NetworkAt(tile).track);
}

std::optional<Piece> StraightestAt(const TileCoord &tile, const Joint &joint, const MapVector &heading, TrackBits among)
{
	return BestPieceAt(tile, joint, [&](const Piece &piece) { return Dot(piece.From(joint), heading); }, among);
}

std::optional<Track> TrackUnder(const TileCoord &tile, const MapVector &point, TrackBits among)
{
	return BestTrackAt(tile, [&](const Piece &piece) -> std::optional<double> { return -SegmentDistance(point, piece.from.Point(), piece.to.Point()); }, among);
}

int StraightOnEnds(int tx, int ty, Track track)
{
	Piece piece = PieceOf(tx, ty, track);
	int straight = 0;
	for (const Joint &end : {piece.from, piece.to}) {
		MapVector outward = piece.From(end) * -1.0;
		std::optional<Piece> beyond = StraightestAt(end.Beyond({tx, ty}), end, outward);
		if (beyond.has_value() && Dot(beyond->From(end), outward) >= STRAIGHT_ON_COSINE) straight++;
	}
	return straight;
}
