/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file way_profile.cpp The levels rails and roads are eased to along their runs, so a run climbs at a steady grade on low banks instead of stepping with the tiles under it. */

#include "../../stdafx.h"
#include "way_profile.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>
#include <unordered_map>
#include <vector>

#include "../../core/bitmath_func.hpp"
#include "../../map_func.h"
#include "../../road_type.h"
#include "../../track_func.h"
#include "tile_shapes.h"
#include "world_tiles.h"

#include "../../safeguards.h"

/* A way is never eased further above its ground than this, in height levels, nor ever below it. */
static constexpr double LIFT_LIMIT = 0.5;
static constexpr double EASE_PULL = 0.6;
static constexpr double RAISED_LIFT = 0.02;
static constexpr int RAISED_SAMPLES = 8;
static constexpr int ROAD_ARMS = 4;

/* Where each of the game's road bits leads out of its tile, in bit order. */
static constexpr std::array<MapVector, ROAD_ARMS> ROAD_ENDS = {{{0.5, 0.0}, {1.0, 0.5}, {0.5, 1.0}, {0.0, 0.5}}};

enum class WayKind : uint8_t {
	None,
	Rail,
	Road,
};

/* The middle of a tile's side, named on the half tile lattice, so the tiles either side name it alike. */
struct Joint {
	int hx;
	int hy;

	static Joint At(const MapVector &point) { return {static_cast<int>(std::lround(point.x * 2.0)), static_cast<int>(std::lround(point.y * 2.0))}; }
	MapVector Point() const { return {this->hx * HALF_TILE, this->hy * HALF_TILE}; }
	bool AlongX() const { return (this->hx & 1) != 0; }
	uint64_t Key() const { return static_cast<uint64_t>(static_cast<uint32_t>(this->hx)) << 32 | static_cast<uint32_t>(this->hy); }
	bool operator==(const Joint &) const = default;

	/* The two tiles whose sides meet here, the one toward lower map coordinates first. */
	std::array<std::pair<int, int>, 2> Tiles() const
	{
		if (this->AlongX()) return {{{(this->hx - 1) / 2, this->hy / 2 - 1}, {(this->hx - 1) / 2, this->hy / 2}}};
		return {{{this->hx / 2 - 1, (this->hy - 1) / 2}, {this->hx / 2, (this->hy - 1) / 2}}};
	}

	/* The way across the side from one of its tiles into the other. */
	MapVector Across(int tx, int ty) const
	{
		auto [low, high] = this->Tiles();
		double sign = low.first == tx && low.second == ty ? 1.0 : -1.0;
		return this->AlongX() ? MapVector{0.0, sign} : MapVector{sign, 0.0};
	}

	/* How far a point lies from the line of the tile side this joint is on. */
	double SideDistance(const MapVector &point) const
	{
		return this->AlongX() ? std::abs(point.y - this->hy * HALF_TILE) : std::abs(point.x - this->hx * HALF_TILE);
	}
};

struct Piece {
	Joint from;
	Joint to;

	Joint Other(const Joint &end) const { return end == this->from ? this->to : this->from; }
	double Span() const { return std::hypot(this->to.hx - this->from.hx, this->to.hy - this->from.hy) * HALF_TILE; }
};

/* A tile's way pieces as far as easing goes: the tile eases only with one rail piece or a parallel pair, or one road running in at two ends. */
struct TileWays {
	WayKind kind = WayKind::None;
	std::array<Piece, 2> pieces{};
	uint8_t count = 0;

	bool Eases() const { return this->count > 0; }

	std::optional<Piece> Touching(const Joint &joint) const
	{
		std::optional<Piece> found;
		for (uint8_t index = 0; index < this->count; index++) {
			const Piece &piece = this->pieces[index];
			if (piece.from != joint && piece.to != joint) continue;
			if (found.has_value()) return std::nullopt;
			found = piece;
		}
		return found;
	}
};

static bool EasingTrackBits(TrackBits bits)
{
	return HasExactlyOneBit(bits) || bits == TRACK_BIT_HORZ || bits == TRACK_BIT_VERT;
}

static TileWays WaysOn(int tx, int ty)
{
	TileWays ways;
	if (!OnMap(tx, ty)) return ways;
	TileIndex tile = TileXY(tx, ty);
	if (!_world_tiles.WaysEase(tile)) return ways;

	MapVector origin = {static_cast<double>(tx), static_cast<double>(ty)};
	NetworkTexel texel = _world_tiles.NetworkAt(tile);
	TrackBits track = static_cast<TrackBits>(texel.track);
	if (track != TRACK_BIT_NONE) {
		if (!EasingTrackBits(track)) return ways;
		ways.kind = WayKind::Rail;
		for (Track piece : SetTrackBitIterator(track)) {
			auto [from, to] = TRACK_ENDS[piece];
			ways.pieces[ways.count++] = {Joint::At(origin + from), Joint::At(origin + to)};
		}
		return ways;
	}

	uint bits = texel.road | texel.tram;
	if (std::popcount(bits) != 2) return ways;
	ways.kind = WayKind::Road;
	int first = std::countr_zero(bits);
	int last = std::bit_width(bits) - 1;
	ways.pieces[ways.count++] = {Joint::At(origin + ROAD_ENDS[first]), Joint::At(origin + ROAD_ENDS[last])};
	return ways;
}

/* A joint eases when the two pieces meeting there are the only ones and their tiles both ease the same kind of way. */
static bool Eases(const Joint &joint)
{
	auto [low, high] = joint.Tiles();
	TileWays a = WaysOn(low.first, low.second);
	TileWays b = WaysOn(high.first, high.second);
	return a.Eases() && a.kind == b.kind && a.Touching(joint).has_value() && b.Touching(joint).has_value();
}

static double GroundAt(int tx, int ty, const Joint &joint)
{
	MapVector point = joint.Point();
	return TileGround(tx, ty).Level(point.x, point.y);
}

/* A run's joint where the ease is worked out: its place along the run, the ground under it, and whether it holds its level. */
struct RunNode {
	Joint joint;
	double place;
	double ground;
	bool held;
};

/* The joints a run passes stepping from one joint into a tile beside it, up to the reach; the last one holds, as does any joint that does not ease. */
static std::vector<RunNode> Walk(Joint at, int tx, int ty, double direction)
{
	std::vector<RunNode> nodes;
	double place = 0.0;
	for (int step = 0; step < WAY_EASE_REACH; step++) {
		std::optional<Piece> piece = WaysOn(tx, ty).Touching(at);
		if (!piece.has_value()) break;
		Joint next = piece->Other(at);
		place += piece->Span() * direction;
		if (!Eases(next)) {
			nodes.push_back({next, place, GroundAt(tx, ty, next), true});
			return nodes;
		}
		auto [low, high] = next.Tiles();
		nodes.push_back({next, place, std::max(GroundAt(low.first, low.second, next), GroundAt(high.first, high.second, next)), false});
		bool from_low = low.first == tx && low.second == ty;
		tx = from_low ? high.first : low.first;
		ty = from_low ? high.second : low.second;
		at = next;
	}
	if (!nodes.empty()) nodes.back().held = true;
	return nodes;
}

/* A joint's eased level, how steeply the run climbs through it toward the tile on its higher side, and the joints either side of it on the run. */
struct JointEase {
	double level;
	double slope;
	Joint low_side;
	Joint high_side;
};

/* Neither steeper than the gentler of the two runs either side, nor anything but level where they do not climb the same way. */
static double JointSlope(double before, double after)
{
	if (before * after <= 0.0) return 0.0;
	return 2.0 * before * after / (before + after);
}

/* The run's levels are pulled toward the straight line between their neighbours, a step for each joint of reach, yet kept on or a little above the ground. */
static JointEase Solve(const Joint &joint)
{
	auto [low, high] = joint.Tiles();
	std::vector<RunNode> lows = Walk(joint, low.first, low.second, -1.0);
	std::vector<RunNode> highs = Walk(joint, high.first, high.second, 1.0);

	std::vector<RunNode> run(lows.rbegin(), lows.rend());
	size_t centre = run.size();
	run.push_back({joint, 0.0, std::max(GroundAt(low.first, low.second, joint), GroundAt(high.first, high.second, joint)), false});
	run.insert(run.end(), highs.begin(), highs.end());

	std::vector<double> levels(run.size());
	std::ranges::transform(run, levels.begin(), &RunNode::ground);
	std::vector<double> next = levels;
	for (int pass = 0; pass < WAY_EASE_REACH; pass++) {
		for (size_t index = 1; index + 1 < run.size(); index++) {
			if (run[index].held) continue;
			const RunNode &before = run[index - 1];
			const RunNode &after = run[index + 1];
			double share = (run[index].place - before.place) / (after.place - before.place);
			double line = std::lerp(levels[index - 1], levels[index + 1], share);
			double pulled = levels[index] + (line - levels[index]) * EASE_PULL;
			next[index] = std::clamp(pulled, run[index].ground, run[index].ground + LIFT_LIMIT);
		}
		std::swap(levels, next);
		next = levels;
	}

	auto climb = [&](size_t a, size_t b) { return (levels[b] - levels[a]) / (run[b].place - run[a].place); };
	return {levels[centre], JointSlope(climb(centre - 1, centre), climb(centre, centre + 1)), run[centre - 1].joint, run[centre + 1].joint};
}

/* Eases are worked out once for each joint until any texel changes. */
class EaseBook {
public:
	std::optional<JointEase> At(const Joint &joint)
	{
		if (this->revision != _world_tiles.Revision()) {
			this->eases.clear();
			this->revision = _world_tiles.Revision();
		}
		auto [entry, fresh] = this->eases.try_emplace(joint.Key());
		if (fresh && Eases(joint)) entry->second = Solve(joint);
		return entry->second;
	}

private:
	std::unordered_map<uint64_t, std::optional<JointEase>> eases;
	uint64_t revision = UINT64_MAX;
};

static EaseBook _ease_book;

WayCourse::WayCourse(int tx, int ty, const Piece &piece, bool road) : tx(tx), ty(ty), from(piece.from.Point()), to(piece.to.Point()), road(road)
{
	CourseEnd first = this->EndAt(this->from, true);
	CourseEnd last = this->EndAt(this->to, false);
	this->before = first.run * -1.0;
	this->after = last.run;
	this->start_level = first.level;
	this->end_level = last.level;
	double chord = (last.level - first.level) / this->Length();
	this->start_slope = first.slope.value_or(chord);
	this->end_slope = last.slope.value_or(chord);
}

/* An end where the run eases takes the joint's level and its climb along the course; one that holds keeps to the tile's ground, climbing straight at the other end.
 * The run beyond an eased end goes on along the next road square to the tile side, or along the next rail piece toward the joint past it. */
WayCourse::CourseEnd WayCourse::EndAt(const MapVector &point, bool leaving) const
{
	Joint joint = Joint::At(point);
	std::optional<JointEase> ease = _ease_book.At(joint);
	if (!ease.has_value()) return {GroundAt(this->tx, this->ty, joint), std::nullopt, {}};

	bool high = joint.Tiles()[1] == std::pair{this->tx, this->ty};
	const Joint &outer = high ? ease->low_side : ease->high_side;
	MapVector run = this->road ? joint.Across(this->tx, this->ty) : Unit(outer.Point() - point);
	return {ease->level, ease->slope * (high == leaving ? 1.0 : -1.0), run};
}

std::optional<WayCourse> WayCourse::OfTrack(int tx, int ty, Track track)
{
	TileWays ways = WaysOn(tx, ty);
	if (ways.kind != WayKind::Rail || !HasBit(_world_tiles.NetworkAt(TileXY(tx, ty)).track, track)) return std::nullopt;
	MapVector origin = {static_cast<double>(tx), static_cast<double>(ty)};
	auto [from, to] = TRACK_ENDS[track];
	return WayCourse(tx, ty, {Joint::At(origin + from), Joint::At(origin + to)}, false);
}

std::optional<WayCourse> WayCourse::OfRoad(int tx, int ty)
{
	TileWays ways = WaysOn(tx, ty);
	if (ways.kind != WayKind::Road) return std::nullopt;
	return WayCourse(tx, ty, ways.pieces[0], true);
}

bool WayCourse::Curved() const
{
	return this->road && this->from.x != this->to.x && this->from.y != this->to.y;
}

MapVector WayCourse::Centre(double share) const
{
	if (!this->Curved()) return this->from + (this->to - this->from) * share;
	bool from_along_x = Joint::At(this->from).AlongX();
	MapVector corner = from_along_x ? MapVector{this->to.x, this->from.y} : MapVector{this->from.x, this->to.y};
	double angle = share * std::numbers::pi * HALF_TILE;
	return corner + (this->from - corner) * std::cos(angle) + (this->to - corner) * std::sin(angle);
}

double WayCourse::Length() const
{
	return std::hypot(this->to.x - this->from.x, this->to.y - this->from.y);
}

double WayCourse::ShareAt(const MapVector &point) const
{
	double near = Joint::At(this->from).SideDistance(point);
	double far = Joint::At(this->to).SideDistance(point);
	return near + far > 0.0 ? near / (near + far) : 0.0;
}

double WayCourse::Level(double share) const
{
	double t = std::clamp(share, 0.0, 1.0);
	double t2 = t * t;
	double t3 = t2 * t;
	double length = this->Length();
	double eased = (2.0 * t3 - 3.0 * t2 + 1.0) * this->start_level + (t3 - 2.0 * t2 + t) * this->start_slope * length
		+ (3.0 * t2 - 2.0 * t3) * this->end_level + (t3 - t2) * this->end_slope * length;
	MapVector centre = this->Centre(t);
	return std::max(eased, TileGround(this->tx, this->ty).Level(centre.x, centre.y));
}

double WayCourse::Lift(double share) const
{
	MapVector centre = this->Centre(share);
	return this->Level(share) - TileGround(this->tx, this->ty).Level(centre.x, centre.y);
}

bool WayCourse::Raised() const
{
	for (int sample = 0; sample <= RAISED_SAMPLES; sample++) {
		if (this->Lift(static_cast<double>(sample) / RAISED_SAMPLES) > RAISED_LIFT) return true;
	}
	return false;
}

static double SegmentDistance(const MapVector &point, const MapVector &from, const MapVector &to)
{
	MapVector along = to - from;
	double share = std::clamp(Dot(point - from, along) / Dot(along, along), 0.0, 1.0);
	MapVector off = point - (from + along * share);
	return std::hypot(off.x, off.y);
}

double WayLevel(double x, double y)
{
	int tx = static_cast<int>(std::floor(x));
	int ty = static_cast<int>(std::floor(y));
	TileWays ways = WaysOn(tx, ty);
	if (!ways.Eases()) return GroundLevel(x, y);

	MapVector point = {x, y};
	if (ways.kind == WayKind::Road) {
		WayCourse course = *WayCourse::OfRoad(tx, ty);
		return course.Level(course.ShareAt(point));
	}
	auto nearest = std::ranges::min_element(ways.pieces.begin(), ways.pieces.begin() + ways.count, {}, [&](const Piece &piece) {
		return SegmentDistance(point, piece.from.Point(), piece.to.Point());
	});
	WayCourse course(tx, ty, *nearest, false);
	return course.Level(course.ShareAt(point));
}
