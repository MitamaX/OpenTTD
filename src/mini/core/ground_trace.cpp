/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ground_trace.cpp Where a sight line through the 3D world first meets the map's ground. */

#include "../../stdafx.h"
#include "ground_trace.h"

#include <algorithm>
#include <array>
#include <limits>

#include "../../map_func.h"
#include "../map/world_tiles.h"

#include "../../safeguards.h"

static constexpr double NEVER = std::numeric_limits<double>::infinity();
static constexpr double PARALLEL = 1e-12;
static constexpr double WALL_INSET = 1e-6;
static constexpr double WALL_TOLERANCE = 1e-6;

/* The span of t over which the line runs above the map's plan, if it crosses it at all. */
static std::optional<std::pair<double, double>> PlanSpan(const SightLine &sight, double reach)
{
	double enter = 0.0;
	double leave = reach;
	const std::array<std::pair<double, double>, 2> axes = {{{sight.origin.x, sight.direction.x}, {sight.origin.y, sight.direction.y}}};
	const std::array<double, 2> sizes = {static_cast<double>(Map::SizeX()), static_cast<double>(Map::SizeY())};
	for (size_t axis = 0; axis < axes.size(); axis++) {
		auto [from, along] = axes[axis];
		if (std::abs(along) < PARALLEL) {
			if (from < 0.0 || from > sizes[axis]) return std::nullopt;
			continue;
		}
		double t0 = (0.0 - from) / along;
		double t1 = (sizes[axis] - from) / along;
		enter = std::max(enter, std::min(t0, t1));
		leave = std::min(leave, std::max(t0, t1));
	}
	if (enter >= leave) return std::nullopt;
	return std::pair{enter, leave};
}

/* Walks the tiles under a sight line in the order it passes over them. */
class TileWalk {
public:
	TileWalk(const SightLine &sight, double start) : sight(sight), t(start)
	{
		Vec3 at = sight.At(start + WALL_INSET);
		this->tx = std::clamp(static_cast<int>(std::floor(at.x)), 0, static_cast<int>(Map::MaxX()));
		this->ty = std::clamp(static_cast<int>(std::floor(at.y)), 0, static_cast<int>(Map::MaxY()));
		this->step_x = sight.direction.x > 0.0 ? 1 : -1;
		this->step_y = sight.direction.y > 0.0 ? 1 : -1;
		this->next_x = Crossing(sight.origin.x, sight.direction.x, this->tx + (this->step_x > 0 ? 1 : 0));
		this->next_y = Crossing(sight.origin.y, sight.direction.y, this->ty + (this->step_y > 0 ? 1 : 0));
		this->delta_x = std::abs(sight.direction.x) < PARALLEL ? NEVER : 1.0 / std::abs(sight.direction.x);
		this->delta_y = std::abs(sight.direction.y) < PARALLEL ? NEVER : 1.0 / std::abs(sight.direction.y);
	}

	int Tx() const { return this->tx; }
	int Ty() const { return this->ty; }
	double Entry() const { return this->t; }
	double Exit() const { return std::min(this->next_x, this->next_y); }

	bool Advance()
	{
		if (this->next_x < this->next_y) {
			this->t = this->next_x;
			this->tx += this->step_x;
			this->next_x += this->delta_x;
		} else {
			this->t = this->next_y;
			this->ty += this->step_y;
			this->next_y += this->delta_y;
		}
		return this->tx >= 0 && this->ty >= 0 && this->tx <= static_cast<int>(Map::MaxX()) && this->ty <= static_cast<int>(Map::MaxY());
	}

private:
	static double Crossing(double from, double along, int edge)
	{
		return std::abs(along) < PARALLEL ? NEVER : (edge - from) / along;
	}

	const SightLine &sight;
	double t;
	int tx = 0;
	int ty = 0;
	int step_x = 1;
	int step_y = 1;
	double next_x = NEVER;
	double next_y = NEVER;
	double delta_x = NEVER;
	double delta_y = NEVER;
};

/* How far above one tile's own ground the line passes at t; the tile's facets make this piecewise straight. */
class TileClearance {
public:
	TileClearance(const SightLine &sight, int tx, int ty) : sight(sight), tx(tx), ty(ty), surface(_world_tiles.SurfaceAt(TileXY(tx, ty)))
	{
	}

	double At(double t) const
	{
		Vec3 at = this->sight.At(t);
		return at.z - FacetLevel(this->surface, at.x - this->tx, at.y - this->ty) * this->sight.rise;
	}

	/* Where the line crosses the fold between the tile's two facets. */
	std::optional<double> Fold() const
	{
		const Vec3 &o = this->sight.origin;
		const Vec3 &d = this->sight.direction;
		double fx = o.x - this->tx;
		double fy = o.y - this->ty;
		bool west_to_east = FoldsWestToEast(this->surface);
		double rate = west_to_east ? d.x + d.y : d.x - d.y;
		if (std::abs(rate) < PARALLEL) return std::nullopt;
		return west_to_east ? (1.0 - fx - fy) / rate : (fy - fx) / rate;
	}

	std::optional<double> FirstHit(double from, double to) const
	{
		std::array<double, 3> marks = {from, to, to};
		size_t count = 2;
		if (std::optional<double> fold = this->Fold(); fold.has_value() && *fold > from && *fold < to) {
			marks = {from, *fold, to};
			count = 3;
		}
		double above = this->At(marks[0]);
		if (above < 0.0) return from;
		for (size_t i = 1; i < count; i++) {
			double next = this->At(marks[i]);
			if (next <= 0.0) return std::lerp(marks[i - 1], marks[i], above / (above - next));
			above = next;
		}
		return std::nullopt;
	}

private:
	const SightLine &sight;
	int tx;
	int ty;
	SurfaceTexel surface;
};

static GroundHit HitAt(const SightLine &sight, double t)
{
	Vec3 at = sight.At(t);
	return {at.x, at.y, at.z / sight.rise};
}

std::optional<GroundHit> TraceGround(const SightLine &sight, double reach)
{
	if (_world_tiles.Size().width == 0) return std::nullopt;
	std::optional<std::pair<double, double>> span = PlanSpan(sight, reach);
	if (!span.has_value()) return std::nullopt;

	TileWalk walk(sight, span->first);
	bool crossed = false;
	do {
		TileClearance clearance(sight, walk.Tx(), walk.Ty());
		if (crossed && clearance.At(walk.Entry()) < -WALL_TOLERANCE) return HitAt(sight, walk.Entry() + WALL_INSET);

		double exit = std::min(walk.Exit(), span->second);
		if (std::optional<double> hit = clearance.FirstHit(walk.Entry(), exit); hit.has_value()) return HitAt(sight, *hit);
		if (exit >= span->second) break;
		crossed = true;
	} while (walk.Advance());
	return std::nullopt;
}
