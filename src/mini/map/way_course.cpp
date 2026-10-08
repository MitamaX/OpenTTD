/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file way_course.cpp The courses rails and roads are eased to along their runs: rails curve through the tiles they cross instead of turning at every tile side,
 * and every run climbs at a steady grade on low banks instead of stepping with the tiles under it. */

#include "../../stdafx.h"
#include "way_course.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <unordered_map>
#include <vector>

#include "../../bridge_map.h"
#include "../../core/bitmath_func.hpp"
#include "../../map_func.h"
#include "../../road_type.h"
#include "../../track_func.h"
#include "../../tunnelbridge_map.h"
#include "../gpu/frame_profile.h"
#include "tile_shapes.h"
#include "world_tiles.h"

#include "../../safeguards.h"

/* A way is never eased further above its ground than this, in height levels, nor ever below it. */
static constexpr double LIFT_LIMIT = 0.5;
static constexpr double EASE_PULL = 0.6;
/* A rail joint slides along its tile side no further off the game's line than this, nor nearer a tile corner than the margin, so the eased line keeps to the tiles the track crosses. */
static constexpr double DRIFT_LIMIT = 0.35;
static constexpr double CORNER_MARGIN = 0.05;
static constexpr double SLIDE_PULL = 0.5;
/* A rail's heading across a tile side never leans nearer to the side than this cosine allows, so a bend stays within the tiles it joins. */
static constexpr double LEAST_CROSSING = 0.5;
/* A run is traced no further than this many joints either way from where it is first asked for. */
static constexpr int LONGEST_WALK = 1024;
static constexpr double RAISED_LIFT = 0.02;
static constexpr int RAISED_SAMPLES = 8;
static constexpr int ROAD_ARMS = 4;
/* A drawn piece reads the eases at the sides of its tile and of the tiles beside it, which ease with the tiles beyond those. */
static constexpr int DRAWN_READ_REACH = 2;

/* Where each of the game's road bits leads out of its tile, in bit order. */
static constexpr std::array<MapVector, ROAD_ARMS> ROAD_ENDS = {{{0.5, 0.0}, {1.0, 0.5}, {0.5, 1.0}, {0.0, 0.5}}};

using TileCoord = std::pair<int, int>;

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
	double Span() const { return std::hypot(this->to.hx - this->from.hx, this->to.hy - this->from.hy) * HALF_TILE; }
};

static TileCoord TileOf(const MapVector &point)
{
	return {static_cast<int>(std::floor(point.x)), static_cast<int>(std::floor(point.y))};
}

static double Distance(const MapVector &from, const MapVector &to)
{
	return std::hypot(to.x - from.x, to.y - from.y);
}

/* How far along a segment a point lies, kept within the segment. */
static double ShareAlong(const MapVector &point, const MapVector &from, const MapVector &to)
{
	MapVector along = to - from;
	return std::clamp(Dot(point - from, along) / Dot(along, along), 0.0, 1.0);
}

static double SegmentDistance(const MapVector &point, const MapVector &from, const MapVector &to)
{
	return Distance(point, from + (to - from) * ShareAlong(point, from, to));
}

static Piece PieceOf(int tx, int ty, Track track)
{
	MapVector origin = {static_cast<double>(tx), static_cast<double>(ty)};
	auto [from, to] = TRACK_ENDS[track];
	return {Joint::At(origin + from), Joint::At(origin + to)};
}

/* The rail pieces a tile carries as far as the track beside it goes, a rail bridge's head among them. */
static TrackBits TrackBitsOf(int tx, int ty)
{
	TileIndex tile = TileXY(tx, ty);
	if (IsBridgeTile(tile) && GetTunnelBridgeTransportType(tile) == TRANSPORT_RAIL) return AxisToTrackBits(DiagDirToAxis(GetTunnelBridgeDirection(tile)));
	return static_cast<TrackBits>(_world_tiles.NetworkAt(tile).track);
}

/* The piece of a tile's track rated highest among some of its pieces, passing over any the rating gives no value. */
template <typename Rating>
static std::optional<Track> BestTrackAt(const TileCoord &tile, Rating rating, TrackBits among = TRACK_BIT_ALL)
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
static std::optional<Piece> BestPieceAt(const TileCoord &tile, const Joint &joint, Rating rating, TrackBits among = TRACK_BIT_ALL)
{
	std::optional<Track> best = BestTrackAt(tile, [&](const Piece &piece) -> std::optional<double> {
		if (!piece.Meets(joint)) return std::nullopt;
		return rating(piece);
	}, among);
	if (!best.has_value()) return std::nullopt;
	return PieceOf(tile.first, tile.second, *best);
}

/* The piece of a tile meeting a joint that runs on from it most nearly along a heading, unless none meets it. */
static std::optional<Piece> StraightestAt(const TileCoord &tile, const Joint &joint, const MapVector &heading, TrackBits among = TRACK_BIT_ALL)
{
	return BestPieceAt(tile, joint, [&](const Piece &piece) { return Dot(Unit(piece.Other(joint).Point() - joint.Point()), heading); }, among);
}

static std::optional<Track> TrackUnder(const TileCoord &tile, const MapVector &point)
{
	return BestTrackAt(tile, [&](const Piece &piece) -> std::optional<double> { return -SegmentDistance(point, piece.from.Point(), piece.to.Point()); });
}

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
			if (!piece.Meets(joint)) continue;
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
		for (Track piece : SetTrackBitIterator(track)) ways.pieces[ways.count++] = PieceOf(tx, ty, piece);
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

static double GroundAt(const TileCoord &tile, const Joint &joint)
{
	MapVector point = joint.Point();
	return TileGround(tile.first, tile.second).Level(point.x, point.y);
}

/* A joint's eased place, level and climb, and the run's heading through it, the climb and the heading both toward the joint's higher tile;
 * and how far along the run from the run's first end it lies, unless a loop closes there, and whether that grows toward its higher tile. */
struct JointEase {
	MapVector at;
	double level;
	double slope;
	MapVector heading;
	std::optional<double> distance;
	bool rising;
};

/* Neither steeper than the gentler of the two runs either side, nor anything but level where they do not climb the same way. */
static double JointSlope(double before, double after)
{
	if (before * after <= 0.0) return 0.0;
	return 2.0 * before * after / (before + after);
}

/* A heading leaning no nearer to a tile side than a bend can keep within the tiles, keeping the side it leans toward. */
static MapVector Crossing(const MapVector &heading, const MapVector &upward)
{
	double across = Dot(heading, upward);
	if (across >= LEAST_CROSSING) return heading;
	MapVector side = RightOf(upward);
	double lean = Dot(heading, side) >= 0.0 ? 1.0 : -1.0;
	return upward * LEAST_CROSSING + side * (lean * std::sqrt(1.0 - LEAST_CROSSING * LEAST_CROSSING));
}

/* A run of way through joints that ease, traced whole from one of them to the joints that hold at either end, or round a loop back to it, and eased all at once:
 * its levels are pulled toward the straight line between their neighbours yet kept on or a little above the ground, and a rail run's joints
 * slide along their tile sides toward that line too, a step for each joint of reach. */
class Run {
public:
	explicit Run(const Joint &start);

	template <typename Store>
	void Ease(Store store);

private:
	/* A joint of the run: where it lies along the run, the ground under it, and whether it holds its place and level. */
	struct Node {
		Joint joint;
		double place;
		double ground;
		bool held;
	};

	std::optional<double> Walk(Joint at, TileCoord tile, double direction, std::vector<Node> &nodes) const;
	std::vector<double> EasedLevels() const;
	std::vector<MapVector> EasedPlaces() const;
	size_t Before(size_t index) const;
	size_t After(size_t index) const;
	double Gap(size_t from, size_t to) const;
	double Share(size_t index) const;

	std::vector<Node> nodes;
	WayKind kind;
	/* How far round the run is, where it closes on itself. */
	std::optional<double> loop;
};

/* The joints a run passes stepping from one joint into a tile beside it, until a joint that does not ease, which holds;
 * a run closing on itself ends where it began, and tells how far round it is. */
std::optional<double> Run::Walk(Joint at, TileCoord tile, double direction, std::vector<Node> &walked) const
{
	Joint start = at;
	double place = 0.0;
	for (int step = 0; step < LONGEST_WALK; step++) {
		std::optional<Piece> piece = WaysOn(tile.first, tile.second).Touching(at);
		if (!piece.has_value()) break;
		Joint next = piece->Other(at);
		place += piece->Span() * direction;
		if (next == start) return std::abs(place);
		if (!Eases(next)) {
			walked.push_back({next, place, GroundAt(tile, next), true});
			return std::nullopt;
		}
		auto [low, high] = next.Tiles();
		walked.push_back({next, place, std::max(GroundAt(low, next), GroundAt(high, next)), false});
		tile = next.Beyond(tile);
		at = next;
	}
	if (!walked.empty()) walked.back().held = true;
	return std::nullopt;
}

Run::Run(const Joint &start) : kind(WaysOn(start.Tiles()[0].first, start.Tiles()[0].second).kind)
{
	auto [low, high] = start.Tiles();
	std::vector<Node> highs;
	this->loop = this->Walk(start, high, 1.0, highs);
	std::vector<Node> lows;
	if (!this->loop.has_value()) this->Walk(start, low, -1.0, lows);

	this->nodes.assign(lows.rbegin(), lows.rend());
	this->nodes.push_back({start, 0.0, std::max(GroundAt(low, start), GroundAt(high, start)), false});
	this->nodes.insert(this->nodes.end(), highs.begin(), highs.end());
}

/* A joint's neighbours along the run, round the joint a loop closes at; the ends of a run that does not close hold, so are never asked about. */
size_t Run::Before(size_t index) const
{
	return (index == 0 ? this->nodes.size() : index) - 1;
}

size_t Run::After(size_t index) const
{
	return (index + 1) % this->nodes.size();
}

/* How far along the run from one joint on to another. */
double Run::Gap(size_t from, size_t to) const
{
	double gap = this->nodes[to].place - this->nodes[from].place;
	return gap < 0.0 ? gap + this->loop.value_or(0.0) : gap;
}

/* How far between its neighbours a joint lies along the run. */
double Run::Share(size_t index) const
{
	double before = this->Gap(this->Before(index), index);
	return before / (before + this->Gap(index, this->After(index)));
}

std::vector<double> Run::EasedLevels() const
{
	std::vector<double> levels(this->nodes.size());
	std::ranges::transform(this->nodes, levels.begin(), &Node::ground);
	std::vector<double> next = levels;
	for (int pass = 0; pass < WAY_EASE_REACH; pass++) {
		for (size_t index = 0; index < this->nodes.size(); index++) {
			const Node &node = this->nodes[index];
			if (node.held) continue;
			double line = std::lerp(levels[this->Before(index)], levels[this->After(index)], this->Share(index));
			double pulled = levels[index] + (line - levels[index]) * EASE_PULL;
			next[index] = std::clamp(pulled, node.ground, node.ground + LIFT_LIMIT);
		}
		std::swap(levels, next);
		next = levels;
	}
	return levels;
}

std::vector<MapVector> Run::EasedPlaces() const
{
	std::vector<MapVector> places(this->nodes.size());
	std::ranges::transform(this->nodes, places.begin(), [](const Node &node) { return node.joint.Point(); });
	if (this->kind != WayKind::Rail) return places;

	std::vector<double> limits(this->nodes.size(), 0.0);
	for (size_t index = 0; index < this->nodes.size(); index++) {
		if (this->nodes[index].held) continue;
		MapVector course = Unit(places[this->After(index)] - places[this->Before(index)]);
		limits[index] = std::min(DRIFT_LIMIT / std::abs(Dot(course, this->nodes[index].joint.Upward())), HALF_TILE - CORNER_MARGIN);
	}
	std::vector<double> slides(this->nodes.size(), 0.0);
	std::vector<double> next = slides;
	for (int pass = 0; pass < WAY_EASE_REACH; pass++) {
		for (size_t index = 0; index < this->nodes.size(); index++) {
			const Node &node = this->nodes[index];
			if (node.held) continue;
			const MapVector &before = places[this->Before(index)];
			MapVector line = before + (places[this->After(index)] - before) * this->Share(index);
			double toward = Dot(line - node.joint.Point(), node.joint.Side());
			next[index] = std::clamp(slides[index] + (toward - slides[index]) * SLIDE_PULL, -limits[index], limits[index]);
		}
		std::swap(slides, next);
		next = slides;
		for (size_t index = 0; index < this->nodes.size(); index++) places[index] = this->nodes[index].joint.Point() + this->nodes[index].joint.Side() * slides[index];
	}
	return places;
}

/* Every joint of the run is stored with its ease, told the way round the joint's own tiles see it, or with none where it holds. */
template <typename Store>
void Run::Ease(Store store)
{
	std::vector<double> levels = this->EasedLevels();
	std::vector<MapVector> places = this->EasedPlaces();
	bool from_front = this->nodes.front().joint.Key() <= this->nodes.back().joint.Key();
	double origin = from_front ? this->nodes.front().place : this->nodes.back().place;
	auto climb = [&](size_t a, size_t b) { return (levels[b] - levels[a]) / this->Gap(a, b); };
	for (size_t index = 0; index < this->nodes.size(); index++) {
		const Node &node = this->nodes[index];
		if (node.held) {
			store(node.joint, std::nullopt);
			continue;
		}
		size_t before = this->Before(index);
		size_t after = this->After(index);
		bool upward = this->nodes[after].joint.Joins(node.joint.Tiles()[1]);
		double sign = upward ? 1.0 : -1.0;
		MapVector heading = this->kind == WayKind::Rail ? Crossing(Unit(places[after] - places[before]) * sign, node.joint.Upward()) : node.joint.Upward();
		double slope = JointSlope(climb(before, index), climb(index, after)) * sign;
		std::optional<double> distance;
		if (!this->loop.has_value() || index > 0) distance = std::abs(node.place - origin);
		store(node.joint, JointEase{places[index], levels[index], slope, heading, distance, upward == from_front});
	}
}

/* Eases are worked out a whole run at a time, and kept until the ground's shape or the ways change within their reach. */
class EaseBook {
public:
	std::optional<JointEase> At(const Joint &joint)
	{
		auto found = this->eases.find(joint.Key());
		if (found != this->eases.end()) return found->second;
		if (!::Eases(joint)) {
			this->eases.emplace(joint.Key(), std::nullopt);
			return std::nullopt;
		}
		_frame_profile.Count("ease_runs");
		Run(joint).Ease([this](const Joint &eased, const std::optional<JointEase> &ease) { this->eases.insert_or_assign(eased.Key(), ease); });
		return this->eases[joint.Key()];
	}

	void Forget(const WorldChanges &changes)
	{
		if (changes.whole) {
			this->eases.clear();
			return;
		}
		for (const Rect &relief : changes.Of(ChangeKind::Relief)) {
			Rect tiles = TilesNear(relief, WAY_EASE_REACH);
			for (int hy = 2 * tiles.top; hy <= 2 * tiles.bottom + 2; hy++) {
				for (int hx = 2 * tiles.left + 1 - hy % 2; hx <= 2 * tiles.right + 2; hx += 2) this->eases.erase(Joint{hx, hy}.Key());
			}
		}
	}

private:
	std::unordered_map<uint64_t, std::optional<JointEase>> eases;
};

static EaseBook _ease_book;

WayCourse::WayCourse(int tx, int ty, const Piece &piece, bool road) :
	tx(tx), ty(ty), from(piece.from.Point()), to(piece.to.Point()), road(road), line(this->from, this->to, this->to - this->from, this->to - this->from)
{
	CourseEnd first = this->EndAt(this->from, true);
	CourseEnd last = this->EndAt(this->to, false);
	this->line = Bend(first.at, last.at, first.heading, last.heading);
	this->start_level = first.level;
	this->end_level = last.level;
	double chord = (last.level - first.level) / this->Length();
	this->start_slope = first.slope.value_or(chord);
	this->end_slope = last.slope.value_or(chord);
}

/* Whether the course runs across the joint at one of its ends toward the joint's higher tile. */
bool WayCourse::Onward(const Joint &joint, bool leaving) const
{
	return (joint.Tiles()[1] == TileCoord{this->tx, this->ty}) == leaving;
}

/* An end where the run eases takes the joint's place, level, climb and heading along the course; one that holds keeps to the middle of the tile side
 * and the tile's ground there, climbing straight at the other end and heading on as the way beyond does. */
WayCourse::CourseEnd WayCourse::EndAt(const MapVector &point, bool leaving) const
{
	Joint joint = Joint::At(point);
	std::optional<JointEase> ease = _ease_book.At(joint);
	if (!ease.has_value()) return {point, GroundAt({this->tx, this->ty}, joint), std::nullopt, this->HeldHeading(point, leaving)};

	double sign = this->Onward(joint, leaving) ? 1.0 : -1.0;
	return {ease->at, ease->level, ease->slope * sign, ease->heading * sign};
}

std::optional<WayCourse::RunMark> WayCourse::MarkAt(const MapVector &point, bool leaving) const
{
	Joint joint = Joint::At(point);
	std::optional<JointEase> ease = _ease_book.At(joint);
	if (!ease.has_value() || !ease->distance.has_value()) return std::nullopt;
	return RunMark{*ease->distance, ease->rising == this->Onward(joint, leaving) ? 1.0 : -1.0};
}

std::optional<std::pair<double, double>> WayCourse::Distances() const
{
	std::optional<RunMark> first = this->MarkAt(this->from, true);
	std::optional<RunMark> last = this->MarkAt(this->to, false);
	if (!first.has_value() && !last.has_value()) return std::nullopt;
	double start = first.has_value() ? first->distance : last->distance - last->growth * this->Length();
	double end = last.has_value() ? last->distance : first->distance + first->growth * this->Length();
	return std::pair{start, end};
}

/* A road heads square across the tile side; a rail heads on along whichever piece beyond the side runs on most nearly straight, or along its own piece where none does. */
MapVector WayCourse::HeldHeading(const MapVector &point, bool leaving) const
{
	Joint joint = Joint::At(point);
	MapVector outward = joint.Across(this->tx, this->ty);
	MapVector along = leaving ? outward * -1.0 : outward;
	if (this->road) return along;

	MapVector chord = Unit(this->to - this->from);
	std::optional<Piece> beyond = StraightestAt(joint.Beyond({this->tx, this->ty}), joint, leaving ? chord * -1.0 : chord);
	if (!beyond.has_value()) return chord;
	MapVector onward = Unit(beyond->Other(joint).Point() - point);
	return leaving ? onward * -1.0 : onward;
}

std::optional<WayCourse> WayCourse::OfTrack(int tx, int ty, Track track)
{
	TileWays ways = WaysOn(tx, ty);
	if (ways.kind != WayKind::Rail || !HasBit(_world_tiles.NetworkAt(TileXY(tx, ty)).track, track)) return std::nullopt;
	return WayCourse(tx, ty, PieceOf(tx, ty, track), false);
}

std::optional<WayCourse> WayCourse::OfRoad(int tx, int ty)
{
	TileWays ways = WaysOn(tx, ty);
	if (ways.kind != WayKind::Road) return std::nullopt;
	return WayCourse(tx, ty, ways.pieces[0], true);
}

MapVector WayCourse::OnwardFrom(const MapVector &end) const
{
	MapVector first = this->line.Centre(0.0);
	MapVector last = this->line.Centre(1.0);
	MapVector to_first = end - first;
	MapVector to_last = end - last;
	return Dot(to_first, to_first) <= Dot(to_last, to_last) ? this->Before() : this->After() * -1.0;
}

double WayCourse::Length() const
{
	return std::hypot(this->to.x - this->from.x, this->to.y - this->from.y);
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

/* How a rail piece's end meets the track beyond: the way that track runs there, along the piece, where one piece alone carries it on at the piece's level;
 * whether none does; and whether the piece is a branch of a junction, one of several of its tile's meeting there.
 * The track beyond runs on along an eased course as that course heads, or else along its own piece. */
struct PieceEnd {
	std::optional<MapVector> run;
	bool open;
	bool branch;
};

static int PiecesMeeting(int tx, int ty, const Joint &joint)
{
	int meeting = 0;
	for (Track track : SetTrackBitIterator(TrackBitsOf(tx, ty))) meeting += PieceOf(tx, ty, track).Meets(joint) ? 1 : 0;
	return meeting;
}

static PieceEnd PieceEndAt(int tx, int ty, const MapVector &joint, const MapVector &outward)
{
	MapVector probe = joint + outward * HALF_TILE;
	int nx = static_cast<int>(std::floor(probe.x));
	int ny = static_cast<int>(std::floor(probe.y));
	if (!OnMap(nx, ny) || TileGround(tx, ty).Level(joint.x, joint.y) != TileGround(nx, ny).Level(joint.x, joint.y)) return {std::nullopt, true, false};

	Joint at = Joint::At(joint);
	PieceEnd end{std::nullopt, true, PiecesMeeting(tx, ty, at) > 1};
	int partners = 0;
	for (Track track : SetTrackBitIterator(TrackBitsOf(nx, ny))) {
		Piece piece = PieceOf(nx, ny, track);
		if (!piece.Meets(at)) continue;
		std::optional<WayCourse> course = WayCourse::OfTrack(nx, ny, track);
		end.run = course.has_value() ? course->OnwardFrom(joint) : Unit(piece.Other(at).Point() - joint);
		end.open = false;
		partners++;
	}
	if (partners > 1) end.run.reset();
	return end;
}

/* A piece off any course leans at an end halfway toward the way the track runs on beyond, which leans as far toward it, so the two meet on one heading;
 * a branch of a junction leaves along the way the track beyond runs, as that track keeps its heading toward the branches, so every branch meets it on one heading.
 * The track beyond the first end runs on away from the piece, so it is turned about to lean along it. */
static MapVector Leaning(const MapVector &own, const PieceEnd &end, bool first)
{
	if (!end.run.has_value()) return own;
	MapVector beyond = *end.run * (first ? -1.0 : 1.0);
	return end.branch ? beyond : Unit(own + beyond);
}

DrawnTrack DrawnTrack::Build(int tx, int ty, Track track)
{
	Piece piece = PieceOf(tx, ty, track);
	MapVector from = piece.from.Point();
	MapVector to = piece.to.Point();
	MapVector own = Unit(to - from);
	PieceEnd first = PieceEndAt(tx, ty, from, own * -1.0);
	PieceEnd last = PieceEndAt(tx, ty, to, own);
	std::optional<WayCourse> course = WayCourse::OfTrack(tx, ty, track);
	if (course.has_value()) return {tx, ty, from, to, course->Line(), course, first.open, last.open};
	return {tx, ty, from, to, Bend(from, to, Leaning(own, first, true), Leaning(own, last, false)), std::nullopt, first.open, last.open};
}

/* Drawn pieces are worked out once and kept until the ground's shape or the ways change within reach of the eases they read,
 * as every unit of every train asks for its own each frame. */
class TrackBook {
public:
	const DrawnTrack &At(int tx, int ty, Track track)
	{
		std::vector<std::pair<Track, DrawnTrack>> &drawn = this->tiles[TileXY(tx, ty).base()];
		auto found = std::ranges::find(drawn, track, &std::pair<Track, DrawnTrack>::first);
		if (found != drawn.end()) return found->second;
		_frame_profile.Count("track_builds");
		return drawn.emplace_back(track, DrawnTrack::Build(tx, ty, track)).second;
	}

	void Forget(const WorldChanges &changes)
	{
		if (changes.whole) {
			this->tiles.clear();
			return;
		}
		for (const Rect &relief : changes.Of(ChangeKind::Relief)) {
			Rect tiles = TilesNear(relief, WAY_EASE_REACH + DRAWN_READ_REACH);
			for (int ty = tiles.top; ty <= tiles.bottom; ty++) {
				for (int tx = tiles.left; tx <= tiles.right; tx++) this->tiles.erase(TileXY(tx, ty).base());
			}
		}
	}

private:
	std::unordered_map<uint32_t, std::vector<std::pair<Track, DrawnTrack>>> tiles;
};

static TrackBook _track_book;

DrawnTrack DrawnTrack::Of(int tx, int ty, Track track)
{
	return _track_book.At(tx, ty, track);
}

/* A point keeps its share along the game's straight piece. */
double DrawnTrack::ShareOf(const MapVector &point) const
{
	return ShareAlong(point, this->from, this->to);
}

WorldPoint DrawnTrack::At(double share, double lateral) const
{
	MapVector at = this->line.At(share, lateral);
	double level = this->course.has_value() ? this->course->Level(share) : TileGround(this->tx, this->ty).Level(at.x, at.y);
	return {at.x, at.y, level};
}

WorldPoint DrawnTrack::Placed(const MapVector &point) const
{
	return this->At(this->ShareOf(point), Dot(point - this->from, RightOf(Unit(this->to - this->from))));
}

WorldPoint DrawnTrack::Centred(const MapVector &point) const
{
	return this->At(this->ShareOf(point), 0.0);
}

/* The drawn piece a point of the game's track lies on, unless the tile under it has no track. */
static std::optional<DrawnTrack> DrawnUnder(const MapVector &point)
{
	TileCoord tile = TileOf(point);
	std::optional<Track> track = TrackUnder(tile, point);
	if (!track.has_value()) return std::nullopt;
	return DrawnTrack::Of(tile.first, tile.second, *track);
}

WorldPoint TrackPoint(double x, double y)
{
	std::optional<DrawnTrack> drawn = DrawnUnder({x, y});
	return drawn.has_value() ? drawn->Placed({x, y}) : WorldPoint{x, y, GroundLevel(x, y)};
}

WorldPoint TrainPoint(double x, double y)
{
	std::optional<DrawnTrack> drawn = DrawnUnder({x, y});
	return drawn.has_value() ? drawn->Centred({x, y}) : WorldPoint{x, y, GroundLevel(x, y)};
}

/* A train runs on into a tile along the straightest piece it is known to take there, or else along the straightest of all. */
static std::optional<Piece> OnwardAt(const TileCoord &tile, const Joint &joint, const MapVector &heading, const TrackTaken &taken)
{
	if (!OnMap(tile.first, tile.second)) return std::nullopt;
	std::optional<Piece> known = StraightestAt(tile, joint, heading, taken(TileXY(tile.first, tile.second)));
	return known.has_value() ? known : StraightestAt(tile, joint, heading);
}

MapVector TrainAhead(const MapVector &from, MapVector heading, double reach, const TrackTaken &taken)
{
	TileCoord tile = TileOf(from);
	std::optional<Track> under = TrackUnder(tile, from);
	if (!under.has_value()) return from + heading * reach;

	std::optional<Piece> piece = PieceOf(tile.first, tile.second, *under);
	MapVector at = from;
	for (int step = 0; step < LONGEST_WALK && piece.has_value(); step++) {
		Joint exit = Dot(piece->to.Point() - piece->from.Point(), heading) >= 0.0 ? piece->to : piece->from;
		heading = Unit(exit.Point() - piece->Other(exit).Point());
		double left = std::max(Dot(exit.Point() - at, heading), 0.0);
		if (reach <= left) return at + heading * reach;
		reach -= left;
		at = exit.Point();
		tile = exit.Beyond(tile);
		piece = OnwardAt(tile, exit, heading, taken);
	}
	return at + heading * reach;
}

TrackRoute::TrackRoute(const MapVector &from, const MapVector &to)
{
	this->points[this->count++] = from;
	if (!this->Cross(from, to)) this->count = 1;
	this->points[this->count++] = to;
}

/* From the piece a train starts on it leaves by the end nearer where it is bound, and on through each tile by the piece leading nearest there. */
bool TrackRoute::Cross(const MapVector &from, const MapVector &to)
{
	TileCoord tile = TileOf(from);
	TileCoord bound = TileOf(to);
	if (tile == bound) return true;
	std::optional<Track> under = TrackUnder(tile, from);
	if (!under.has_value()) return false;

	auto nearness = [&to](const Joint &joint) { return -Distance(joint.Point(), to); };
	Piece piece = PieceOf(tile.first, tile.second, *under);
	Joint exit = nearness(piece.from) >= nearness(piece.to) ? piece.from : piece.to;
	for (size_t side = 0; side < MOST_SIDES; side++) {
		this->points[this->count++] = exit.Point();
		tile = exit.Beyond(tile);
		if (tile == bound) return true;
		std::optional<Piece> next = BestPieceAt(tile, exit, [&](const Piece &onward) { return nearness(onward.Other(exit)); });
		if (!next.has_value()) return false;
		exit = next->Other(exit);
	}
	return false;
}

std::span<const MapVector> TrackRoute::Sides() const
{
	return std::span(this->points).subspan(1, this->count - 2);
}

MapVector TrackRoute::At(double share) const
{
	std::span<const MapVector> path = std::span(this->points).first(this->count);
	double length = 0.0;
	for (size_t point = 1; point < path.size(); point++) length += Distance(path[point - 1], path[point]);
	double left = share * length;
	for (size_t point = 1; point < path.size(); point++) {
		double stretch = Distance(path[point - 1], path[point]);
		if (left <= stretch || point + 1 == path.size()) return path[point - 1] + (path[point] - path[point - 1]) * (stretch > 0.0 ? left / stretch : 0.0);
		left -= stretch;
	}
	return path.front();
}

WorldPoint RoadPoint(double x, double y)
{
	std::optional<WayCourse> course = WayCourse::OfRoad(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)));
	if (!course.has_value()) return {x, y, GroundLevel(x, y)};
	return {x, y, course->Level(course->ShareAt({x, y}))};
}

static double SlideAt(const MapVector &middle)
{
	Joint joint = Joint::At(middle);
	std::optional<JointEase> ease = _ease_book.At(joint);
	return ease.has_value() ? Dot(ease->at - middle, joint.Side()) : 0.0;
}

std::pair<double, double> SideSlides(int tx, int ty)
{
	return {SlideAt({static_cast<double>(tx), ty + HALF_TILE}), SlideAt({tx + HALF_TILE, static_cast<double>(ty)})};
}

void ForgetCourses(const WorldChanges &changes)
{
	_ease_book.Forget(changes);
	_track_book.Forget(changes);
}
