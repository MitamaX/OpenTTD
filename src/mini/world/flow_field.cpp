/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file flow_field.cpp Which way and how fast the water runs on every tile: rivers down toward the sea, canals gently, and a river on out past its mouth. */

#include "../../stdafx.h"
#include "flow_field.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <queue>
#include <tuple>
#include <unordered_set>

#include "../../direction_func.h"
#include "../../map_func.h"
#include "../map/tile_shapes.h"
#include "../map/world_tiles.h"
#include "seabed.h"

#include "../../safeguards.h"

static constexpr double RAPIDS_PACE = 1.0;
static constexpr double RIVER_PACE = 0.45;
static constexpr double CANAL_PACE = 0.12;
/* Water runs at full pace with at most this many tiles of water about it, slowing to this share of it with water all round. */
static constexpr int NARROW_WATER = 4;
static constexpr int ALL_ROUND = 9;
static constexpr double WIDE_WATER_PACE = 0.3;
static constexpr int MOUTH_REACH = 4;
/* A river's fan reaches this far from the last tile of its running water, which its mouth lies beside. */
static constexpr int FAN_REACH = MOUTH_REACH + 1;

static constexpr uint8_t STILL_CHANNEL = static_cast<uint8_t>(FLOW_TEXEL_STILL);
static constexpr FlowTexel STILL = {STILL_CHANNEL, STILL_CHANNEL};
static constexpr std::array<DiagDirection, DIAGDIR_END> STEPS = {DIAGDIR_SW, DIAGDIR_SE, DIAGDIR_NE, DIAGDIR_NW};
static constexpr DiagDirection UNREACHED = INVALID_DIAGDIR;
static constexpr DiagDirection POOLED = DIAGDIR_END;

static_assert(sizeof(FlowTexel) == std::tuple_size_v<FlowTexel> * sizeof(uint8_t));

enum class Stream : uint8_t {
	None,
	River,
	Canal,
};

template <typename Visit>
static void EachTile(const Rect &span, Visit visit)
{
	for (int ty = span.top; ty <= span.bottom; ty++) {
		for (int tx = span.left; tx <= span.right; tx++) visit(tx, ty);
	}
}

static std::pair<int, int> Beside(TileIndex tile, DiagDirection step)
{
	TileIndexDiffC offset = TileIndexDiffCByDiagDir(step);
	return {static_cast<int>(TileX(tile)) + offset.x, static_cast<int>(TileY(tile)) + offset.y};
}

static Stream StreamOf(int tx, int ty)
{
	if (!OnMap(tx, ty)) return Stream::None;
	WaterTexel water = _world_tiles.WaterAt(TileXY(tx, ty));
	if (water.level == 0 || water.sea != 0) return Stream::None;
	return water.canal != 0 ? Stream::Canal : Stream::River;
}

static bool IsSea(int tx, int ty)
{
	return OnMap(tx, ty) && _world_tiles.WaterAt(TileXY(tx, ty)).sea != 0;
}

static bool IsOutlet(int tx, int ty)
{
	return !OnMap(tx, ty) || IsSea(tx, ty);
}

static double LevelOf(TileIndex tile)
{
	return SurfaceLevelOf(TileX(tile), TileY(tile));
}

/* The share of full pace water keeps where it spreads wide. */
static double Narrowing(int tx, int ty)
{
	int water = 0;
	for (int ny = ty - 1; ny <= ty + 1; ny++) {
		for (int nx = tx - 1; nx <= tx + 1; nx++) {
			if (OnMap(nx, ny) && _world_tiles.WaterAt(TileXY(nx, ny)).level != 0) water++;
		}
	}
	double spread = std::clamp(static_cast<double>(water - NARROW_WATER) / (ALL_ROUND - NARROW_WATER), 0.0, 1.0);
	return std::lerp(1.0, WIDE_WATER_PACE, spread);
}

static double PaceOf(int tx, int ty, bool sloping)
{
	if (StreamOf(tx, ty) == Stream::Canal) return CANAL_PACE * Narrowing(tx, ty);
	return sloping ? RAPIDS_PACE : RIVER_PACE * Narrowing(tx, ty);
}

/* The running water joined to a tile of it, in map order. */
static std::vector<TileIndex> BodyFrom(TileIndex first, std::unordered_set<TileIndex> &seen)
{
	std::vector<TileIndex> body = {first};
	seen.insert(first);
	for (size_t next = 0; next < body.size(); next++) {
		for (DiagDirection step : STEPS) {
			auto [nx, ny] = Beside(body[next], step);
			if (StreamOf(nx, ny) == Stream::None) continue;
			TileIndex tile = TileXY(nx, ny);
			if (seen.insert(tile).second) body.push_back(tile);
		}
	}
	std::ranges::sort(body);
	return body;
}

/* Each body of running water joined to any near the changes, which drains alone as it would among them all, as no water runs from one into another. */
static std::vector<std::vector<TileIndex>> BodiesAbout(std::span<const Rect> changed)
{
	std::vector<std::vector<TileIndex>> bodies;
	std::unordered_set<TileIndex> seen;
	for (const Rect &span : changed) {
		EachTile(TilesNear(span, 1), [&](int tx, int ty) {
			if (StreamOf(tx, ty) != Stream::None && !seen.contains(TileXY(tx, ty))) bodies.push_back(BodyFrom(TileXY(tx, ty), seen));
		});
	}
	return bodies;
}

/* Which neighbour each tile of running water runs on into, or that it pools, learnt by flooding up from where the water leaves, the lowest water first and the nearest the way out first. */
class Drainage {
public:
	explicit Drainage(std::vector<DiagDirection> &downstream) : downstream(downstream) {}

	/* Ties fall to the tile first in map order, so the water must come in map order to drain as it does over the whole map. */
	void Drain(std::span<const TileIndex> running);
	void Forget(TileIndex tile) { this->downstream[tile.base()] = UNREACHED; }
	MapVector Flow(TileIndex tile) const;
	std::optional<TileIndex> MouthOf(TileIndex tile) const;

private:
	struct Front {
		double level;
		uint32_t order;
		TileIndex tile;

		bool operator>(const Front &other) const { return std::tie(this->level, this->order) > std::tie(other.level, other.order); }
	};

	void Reach(TileIndex tile, double level, DiagDirection toward);
	void Flood();
	MapVector Course(TileIndex tile) const;

	std::vector<DiagDirection> &downstream;
	std::priority_queue<Front, std::vector<Front>, std::greater<>> front;
	uint32_t order = 0;
};

void Drainage::Drain(std::span<const TileIndex> running)
{
	for (TileIndex tile : running) {
		auto outlet = std::ranges::find_if(STEPS, [tile](DiagDirection step) { return std::apply(IsOutlet, Beside(tile, step)); });
		if (outlet != STEPS.end()) this->Reach(tile, LevelOf(tile), *outlet);
	}
	this->Flood();

	std::vector<TileIndex> stranded;
	std::ranges::copy_if(running, std::back_inserter(stranded), [this](TileIndex tile) { return this->downstream[tile.base()] == UNREACHED; });
	std::ranges::stable_sort(stranded, {}, LevelOf);
	for (TileIndex tile : stranded) {
		if (this->downstream[tile.base()] != UNREACHED) continue;
		this->Reach(tile, LevelOf(tile), POOLED);
		this->Flood();
	}
}

void Drainage::Reach(TileIndex tile, double level, DiagDirection toward)
{
	this->downstream[tile.base()] = toward;
	this->front.push({level, this->order++, tile});
}

void Drainage::Flood()
{
	while (!this->front.empty()) {
		Front at = this->front.top();
		this->front.pop();
		for (DiagDirection step : STEPS) {
			auto [nx, ny] = Beside(at.tile, step);
			if (StreamOf(nx, ny) == Stream::None) continue;
			TileIndex next = TileXY(nx, ny);
			if (this->downstream[next.base()] != UNREACHED) continue;
			this->Reach(next, std::max(at.level, LevelOf(next)), ReverseDiagDir(step));
		}
	}
}

/* Between the way the water comes in and the way it goes on, so a bend turns through its tile instead of at the tile's side. */
MapVector Drainage::Course(TileIndex tile) const
{
	DiagDirection toward = this->downstream[tile.base()];
	MapVector out = toward == POOLED ? MapVector{} : Outward(toward);
	MapVector in{};
	for (DiagDirection step : STEPS) {
		auto [nx, ny] = Beside(tile, step);
		DiagDirection back = ReverseDiagDir(step);
		if (OnMap(nx, ny) && this->downstream[TileXY(nx, ny).base()] == back) in = in + Outward(back);
	}
	return Unit(out + Unit(in));
}

MapVector Drainage::Flow(TileIndex tile) const
{
	bool sloping = WaterFormOf(TileX(tile), TileY(tile)) == WaterForm::Incline;
	MapVector course = sloping ? Downhill(TileX(tile), TileY(tile)) : this->Course(tile);
	return course * PaceOf(TileX(tile), TileY(tile), sloping);
}

std::optional<TileIndex> Drainage::MouthOf(TileIndex tile) const
{
	DiagDirection toward = this->downstream[tile.base()];
	if (!IsValidDiagDirection(toward)) return std::nullopt;
	auto [nx, ny] = Beside(tile, toward);
	return IsSea(nx, ny) ? std::optional<TileIndex>(TileXY(nx, ny)) : std::nullopt;
}

static uint8_t Channel(double run)
{
	return static_cast<uint8_t>(std::lround(FLOW_TEXEL_STILL + std::clamp(run, -1.0, 1.0) * FLOW_TEXEL_SPAN));
}

static FlowTexel TexelOf(MapVector flow)
{
	return {Channel(flow.x), Channel(flow.y)};
}

static double SquaredPace(const FlowTexel &texel)
{
	MapVector flow = MapVector{texel[0] - FLOW_TEXEL_STILL, texel[1] - FLOW_TEXEL_STILL} * (1.0 / FLOW_TEXEL_SPAN);
	return Dot(flow, flow);
}

/* The texels over a window of the map, still until fanned out past a mouth. */
class FanPatch {
public:
	explicit FanPatch(const Rect &window) : window(window), texels(static_cast<size_t>(window.Width()) * window.Height(), STILL) {}

	const FlowTexel &At(int tx, int ty) const { return this->texels[this->IndexOf(tx, ty)]; }
	void Fan(TileIndex mouth, MapVector flow);

private:
	size_t IndexOf(int tx, int ty) const { return static_cast<size_t>(ty - this->window.top) * this->window.Width() + (tx - this->window.left); }

	Rect window;
	std::vector<FlowTexel> texels;
};

/* A river runs on into the sea past its mouth, fanning out and slowing until it is lost a few tiles out; where fans meet, the faster holds. */
void FanPatch::Fan(TileIndex mouth, MapVector flow)
{
	MapVector course = Unit(flow);
	double pace = std::sqrt(Dot(flow, flow));
	for (int dy = -MOUTH_REACH; dy <= MOUTH_REACH; dy++) {
		for (int dx = -MOUTH_REACH; dx <= MOUTH_REACH; dx++) {
			int tx = static_cast<int>(TileX(mouth)) + dx;
			int ty = static_cast<int>(TileY(mouth)) + dy;
			int reach = std::abs(dx) + std::abs(dy);
			MapVector offset = {static_cast<double>(dx), static_cast<double>(dy)};
			if (reach > MOUTH_REACH || Dot(offset, course) < 0.0 || !this->window.Contains({tx, ty}) || !IsSea(tx, ty)) continue;
			MapVector fanned = Unit(offset + course) * (pace * (1.0 - reach / (MOUTH_REACH + 1.0)));
			FlowTexel &texel = this->texels[this->IndexOf(tx, ty)];
			if (Dot(fanned, fanned) > SquaredPace(texel)) texel = TexelOf(fanned);
		}
	}
}

static Rect FanWindow(TileIndex mouth)
{
	int tx = TileX(mouth);
	int ty = TileY(mouth);
	return TilesNear({tx, ty, tx, ty}, MOUTH_REACH);
}

void FlowField::Survey(Dimension map)
{
	size_t tiles = static_cast<size_t>(map.width) * map.height;
	this->texels.assign(tiles, STILL);
	this->downstream.assign(tiles, UNREACHED);
	this->Resurvey(std::array{Rect{0, 0, static_cast<int>(map.width) - 1, static_cast<int>(map.height) - 1}});
}

/* The spans of texels the change moved, one about each body of water drained again and each stretch of sea fanned again. */
std::vector<Rect> FlowField::Resurvey(std::span<const Rect> changed)
{
	Drainage drainage(this->downstream);
	std::vector<std::vector<TileIndex>> bodies = BodiesAbout(changed);
	std::vector<Rect> windows;
	auto note_mouth = [&](TileIndex tile) {
		if (std::optional<TileIndex> mouth = drainage.MouthOf(tile); mouth.has_value()) windows.push_back(FanWindow(*mouth));
	};
	auto forget = [&](TileIndex tile) {
		note_mouth(tile);
		drainage.Forget(tile);
	};
	for (const Rect &span : changed) {
		windows.push_back(TilesNear(span, MOUTH_REACH));
		EachTile(span, [&](int tx, int ty) { forget(TileXY(tx, ty)); });
	}
	for (const std::vector<TileIndex> &body : bodies) std::ranges::for_each(body, forget);

	std::vector<Rect> moved;
	auto gather = [&moved](const std::optional<Rect> &span) {
		if (span.has_value()) moved.push_back(*span);
	};
	for (const std::vector<TileIndex> &body : bodies) {
		drainage.Drain(body);
		std::optional<Rect> span;
		for (TileIndex tile : body) {
			note_mouth(tile);
			this->Put(tile, TexelOf(drainage.Flow(tile)), span);
		}
		gather(span);
	}
	for (const Rect &window : windows) {
		std::optional<Rect> span;
		this->Refan(drainage, window, span);
		gather(span);
	}
	return moved;
}

/* Every texel of a window off the running water afresh, fanned out past each mouth within reach in map order, as a whole survey fans them. */
void FlowField::Refan(const Drainage &drainage, const Rect &window, std::optional<Rect> &moved)
{
	FanPatch patch(window);
	EachTile(TilesNear(window, FAN_REACH), [&](int tx, int ty) {
		TileIndex tile = TileXY(tx, ty);
		if (std::optional<TileIndex> mouth = drainage.MouthOf(tile); mouth.has_value()) patch.Fan(*mouth, drainage.Flow(tile));
	});
	EachTile(window, [&](int tx, int ty) {
		if (StreamOf(tx, ty) == Stream::None) this->Put(TileXY(tx, ty), patch.At(tx, ty), moved);
	});
}

void FlowField::Put(TileIndex tile, const FlowTexel &texel, std::optional<Rect> &moved)
{
	FlowTexel &stored = this->texels[tile.base()];
	if (stored == texel) return;
	stored = texel;
	int tx = TileX(tile);
	int ty = TileY(tile);
	moved = TakingIn(moved.value_or(Rect{tx, ty, tx, ty}), tx, ty);
}
