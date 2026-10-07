/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file way_profile.h The levels rails and roads are eased to along their runs, so a run climbs at a steady grade on low banks instead of stepping with the tiles under it. */

#ifndef MINI_MAP_WAY_PROFILE_H
#define MINI_MAP_WAY_PROFILE_H

#include <array>
#include <optional>
#include <utility>

#include "../../track_type.h"
#include "../core/camera.h"

/* Each rail piece's two ends on its tile, at the middles of the sides it joins, in the game's track order. */
inline constexpr std::array<std::pair<MapVector, MapVector>, TRACK_END> TRACK_ENDS = {{
	{{0.0, 0.5}, {1.0, 0.5}},
	{{0.5, 0.0}, {0.5, 1.0}},
	{{0.0, 0.5}, {0.5, 0.0}},
	{{1.0, 0.5}, {0.5, 1.0}},
	{{0.5, 0.0}, {1.0, 0.5}},
	{{0.0, 0.5}, {0.5, 1.0}},
}};

/* How many joints a run's ease reaches along it either way, which is also how many tiles away a change of the ways can move it. */
inline constexpr int WAY_EASE_REACH = 8;

struct Piece;

/* One piece of way across a tile, from the middle of one side to the middle of another, and the level it is eased to along its length.
 * Its ends meet the pieces either side at one level; an end at a junction, station, crossing, depot, bridge, tunnel or town street keeps to the ground there. */
class WayCourse {
public:
	/* The course of a rail piece, or of a road running in at two ends, unless the tile's ways keep to its ground. */
	static std::optional<WayCourse> OfTrack(int tx, int ty, Track track);
	static std::optional<WayCourse> OfRoad(int tx, int ty);

	const MapVector &From() const { return this->from; }
	const MapVector &To() const { return this->to; }
	/* The way the run comes into the first end and goes on from the last; nothing where an end keeps to the ground. */
	const MapVector &Before() const { return this->before; }
	const MapVector &After() const { return this->after; }

	/* A road bend's middle line rounds the tile corner its two ends share; every other course runs straight. */
	MapVector Centre(double share) const;
	double Length() const;
	/* How far along from the first end a point of the tile lies, the same all across the way's width. */
	double ShareAt(const MapVector &point) const;
	double Level(double share) const;
	/* How far the eased level stands above the tile's ground under the middle line. */
	double Lift(double share) const;
	bool Raised() const;

private:
	/* How a course meets the run at one end: its level there, its climb along the course where the run eases through it, and the way the run goes on beyond it. */
	struct CourseEnd {
		double level;
		std::optional<double> slope;
		MapVector run;
	};

	WayCourse(int tx, int ty, const Piece &piece, bool road);
	CourseEnd EndAt(const MapVector &point, bool leaving) const;
	bool Curved() const;

	friend double WayLevel(double x, double y);

	int tx;
	int ty;
	MapVector from;
	MapVector to;
	MapVector before{};
	MapVector after{};
	bool road;
	double start_level = 0.0;
	double end_level = 0.0;
	double start_slope = 0.0;
	double end_slope = 0.0;
};

/* The level a vehicle at a point of the map rides at: the eased level of the course it is on, or the ground. */
double WayLevel(double x, double y);

#endif /* MINI_MAP_WAY_PROFILE_H */
