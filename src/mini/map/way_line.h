/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file way_line.h The middle lines ways are laid along over the map, straight or bending from one heading to another. */

#ifndef MINI_MAP_WAY_LINE_H
#define MINI_MAP_WAY_LINE_H

#include "../core/camera.h"

inline double Hermite(double from, double from_slope, double to, double to_slope, double along)
{
	double t2 = along * along;
	double t3 = t2 * along;
	return (2.0 * t3 - 3.0 * t2 + 1.0) * from + (t3 - 2.0 * t2 + along) * from_slope + (3.0 * t2 - 2.0 * t3) * to + (t3 - t2) * to_slope;
}

/* A way's middle line, run from its first end at share 0 to its last at share 1. */
class WayLine {
public:
	/* The point a share of the way along and a lateral offset to the right of the line. */
	virtual MapVector At(double share, double lateral) const = 0;
	/* Which way the line runs a share of the way along. */
	virtual MapVector Along(double share) const = 0;
	virtual double Span() const = 0;
	/* How far along a point lies, the same all across the line's width. */
	virtual double ShareOf(const MapVector &point) const = 0;

protected:
	~WayLine() = default;
};

/* A line leaving one point on one heading and reaching another on another, curving between them as a circle would where the turn is even.
 * Bends meeting at a point on one heading join without a seam, as each end is square to its heading. */
class Bend final : public WayLine {
public:
	Bend(const MapVector &from, const MapVector &to, const MapVector &leaving, const MapVector &arriving);

	MapVector Centre(double share) const;
	double Curvature(double share) const;
	double MeanCurvature() const;
	double MeanSharpness() const;
	double PeakSharpness() const;
	MapVector At(double share, double lateral) const override;
	MapVector Along(double share) const override;
	double Span() const override;
	double ShareOf(const MapVector &point) const override;

private:
	template <typename Visit>
	void Sample(Visit visit) const;
	MapVector Velocity(double share) const;
	MapVector Acceleration(double share) const;

	MapVector from;
	MapVector to;
	MapVector leaving;
	MapVector arriving;
	MapVector cubic;
	MapVector square;
};

#endif /* MINI_MAP_WAY_LINE_H */
