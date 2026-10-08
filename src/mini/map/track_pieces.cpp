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

TileCoord TileOf(const MapVector &point)
{
	return {static_cast<int>(std::floor(point.x)), static_cast<int>(std::floor(point.y))};
}

double ShareAlong(const MapVector &point, const MapVector &from, const MapVector &to)
{
	MapVector along = to - from;
	return std::clamp(Dot(point - from, along) / Dot(along, along), 0.0, 1.0);
}

static double SegmentDistance(const MapVector &point, const MapVector &from, const MapVector &to)
{
	return Length(from + (to - from) * ShareAlong(point, from, to) - point);
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
	return BestPieceAt(tile, joint, [&](const Piece &piece) { return Dot(Unit(piece.Other(joint).Point() - joint.Point()), heading); }, among);
}

std::optional<Track> TrackUnder(const TileCoord &tile, const MapVector &point, TrackBits among)
{
	return BestTrackAt(tile, [&](const Piece &piece) -> std::optional<double> { return -SegmentDistance(point, piece.from.Point(), piece.to.Point()); }, among);
}
