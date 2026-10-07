/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file way_course.h The courses rails and roads are eased to along their runs: rails curve through the tiles they cross instead of turning at every tile side,
 * and every run climbs at a steady grade on low banks instead of stepping with the tiles under it. */

#ifndef MINI_MAP_WAY_COURSE_H
#define MINI_MAP_WAY_COURSE_H

#include <array>
#include <optional>
#include <utility>

#include "../../track_type.h"
#include "../core/camera.h"
#include "way_line.h"

/* Each rail piece's two ends on its tile, at the middles of the sides it joins, in the game's track order. */
inline constexpr std::array<std::pair<MapVector, MapVector>, TRACK_END> TRACK_ENDS = {{
	{{0.0, 0.5}, {1.0, 0.5}},
	{{0.5, 0.0}, {0.5, 1.0}},
	{{0.0, 0.5}, {0.5, 0.0}},
	{{1.0, 0.5}, {0.5, 1.0}},
	{{0.5, 0.0}, {1.0, 0.5}},
	{{0.0, 0.5}, {0.5, 1.0}},
}};

/* How many joints along a run a change of the ways can move its ease, which is also how many tiles away it can reach. */
inline constexpr int WAY_EASE_REACH = 16;

struct Piece;

/* One piece of way across a tile, from one side to another along its eased line, and the level it is eased to along its length.
 * Its ends meet the pieces either side at one point, heading and level; an end at a junction, station, crossing, depot, bridge, tunnel or town street
 * keeps to the middle of the tile side and the ground there, heading on along the way beyond. */
class WayCourse {
public:
	/* The course of a rail piece, or of a road running in at two ends, unless the tile's ways keep to its ground. */
	static std::optional<WayCourse> OfTrack(int tx, int ty, Track track);
	static std::optional<WayCourse> OfRoad(int tx, int ty);

	const Bend &Line() const { return this->line; }
	MapVector Centre(double share) const { return this->line.Centre(share); }
	/* The way the course heads at its first end and at its last, both along it. */
	MapVector Before() const { return this->line.Along(0.0); }
	MapVector After() const { return this->line.Along(1.0); }
	/* Which way the course runs on from one of its ends, away from the tile beyond that end. */
	MapVector OnwardFrom(const MapVector &end) const;
	double Length() const;
	double ShareAt(const MapVector &point) const { return this->line.ShareOf(point); }
	/* How far along its run from the run's first end the course's first and last ends lie, unless it holds at both. */
	const std::optional<std::pair<double, double>> &Distances() const { return this->distances; }
	double Level(double share) const;
	/* How far the eased level stands above the tile's ground under the middle line. */
	double Lift(double share) const;
	bool Raised() const;

private:
	/* How far along its run an end lies, and whether that grows or shrinks along the course. */
	struct RunMark {
		double distance;
		double growth;
	};

	/* How a course meets the run at one end: where and at what level, its heading along the course, and where the run eases through it, its climb along the course and its mark along the run. */
	struct CourseEnd {
		MapVector at;
		double level;
		std::optional<double> slope;
		MapVector heading;
		std::optional<RunMark> mark;
	};

	WayCourse(int tx, int ty, const Piece &piece, bool road);
	CourseEnd EndAt(const MapVector &point, bool leaving) const;
	MapVector HeldHeading(const MapVector &point, bool leaving) const;

	int tx;
	int ty;
	MapVector from;
	MapVector to;
	bool road;
	Bend line;
	double start_level = 0.0;
	double end_level = 0.0;
	double start_slope = 0.0;
	double end_slope = 0.0;
	std::optional<std::pair<double, double>> distances;
};

/* A rail piece as it is drawn: along its course's eased line where it eases, or else along the game's straight piece leaning at either end halfway toward
 * the track beyond, so pieces meet without a seam; and whether either end stands open, no track carrying on beyond it at its level. */
struct DrawnTrack {
	int tx;
	int ty;
	MapVector from;
	MapVector to;
	Bend line;
	std::optional<WayCourse> course;
	bool open_first;
	bool open_last;

	static DrawnTrack Of(int tx, int ty, Track track);
	/* Where a point on the game's own piece is drawn: as far along the line and as far to its side, at the course's level or on the ground. */
	WorldPoint Placed(const MapVector &point) const;

private:
	static DrawnTrack Build(int tx, int ty, Track track);

	friend class TrackBook;
};

/* How far the eased rail line slides along a tile's west side and along its north side, from the middle of each. */
std::pair<double, double> SideSlides(int tx, int ty);

/* Where a train or a road vehicle at a point of the map is drawn: a train on the line of the piece it is on, either at its way's eased level or on the ground. */
WorldPoint TrackPoint(double x, double y);
WorldPoint RoadPoint(double x, double y);

#endif /* MINI_MAP_WAY_COURSE_H */
