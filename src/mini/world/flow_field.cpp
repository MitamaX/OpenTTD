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
#include <array>
#include <cmath>
#include <functional>
#include <iterator>
#include <queue>
#include <tuple>

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

static constexpr size_t CHANNELS = 2;
static constexpr uint8_t STILL_TEXEL = static_cast<uint8_t>(FLOW_TEXEL_STILL);
static constexpr int8_t UNREACHED = -1;
static constexpr int8_t POOLED = 4;
static constexpr std::array<std::array<int, 2>, 4> STEPS = {{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}};
static constexpr int8_t STEP_COUNT = static_cast<int8_t>(STEPS.size());

enum class Stream : uint8_t {
	None,
	River,
	Canal,
};

static int8_t Opposite(int8_t step)
{
	return static_cast<int8_t>((step + STEP_COUNT / 2) % STEP_COUNT);
}

static MapVector StepVector(int8_t step)
{
	return {static_cast<double>(STEPS[step][0]), static_cast<double>(STEPS[step][1])};
}

static std::pair<int, int> Beside(TileIndex tile, int8_t step)
{
	return {static_cast<int>(TileX(tile)) + STEPS[step][0], static_cast<int>(TileY(tile)) + STEPS[step][1]};
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

static MapVector Downhill(TileIndex tile)
{
	SurfaceTexel surface = _world_tiles.SurfaceAt(tile);
	double rise_x = (surface.west - surface.north) + (surface.south - surface.east);
	double rise_y = (surface.east - surface.north) + (surface.south - surface.west);
	return Unit({-rise_x, -rise_y});
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

/* Floods up from where running water leaves, the lowest water first and the nearest the way out first, so each tile learns the neighbour it runs on into. */
class Drainage {
public:
	explicit Drainage(Dimension map);

	std::span<const TileIndex> Running() const { return this->running; }
	MapVector Flow(TileIndex tile) const;
	std::optional<TileIndex> MouthOf(TileIndex tile) const;

private:
	struct Front {
		double level;
		uint32_t order;
		TileIndex tile;

		bool operator>(const Front &other) const { return std::tie(this->level, this->order) > std::tie(other.level, other.order); }
	};

	void Reach(TileIndex tile, double level, int8_t toward);
	void Flood();
	MapVector Course(TileIndex tile) const;

	std::vector<int8_t> downstream;
	std::vector<TileIndex> running;
	std::priority_queue<Front, std::vector<Front>, std::greater<>> front;
	uint32_t order = 0;
};

Drainage::Drainage(Dimension map) : downstream(static_cast<size_t>(map.width) * map.height, UNREACHED)
{
	for (int ty = 0; ty < static_cast<int>(map.height); ty++) {
		for (int tx = 0; tx < static_cast<int>(map.width); tx++) {
			if (StreamOf(tx, ty) != Stream::None) this->running.push_back(TileXY(tx, ty));
		}
	}
	for (TileIndex tile : this->running) {
		for (int8_t step = 0; step < STEP_COUNT; step++) {
			auto [nx, ny] = Beside(tile, step);
			if (!IsOutlet(nx, ny)) continue;
			this->Reach(tile, LevelOf(tile), step);
			break;
		}
	}
	this->Flood();

	std::vector<TileIndex> stranded;
	std::ranges::copy_if(this->running, std::back_inserter(stranded), [this](TileIndex tile) { return this->downstream[tile.base()] == UNREACHED; });
	std::ranges::stable_sort(stranded, {}, LevelOf);
	for (TileIndex tile : stranded) {
		if (this->downstream[tile.base()] != UNREACHED) continue;
		this->Reach(tile, LevelOf(tile), POOLED);
		this->Flood();
	}
}

void Drainage::Reach(TileIndex tile, double level, int8_t toward)
{
	this->downstream[tile.base()] = toward;
	this->front.push({level, this->order++, tile});
}

void Drainage::Flood()
{
	while (!this->front.empty()) {
		Front at = this->front.top();
		this->front.pop();
		for (int8_t step = 0; step < STEP_COUNT; step++) {
			auto [nx, ny] = Beside(at.tile, step);
			if (StreamOf(nx, ny) == Stream::None) continue;
			TileIndex next = TileXY(nx, ny);
			if (this->downstream[next.base()] != UNREACHED) continue;
			this->Reach(next, std::max(at.level, LevelOf(next)), Opposite(step));
		}
	}
}

/* Between the way the water comes in and the way it goes on, so a bend turns through its tile instead of at the tile's side. */
MapVector Drainage::Course(TileIndex tile) const
{
	int8_t toward = this->downstream[tile.base()];
	MapVector out = toward == POOLED ? MapVector{} : StepVector(toward);
	MapVector in{};
	for (int8_t step = 0; step < STEP_COUNT; step++) {
		auto [nx, ny] = Beside(tile, step);
		if (OnMap(nx, ny) && this->downstream[TileXY(nx, ny).base()] == Opposite(step)) in = in + StepVector(Opposite(step));
	}
	return Unit(out + Unit(in));
}

MapVector Drainage::Flow(TileIndex tile) const
{
	bool sloping = WaterFormOf(TileX(tile), TileY(tile)) == WaterForm::Incline;
	MapVector course = sloping ? Downhill(tile) : this->Course(tile);
	return course * PaceOf(TileX(tile), TileY(tile), sloping);
}

std::optional<TileIndex> Drainage::MouthOf(TileIndex tile) const
{
	int8_t toward = this->downstream[tile.base()];
	if (toward == POOLED) return std::nullopt;
	auto [nx, ny] = Beside(tile, toward);
	return IsSea(nx, ny) ? std::optional<TileIndex>(TileXY(nx, ny)) : std::nullopt;
}

static uint8_t Channel(double run)
{
	return static_cast<uint8_t>(std::lround(FLOW_TEXEL_STILL + std::clamp(run, -1.0, 1.0) * FLOW_TEXEL_SPAN));
}

static void Store(std::vector<uint8_t> &texels, TileIndex tile, MapVector flow)
{
	size_t at = tile.base() * CHANNELS;
	texels[at] = Channel(flow.x);
	texels[at + 1] = Channel(flow.y);
}

static double SquaredPace(const std::vector<uint8_t> &texels, TileIndex tile)
{
	size_t at = tile.base() * CHANNELS;
	MapVector flow = MapVector{texels[at] - FLOW_TEXEL_STILL, texels[at + 1] - FLOW_TEXEL_STILL} * (1.0 / FLOW_TEXEL_SPAN);
	return Dot(flow, flow);
}

/* A river runs on into the sea past its mouth, fanning out and slowing until it is lost a few tiles out. */
static void Fan(std::vector<uint8_t> &texels, TileIndex mouth, MapVector flow)
{
	MapVector course = Unit(flow);
	double pace = std::sqrt(Dot(flow, flow));
	for (int dy = -MOUTH_REACH; dy <= MOUTH_REACH; dy++) {
		for (int dx = -MOUTH_REACH; dx <= MOUTH_REACH; dx++) {
			int tx = static_cast<int>(TileX(mouth)) + dx;
			int ty = static_cast<int>(TileY(mouth)) + dy;
			int reach = std::abs(dx) + std::abs(dy);
			MapVector offset = {static_cast<double>(dx), static_cast<double>(dy)};
			if (reach > MOUTH_REACH || Dot(offset, course) < 0.0 || !IsSea(tx, ty)) continue;
			MapVector fanned = Unit(offset + course) * (pace * (1.0 - reach / (MOUTH_REACH + 1.0)));
			TileIndex tile = TileXY(tx, ty);
			if (Dot(fanned, fanned) > SquaredPace(texels, tile)) Store(texels, tile, fanned);
		}
	}
}

void FlowField::Survey(Dimension map)
{
	this->map = map;
	this->texels.assign(static_cast<size_t>(map.width) * map.height * CHANNELS, STILL_TEXEL);
	this->Trace();
}

std::optional<Rect> FlowField::Resurvey(std::span<const Rect> changed)
{
	if (std::ranges::none_of(changed, [this](const Rect &span) { return this->Touches(span); })) return std::nullopt;
	return this->Trace();
}

bool FlowField::Touches(const Rect &changed) const
{
	Rect near = TilesNear(changed, 1);
	for (int ty = near.top; ty <= near.bottom; ty++) {
		for (int tx = near.left; tx <= near.right; tx++) {
			size_t at = TileXY(tx, ty).base() * CHANNELS;
			if (StreamOf(tx, ty) != Stream::None || this->texels[at] != STILL_TEXEL || this->texels[at + 1] != STILL_TEXEL) return true;
		}
	}
	return false;
}

/* The span of texels the tracing moved, if it moved any. */
std::optional<Rect> FlowField::Trace()
{
	std::vector<uint8_t> traced(this->texels.size(), STILL_TEXEL);
	Drainage drainage(this->map);
	for (TileIndex tile : drainage.Running()) {
		MapVector flow = drainage.Flow(tile);
		Store(traced, tile, flow);
		if (std::optional<TileIndex> mouth = drainage.MouthOf(tile); mouth.has_value()) Fan(traced, *mouth, flow);
	}

	std::optional<Rect> moved;
	for (int ty = 0; ty < static_cast<int>(this->map.height); ty++) {
		for (int tx = 0; tx < static_cast<int>(this->map.width); tx++) {
			size_t at = TileXY(tx, ty).base() * CHANNELS;
			if (traced[at] == this->texels[at] && traced[at + 1] == this->texels[at + 1]) continue;
			moved = TakingIn(moved.value_or(Rect{tx, ty, tx, ty}), tx, ty);
		}
	}
	this->texels = std::move(traced);
	return moved;
}
