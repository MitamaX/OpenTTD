/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file track_pieces.h The game's pieces of rail track as the mini map follows them, each running across its tile between the middles of two of its sides. */

#ifndef MINI_MAP_TRACK_PIECES_H
#define MINI_MAP_TRACK_PIECES_H

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

#include "../../track_func.h"
#include "../../track_type.h"
#include "../core/camera.h"
#include "tile_shapes.h"

/* Each rail piece's two ends on its tile, at the middles of the sides it joins, in the game's track order. */
inline constexpr std::array<std::pair<MapVector, MapVector>, TRACK_END> TRACK_ENDS = {{
	{{0.0, 0.5}, {1.0, 0.5}},
	{{0.5, 0.0}, {0.5, 1.0}},
	{{0.0, 0.5}, {0.5, 0.0}},
	{{1.0, 0.5}, {0.5, 1.0}},
	{{0.5, 0.0}, {1.0, 0.5}},
	{{0.0, 0.5}, {0.5, 1.0}},
}};

using TileCoord = std::pair<int, int>;

/* The middle of a tile's side, named on the half tile lattice, so the tiles either side name it alike. */
struct Joint {
	int hx;
	int hy;

	static Joint At(const MapVector &point) { return {static_cast<int>(std::lround(point.x * 2.0)), static_cast<int>(std::lround(point.y * 2.0))}; }
	MapVector Point() const { return {this->hx * HALF_TILE, this->hy * HALF_TILE}; }
	bool AlongX() const { return (this->hx & 1) != 0; }
	/* Which way along its tile side a joint slides. */
	MapVector Side() const { return this->AlongX() ? MapVector{1.0, 0.0} : MapVector{0.0, 1.0}; }
	uint64_t Key() const { return static_cast<uint64_t>(static_cast<uint32_t>(this->hx)) << 32 | static_cast<uint32_t>(this->hy); }
	bool operator==(const Joint &) const = default;

	/* The two tiles whose sides meet here, the one toward lower map coordinates first. */
	std::array<TileCoord, 2> Tiles() const
	{
		if (this->AlongX()) return {{{(this->hx - 1) / 2, this->hy / 2 - 1}, {(this->hx - 1) / 2, this->hy / 2}}};
		return {{{this->hx / 2 - 1, (this->hy - 1) / 2}, {this->hx / 2, (this->hy - 1) / 2}}};
	}

	bool Joins(const TileCoord &tile) const
	{
		auto [low, high] = this->Tiles();
		return tile == low || tile == high;
	}

	TileCoord Beyond(const TileCoord &tile) const
	{
		auto [low, high] = this->Tiles();
		return tile == low ? high : low;
	}

	/* The way across the side from its lower tile into its higher. */
	MapVector Upward() const { return this->AlongX() ? MapVector{0.0, 1.0} : MapVector{1.0, 0.0}; }

	/* The way across the side from one of its tiles into the other. */
	MapVector Across(int tx, int ty) const
	{
		return this->Upward() * (this->Tiles()[0] == TileCoord{tx, ty} ? 1.0 : -1.0);
	}
};

struct Piece {
	Joint from;
	Joint to;

	Joint Other(const Joint &end) const { return end == this->from ? this->to : this->from; }
	bool Meets(const Joint &joint) const { return this->from == joint || this->to == joint; }
	MapVector From(const Joint &end) const { return Unit(this->Other(end).Point() - end.Point()); }
	double Span() const { return std::hypot(this->to.hx - this->from.hx, this->to.hy - this->from.hy) * HALF_TILE; }
};

TileCoord TileOf(const MapVector &point);
double ShareAlong(const MapVector &point, const MapVector &from, const MapVector &to);
Piece PieceOf(int tx, int ty, Track track);
/* The rail pieces a tile carries as far as the track beside it goes, a rail bridge's head among them. */
TrackBits TrackBitsOf(int tx, int ty);

/* The piece of a tile's track rated highest among some of its pieces, passing over any the rating gives no value. */
template <typename Rating>
std::optional<Track> BestTrackAt(const TileCoord &tile, Rating rating, TrackBits among = TRACK_BIT_ALL)
{
	std::optional<Track> best;
	if (!OnMap(tile.first, tile.second)) return best;
	double best_rating = 0.0;
	for (Track track : SetTrackBitIterator(TrackBitsOf(tile.first, tile.second) & among)) {
		std::optional<double> piece_rating = rating(PieceOf(tile.first, tile.second, track));
		if (!piece_rating.has_value() || (best.has_value() && *piece_rating <= best_rating)) continue;
		best = track;
		best_rating = *piece_rating;
	}
	return best;
}

template <typename Rating>
std::optional<Piece> BestPieceAt(const TileCoord &tile, const Joint &joint, Rating rating, TrackBits among = TRACK_BIT_ALL)
{
	std::optional<Track> best = BestTrackAt(tile, [&](const Piece &piece) -> std::optional<double> {
		if (!piece.Meets(joint)) return std::nullopt;
		return rating(piece);
	}, among);
	if (!best.has_value()) return std::nullopt;
	return PieceOf(tile.first, tile.second, *best);
}

std::optional<Piece> StraightestAt(const TileCoord &tile, const Joint &joint, const MapVector &heading, TrackBits among = TRACK_BIT_ALL);
std::optional<Track> TrackUnder(const TileCoord &tile, const MapVector &point, TrackBits among = TRACK_BIT_ALL);
int StraightOnEnds(int tx, int ty, Track track);

#endif /* MINI_MAP_TRACK_PIECES_H */
