/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tile_track.h A tile's rail pieces taken together, ranked from the tile's main line down: where each is laid, how high, and where its sleepers lie. */

#ifndef MINI_WORLD_TILE_TRACK_H
#define MINI_WORLD_TILE_TRACK_H

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "../../track_type.h"
#include "../map/way_course.h"
#include "../map/world_tiles.h"
#include "way_shapes.h"

inline constexpr double GAUGE_HALF = 0.105;
inline constexpr double SLEEPER_HALF_WIDTH = 0.02;
inline constexpr double SLEEPER_HALF_LENGTH = 0.17;
inline constexpr double SLEEPER_FOOT = 0.026;
inline constexpr double SLEEPER_TOP = 0.047;
inline constexpr size_t MAIN_LINE = 0;

/* A tile's track and how it is laid. */
struct TrackSite {
	int tx;
	int ty;
	TrackBits bits;
	RailLook look;
	bool wired;
	bool grounded;
	WayDetail detail;
};

/* A piece of track laid along the line it is drawn on. */
class TrackPiece {
public:
	TrackPiece(int tx, int ty, Track track, bool grounded);

	Track Id() const { return this->track; }
	bool Eased() const { return this->drawn.course.has_value(); }
	const WayLine &Line() const { return this->drawn.line; }
	ModelMesh Body(Section section, bool capped) const;

	template <size_t N>
	ModelMesh Pair(const std::array<SectionPoint, N> &section, double gap_half) const
	{
		ModelMesh pair;
		for (double side : {-1.0, 1.0}) pair.Append(Laid(this->Line(), Shifted(section, side * gap_half), this->rows));
		return pair;
	}

	std::optional<std::pair<double, double>> Distances() const;
	Footing FootingOn(const Footing &ground) const;

private:
	Track track;
	DrawnTrack drawn;
	int rows;
};

struct Sleeper {
	double share;
	double left;
	double right;
	uint32_t seed;

	double Middle() const { return (this->left + this->right) / 2.0; }
	double HalfLength() const { return (this->right - this->left) / 2.0; }
};

struct Frog {
	double main_share;
	double branch_share;
	MapVector point;
	MapVector heading;
};

struct Turnout {
	size_t main;
	size_t branch;
	double main_toe;
	double branch_toe;
	double side;
	double branch_side;
	std::optional<Frog> frog;

	static constexpr double Inward(double toe) { return toe == 0.0 ? 1.0 : -1.0; }
};

class TileTrack {
public:
	explicit TileTrack(const TrackSite &site);

	const TrackSite &Site() const { return this->site; }
	const std::vector<TrackPiece> &Pieces() const { return this->pieces; }
	const std::vector<Turnout> &Turnouts() const { return this->turnouts; }
	double Lift(const TrackPiece &piece) const;
	std::optional<uint32_t> BallastSeed(const TrackPiece &piece) const;
	bool Branch(size_t rank) const;
	/* Sleepers lie on equal slots along the piece, so the same pitch runs on across every seam. */
	std::vector<std::vector<Sleeper>> Sleepers() const;

private:
	bool Shared() const;
	std::vector<Turnout> FindTurnouts() const;
	void ReachUnderBranches(size_t rank, Sleeper &sleeper) const;

	TrackSite site;
	std::vector<TrackPiece> pieces;
	std::vector<Turnout> turnouts;
};

#endif /* MINI_WORLD_TILE_TRACK_H */
