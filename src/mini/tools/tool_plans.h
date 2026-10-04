/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tool_plans.h The blueprints a build drag lays out before anything is built. */

#ifndef MINI_TOOLS_TOOL_PLANS_H
#define MINI_TOOLS_TOOL_PLANS_H

#include <utility>
#include <vector>

#include "../../direction_type.h"
#include "../../tile_type.h"
#include "../../track_type.h"
#include "../core/camera.h"

/* One command's worth of a line plan: an index range into its tiles and
 * whether the range crosses water as a bridge. */
struct PlanRun {
	size_t a;
	size_t b;
	bool bridge;
};

/* Pipe-style placement: the drag lays a free-form path that follows the
 * cursor tile by tile and turns where the cursor turns; stepping back onto
 * the previous tile undoes the last step. Pieces derive from the pairs of
 * tile edges the path crosses. */
struct RailPlan {
	std::vector<TileIndex> path;
	std::vector<std::pair<TileIndex, Track>> pieces;

	size_t Size() const { return this->pieces.size(); }
	void Walk(TileIndex anchor, TileIndex target);
	std::vector<PlanRun> Runs(bool remove) const;
	void Clear();
};

/* An axis-locked line: roads run along it, and a bridge spans it with the
 * two end tiles as ramps and everything between them as the bridge itself. */
struct LinePlan {
	Axis axis = AXIS_X;
	std::vector<TileIndex> tiles;

	size_t Size() const { return this->tiles.size(); }
	uint BridgeLength() const;
	void Lay(TilePoint anchor, TilePoint cursor);
	std::vector<PlanRun> Runs(bool remove) const;
	void Clear() { this->tiles.clear(); }
};

/* Signals lay in runs as well as one at a time. Only the two grid-straight
 * tracks carry a run: the half tracks stop the plan at the anchor tile, so
 * the preview always matches what the command will build. */
struct SignalPlan {
	Track track = INVALID_TRACK;
	std::vector<TileIndex> tiles;

	void Lay(TilePoint anchor, TilePoint cursor);
	void Clear();
};

struct AreaPlan {
	bool valid = false;
	int x0 = 0;
	int y0 = 0;
	int x1 = 0;
	int y1 = 0;

	int Width() const { return this->x1 - this->x0 + 1; }
	int Height() const { return this->y1 - this->y0 + 1; }
	size_t Tiles() const { return static_cast<size_t>(this->Width()) * this->Height(); }
	TileIndex Origin() const;
	TileIndex Far() const;
	size_t Index(int tx, int ty) const;
	void Span(TileIndex anchor, TileIndex cursor, int limit);
	void Clear() { this->valid = false; }

	template <typename Visit>
	void ForEach(Visit visit) const
	{
		for (int tx = this->x0; tx <= this->x1; tx++) {
			for (int ty = this->y0; ty <= this->y1; ty++) visit(tx, ty);
		}
	}
};

struct ToolPlans {
	RailPlan rail;
	LinePlan line;
	SignalPlan signal;
	AreaPlan area;

	void Clear();
};

Track SignalTrackAt(TileIndex tile, TilePoint at);

#endif /* MINI_TOOLS_TOOL_PLANS_H */
