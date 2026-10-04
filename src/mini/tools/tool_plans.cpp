/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tool_plans.cpp The blueprints a build drag lays out before anything is built. */

#include "../../stdafx.h"
#include "tool_plans.h"

#include <optional>
#include <span>
#include <tuple>

#include "../../core/math_func.hpp"
#include "../../map_func.h"
#include "../../rail_map.h"
#include "../../tile_map.h"
#include "../../track_func.h"
#include "tile_pick.h"

#include "../../safeguards.h"

static constexpr int WALK_STEP_LIMIT = 4096;
static constexpr size_t PATH_TILE_LIMIT = 1024;
static constexpr int MAX_LINE_STEPS = 127;

/* Edge bits: 1 = -x, 2 = +x, 4 = -y, 8 = +y. */
static int StepBit(TileIndex from, TileIndex to)
{
	if ((int)TileX(to) < (int)TileX(from)) return 1;
	if ((int)TileX(to) > (int)TileX(from)) return 2;
	if ((int)TileY(to) < (int)TileY(from)) return 4;
	return 8;
}

static int OppositeBit(int b)
{
	switch (b) {
		case 1: return 2;
		case 2: return 1;
		case 4: return 8;
		default: return 4;
	}
}

static Track EdgePairTrack(int mask)
{
	switch (mask) {
		case 1 | 2: return TRACK_X;
		case 4 | 8: return TRACK_Y;
		case 1 | 4: return TRACK_UPPER;
		case 2 | 8: return TRACK_LOWER;
		case 2 | 4: return TRACK_LEFT;
		case 1 | 8: return TRACK_RIGHT;
		default: return INVALID_TRACK;
	}
}

static bool StepBitIsX(int b)
{
	return b == 1 || b == 2;
}

/* Two corner pieces meeting as a switchback turn a train 90 degrees, which
 * rail cannot carry: with the last two steps perpendicular, a new step that
 * reverses the older one is refused and the walk detours instead. */
static bool RailStepAllowed(int d_pp, int d_prev, int d_new)
{
	if (d_pp == 0 || d_prev == 0) return true;
	return StepBitIsX(d_pp) == StepBitIsX(d_prev) || d_new != OppositeBit(d_pp);
}

static TileIndex StepTile(TileIndex t, int b)
{
	int x = (int)TileX(t) + (b == 2) - (b == 1);
	int y = (int)TileY(t) + (b == 8) - (b == 4);
	if (x < 0 || y < 0 || x > (int)Map::SizeX() - 2 || y > (int)Map::SizeY() - 2) return INVALID_TILE;
	return TileXY(x, y);
}

static std::vector<std::pair<TileIndex, Track>> PathPieces(std::span<const TileIndex> path)
{
	std::vector<std::pair<TileIndex, Track>> pieces;
	size_t n = path.size();
	if (n < 2) return pieces;
	for (size_t i = 0; i < n; i++) {
		int in = i > 0 ? OppositeBit(StepBit(path[i - 1], path[i])) : 0;
		int out = i + 1 < n ? StepBit(path[i], path[i + 1]) : 0;
		if (in == 0) in = OppositeBit(out);
		if (out == 0) out = OppositeBit(in);
		Track t = EdgePairTrack(in | out);
		if (t != INVALID_TRACK) pieces.emplace_back(path[i], t);
	}
	return pieces;
}

/* A straight drag bridges water automatically: each water run becomes a
 * bridge between its neighbouring land tiles, which turn into ramps, so
 * the land spans stop short of them. Water at either end leaves no ramp. */
static std::optional<std::vector<PlanRun>> WaterRuns(std::span<const TileIndex> ts, size_t offset)
{
	size_t n = ts.size();
	std::vector<bool> ramp(n, false);
	std::vector<PlanRun> runs;
	for (size_t i = 0; i < n; i++) {
		if (!IsTileType(ts[i], MP_WATER)) continue;
		size_t j = i;
		while (j + 1 < n && IsTileType(ts[j + 1], MP_WATER)) j++;
		if (i == 0 || j == n - 1) return std::nullopt;
		ramp[i - 1] = true;
		ramp[j + 1] = true;
		runs.push_back({offset + i - 1, offset + j + 1, true});
		i = j;
	}
	for (size_t i = 0; i < n; i++) {
		if (IsTileType(ts[i], MP_WATER) || ramp[i]) continue;
		size_t j = i;
		while (j + 1 < n && !IsTileType(ts[j + 1], MP_WATER) && !ramp[j + 1]) j++;
		runs.push_back({offset + i, offset + j, false});
		i = j;
	}
	return runs;
}

void RailPlan::Walk(TileIndex anchor, TileIndex target)
{
	if (this->path.empty()) this->path.push_back(anchor);

	int tx = TileX(target);
	int ty = TileY(target);
	for (int guard = 0; guard < WALK_STEP_LIMIT && this->path.size() < PATH_TILE_LIMIT; guard++) {
		TileIndex cur = this->path.back();
		int cx = (int)TileX(cur), cy = (int)TileY(cur);
		int dx = tx - cx, dy = ty - cy;
		if (dx == 0 && dy == 0) break;

		size_t n = this->path.size();
		int d_prev = n >= 2 ? StepBit(this->path[n - 2], this->path[n - 1]) : 0;
		int d_pp = n >= 3 ? StepBit(this->path[n - 3], this->path[n - 2]) : 0;

		int step_x = dx != 0 ? (dx > 0 ? 2 : 1) : 0;
		int step_y = dy != 0 ? (dy > 0 ? 8 : 4) : 0;
		int prim = std::abs(dx) >= std::abs(dy) ? step_x : step_y;
		int sec = prim == step_x ? step_y : step_x;

		TileIndex next = INVALID_TILE;
		bool popped = false;
		for (int cand : {prim, sec, d_prev}) {
			if (cand == 0) continue;
			TileIndex t = StepTile(cur, cand);
			if (t == INVALID_TILE) continue;
			/* Any candidate stepping back onto the previous tile is the undo
			 * gesture; undoing ignores the turn rule. */
			if (n >= 2 && t == this->path[n - 2]) {
				this->path.pop_back();
				popped = true;
				break;
			}
			if (!RailStepAllowed(d_pp, d_prev, cand)) continue;
			next = t;
			break;
		}
		if (popped) continue;
		if (next == INVALID_TILE) break;
		this->path.push_back(next);
	}

	this->pieces = PathPieces(this->path);
}

static size_t StraightEnd(std::span<const std::pair<TileIndex, Track>> pieces, size_t i)
{
	Track t = pieces[i].second;
	if (t != TRACK_X && t != TRACK_Y) return i;

	size_t j = i;
	int dir = 0;
	while (j + 1 < pieces.size() && pieces[j + 1].second == t) {
		int step = StepBit(pieces[j].first, pieces[j + 1].first);
		if (dir != 0 && step != dir) break;
		dir = step;
		j++;
	}
	return j;
}

static std::vector<TileIndex> PieceTiles(std::span<const std::pair<TileIndex, Track>> pieces)
{
	std::vector<TileIndex> tiles;
	for (const auto &[tile, track] : pieces) tiles.push_back(tile);
	return tiles;
}

/* Maximal straight runs go through the range command so the water auto-bridge
 * logic still applies; corner pieces stand alone. The commit and the blueprint
 * probe both walk the plan this way. */
std::vector<PlanRun> RailPlan::Runs(bool remove) const
{
	std::vector<PlanRun> runs;
	if (remove) {
		for (size_t k = 0; k < this->pieces.size(); k++) runs.push_back({k, k, false});
		return runs;
	}

	for (size_t i = 0; i < this->pieces.size();) {
		size_t j = StraightEnd(this->pieces, i);
		std::optional<std::vector<PlanRun>> water;
		if (j > i) water = WaterRuns(PieceTiles(std::span(this->pieces).subspan(i, j - i + 1)), i);
		if (water.has_value()) {
			runs.insert(runs.end(), water->begin(), water->end());
		} else {
			runs.push_back({i, j, false});
		}
		i = j + 1;
	}
	return runs;
}

void RailPlan::Clear()
{
	this->path.clear();
	this->pieces.clear();
}

uint LinePlan::BridgeLength() const
{
	return this->tiles.size() < 3 ? 0 : (uint)(this->tiles.size() - 2);
}

void LinePlan::Lay(TilePoint anchor, TilePoint cursor)
{
	this->tiles.clear();

	TileIndex start = PathTileAt(anchor);
	int atx = TileX(start);
	int aty = TileY(start);
	double dx = cursor.first - anchor.first;
	double dy = cursor.second - anchor.second;

	this->axis = std::abs(dx) >= std::abs(dy) ? AXIS_X : AXIS_Y;
	int steps = Clamp((int)std::lround(this->axis == AXIS_X ? dx : dy), -MAX_LINE_STEPS, MAX_LINE_STEPS);
	int dir = steps >= 0 ? 1 : -1;
	for (int i = 0; ; i += dir) {
		int tx = this->axis == AXIS_X ? atx + i : atx;
		int ty = this->axis == AXIS_Y ? aty + i : aty;
		if (tx < 0 || ty < 0 || tx > (int)Map::SizeX() - 2 || ty > (int)Map::SizeY() - 2) break;
		this->tiles.push_back(TileXY(tx, ty));
		if (i == steps) break;
	}
}

std::vector<PlanRun> LinePlan::Runs(bool remove) const
{
	if (this->tiles.empty()) return {};

	PlanRun whole{0, this->tiles.size() - 1, false};
	if (remove) return {whole};
	return WaterRuns(this->tiles, 0).value_or(std::vector<PlanRun>{whole});
}

void SignalPlan::Lay(TilePoint anchor, TilePoint cursor)
{
	this->tiles.clear();

	TileIndex start = WholeTileAt(anchor);
	this->track = SignalTrackAt(start, anchor);
	if (this->track == INVALID_TRACK) return;

	this->tiles.push_back(start);
	if (this->track != TRACK_X && this->track != TRACK_Y) return;

	int ax = TileX(start);
	int ay = TileY(start);
	bool along_x = this->track == TRACK_X;
	int steps = Clamp((int)std::lround(along_x ? cursor.first - anchor.first : cursor.second - anchor.second), -MAX_LINE_STEPS, MAX_LINE_STEPS);
	int dir = steps >= 0 ? 1 : -1;
	for (int i = dir; i != steps + dir; i += dir) {
		int tx = along_x ? ax + i : ax;
		int ty = along_x ? ay : ay + i;
		if (tx < 0 || ty < 0 || tx >= (int)Map::SizeX() || ty >= (int)Map::SizeY()) break;
		TileIndex tile = TileXY(tx, ty);
		if (!IsPlainRailTile(tile)) break;
		if ((GetTrackBits(tile) & TrackToTrackBits(this->track)) == TRACK_BIT_NONE) break;
		this->tiles.push_back(tile);
	}
}

void SignalPlan::Clear()
{
	this->tiles.clear();
	this->track = INVALID_TRACK;
}

TileIndex AreaPlan::Origin() const
{
	return TileXY(this->x0, this->y0);
}

TileIndex AreaPlan::Far() const
{
	return TileXY(this->x1, this->y1);
}

/* Tiles are numbered along the y axis inside each x column; the probe and
 * the blueprint both index them this way. */
size_t AreaPlan::Index(int tx, int ty) const
{
	return static_cast<size_t>(tx - this->x0) * this->Height() + (ty - this->y0);
}

static std::pair<int, int> SpanAxis(int anchor, int cursor, int limit)
{
	if (cursor >= anchor) return {anchor, std::min(cursor, anchor + limit - 1)};
	return {std::max(cursor, anchor - limit + 1), anchor};
}

/* Rectangle drag; the far corner truncates at the size limit so the
 * anchor corner always stays inside the allowed area. */
void AreaPlan::Span(TileIndex anchor, TileIndex cursor, int limit)
{
	std::tie(this->x0, this->x1) = SpanAxis(TileX(anchor), TileX(cursor), limit);
	std::tie(this->y0, this->y1) = SpanAxis(TileY(anchor), TileY(cursor), limit);
	this->valid = true;
}

void ToolPlans::Clear()
{
	this->rail.Clear();
	this->line.Clear();
	this->signal.Clear();
	this->area.Clear();
}

/* Same sub-track pick as GenericPlaceSignals: on paired straight pieces the
 * fractional click position decides which half gets the signal. */
Track SignalTrackAt(TileIndex tile, TilePoint at)
{
	if (!IsPlainRailTile(tile)) return INVALID_TRACK;
	TrackBits trackbits = GetTrackBits(tile);
	double fx = at.first - std::floor(at.first);
	double fy = at.second - std::floor(at.second);
	if (trackbits & TRACK_BIT_VERT) trackbits = (fx <= fy) ? TRACK_BIT_RIGHT : TRACK_BIT_LEFT;
	if (trackbits & TRACK_BIT_HORZ) trackbits = (fx + fy <= 1.0) ? TRACK_BIT_UPPER : TRACK_BIT_LOWER;
	return FindFirstTrack(trackbits);
}
