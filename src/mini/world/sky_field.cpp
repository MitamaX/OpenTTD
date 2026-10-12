/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sky_field.cpp How high the bodies around every tile rise against the sky, eight ways round, one texel per tile, for the light that reaches streets and yards. */

#include "../../stdafx.h"
#include "sky_field.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <tuple>
#include <utility>

#include "../../map_func.h"
#include "../core/camera.h"
#include "../map/tile_shapes.h"
#include "../map/world_tiles.h"
#include "rect_spans.h"

#include "../../safeguards.h"

static constexpr std::pair<float, float> NO_RUN = {1.0f, 0.0f};
static constexpr std::pair<float, float> UNBOUNDED_RUN = {0.0f, std::numeric_limits<float>::infinity()};
static constexpr float MOST_STEPS = 255.0f;
static constexpr float STRAIGHT_RAY = 1e-6f;

struct SkyWay {
	float x;
	float y;
};

static constexpr std::array<SkyWay, SKY_SECTORS> SKY_WAYS = {{
	{1.0f, 0.0f}, {SKY_SECTOR_TURN, SKY_SECTOR_TURN}, {0.0f, 1.0f}, {-SKY_SECTOR_TURN, SKY_SECTOR_TURN},
	{-1.0f, 0.0f}, {-SKY_SECTOR_TURN, -SKY_SECTOR_TURN}, {0.0f, -1.0f}, {SKY_SECTOR_TURN, -SKY_SECTOR_TURN},
}};

struct Standing {
	MassingBox box;
	TileIndex anchor;
};

class Surroundings {
public:
	Surroundings(const MassingField &massing, const Rect &bounds) : bounds(bounds), width(bounds.Width()), cells(static_cast<size_t>(bounds.Width()) * bounds.Height())
	{
		double rise = LevelRise();
		for (int y = bounds.top; y <= bounds.bottom; y++) {
			for (int x = bounds.left; x <= bounds.right; x++) {
				std::optional<MassingBox> box = massing.BoxAt(x, y, rise);
				if (!box.has_value()) continue;
				this->cells[this->IndexOf(x, y)] = Standing{*box, *massing.AnchorAt(x, y)};
				this->tallest = std::max(this->tallest, box->top);
			}
		}
	}

	bool Holds(int x, int y) const { return x >= this->bounds.left && x <= this->bounds.right && y >= this->bounds.top && y <= this->bounds.bottom; }
	const std::optional<Standing> &At(int x, int y) const { return this->cells[this->IndexOf(x, y)]; }
	float Tallest() const { return this->tallest; }

private:
	size_t IndexOf(int x, int y) const { return static_cast<size_t>(y - this->bounds.top) * this->width + (x - this->bounds.left); }

	Rect bounds;
	int width;
	std::vector<std::optional<Standing>> cells;
	float tallest = 0.0f;
};

struct Horizon {
	float tangent = 0.0f;
	float height = 0.0f;
	float run = 0.0f;
};

static std::pair<float, float> SlabRuns(float low, float high, float from, float along)
{
	if (std::abs(along) < STRAIGHT_RAY) return from > low && from < high ? UNBOUNDED_RUN : NO_RUN;
	float first = (low - from) / along;
	float second = (high - from) / along;
	return {std::min(first, second), std::max(first, second)};
}

static std::optional<float> RunInto(const MassingBox &box, float from_x, float from_y, const SkyWay &way)
{
	auto [enter_x, leave_x] = SlabRuns(box.low_x, box.high_x, from_x, way.x);
	auto [enter_y, leave_y] = SlabRuns(box.low_y, box.high_y, from_y, way.y);
	float enter = std::max({enter_x, enter_y, 0.0f});
	if (enter > std::min(leave_x, leave_y) || enter > HORIZON_REACH_TILES) return std::nullopt;
	return enter;
}

static float PerCell(float along)
{
	return std::abs(along) < STRAIGHT_RAY ? std::numeric_limits<float>::infinity() : 1.0f / std::abs(along);
}

class TileHorizons {
public:
	TileHorizons(const Surroundings &around, const MassingField &massing, int tx, int ty) :
		around(around), tx(tx), ty(ty), own(massing.AnchorAt(tx, ty)), floor(_world_tiles.SurfaceAt(TileXY(tx, ty)).Lowest() * static_cast<float>(LevelRise()))
	{
	}

	SkyTexel Survey() const
	{
		SkyTexel texel{};
		for (size_t sector = 0; sector < SKY_SECTORS; sector++) {
			texel[sector / SKY_SECTORS_PER_CHANNEL] |= Packed(this->Sweep(SKY_WAYS[sector])) << (sector % SKY_SECTORS_PER_CHANNEL * SKY_SECTOR_BITS);
		}
		return texel;
	}

private:
	static uint32_t Steps(float amount, float step)
	{
		return static_cast<uint32_t>(std::min(std::round(amount / step), MOST_STEPS));
	}

	static uint32_t Packed(const Horizon &horizon)
	{
		uint32_t height = Steps(horizon.height, SKY_HEIGHT_STEP);
		return height == 0 ? 0 : height | Steps(horizon.run, SKY_REACH_STEP) << SKY_REACH_SHIFT;
	}

	Horizon Sweep(const SkyWay &way) const
	{
		float per_x = PerCell(way.x);
		float per_y = PerCell(way.y);
		float next_x = per_x * HALF_TILE;
		float next_y = per_y * HALF_TILE;
		int x = this->tx;
		int y = this->ty;
		float tallest_height = this->around.Tallest() - this->floor;
		Horizon best;
		for (float entered = 0.0f; entered <= HORIZON_REACH_TILES;) {
			if (tallest_height / std::max(entered, NEAREST_HORIZON) <= best.tangent) break;
			bool across_x = next_x < next_y;
			entered = across_x ? next_x : next_y;
			if (across_x) {
				x += way.x > 0.0f ? 1 : -1;
				next_x += per_x;
			} else {
				y += way.y > 0.0f ? 1 : -1;
				next_y += per_y;
			}
			if (!this->around.Holds(x, y)) break;
			this->Consider(x, y, way, best);
		}
		return best;
	}

	void Consider(int x, int y, const SkyWay &way, Horizon &best) const
	{
		const std::optional<Standing> &standing = this->around.At(x, y);
		if (!standing.has_value() || (this->own.has_value() && standing->anchor == *this->own)) return;
		float height = standing->box.top - this->floor;
		if (height <= 0.0f) return;
		std::optional<float> run = RunInto(standing->box, this->tx + HALF_TILE, this->ty + HALF_TILE, way);
		if (!run.has_value()) return;
		float tangent = height / std::max(*run, NEAREST_HORIZON);
		if (tangent > best.tangent) best = {tangent, height, *run};
	}

	const Surroundings &around;
	int tx;
	int ty;
	std::optional<TileIndex> own;
	float floor;
};

void SkyField::Clear(Dimension map)
{
	this->map = map;
	this->generation = 0;
	this->texels.assign(static_cast<size_t>(map.width) * map.height, {});
	this->surveyed.assign(this->texels.size(), 0);
}

static bool Precedes(const Rect &a, const Rect &b)
{
	return std::tie(a.left, a.top, a.right, a.bottom) < std::tie(b.left, b.top, b.right, b.bottom);
}

static bool Alike(const Rect &a, const Rect &b)
{
	return !Precedes(a, b) && !Precedes(b, a);
}

static std::vector<Rect> Distinct(std::vector<Rect> areas)
{
	std::ranges::sort(areas, Precedes);
	areas.erase(std::ranges::unique(areas, Alike).begin(), areas.end());
	return areas;
}

static bool Within(const Rect &area, const Rect &clump)
{
	return area.left >= clump.left && area.top >= clump.top && area.right <= clump.right && area.bottom <= clump.bottom;
}

static std::vector<Rect> Clumped(std::vector<Rect> areas)
{
	for (bool joined_any = true; joined_any;) {
		joined_any = false;
		for (size_t kept = 0; kept < areas.size(); kept++) {
			for (size_t other = kept + 1; other < areas.size();) {
				Rect joined = Enclosing(areas[kept], areas[other]);
				if (AreaOf(joined) > MOST_JOINED_SPREAD * (AreaOf(areas[kept]) + AreaOf(areas[other]))) {
					other++;
					continue;
				}
				areas[kept] = joined;
				areas.erase(areas.begin() + static_cast<std::ptrdiff_t>(other));
				joined_any = true;
			}
		}
	}
	return areas;
}

std::vector<Rect> SkyField::Resurvey(std::span<const Rect> changed, const MassingField &massing)
{
	std::vector<Rect> areas;
	for (const Rect &span : changed) areas.push_back(TilesNear(span, HORIZON_REACH_TILES));
	areas = Distinct(std::move(areas));

	this->generation++;
	for (const Rect &clump : Clumped(areas)) {
		Surroundings around(massing, TilesNear(clump, HORIZON_REACH_TILES));
		for (const Rect &area : areas) {
			if (Within(area, clump)) this->Survey(area, around, massing);
		}
	}
	return areas;
}

void SkyField::Survey(const Rect &area, const Surroundings &around, const MassingField &massing)
{
	for (int y = area.top; y <= area.bottom; y++) {
		for (int x = area.left; x <= area.right; x++) {
			size_t index = static_cast<size_t>(y) * this->map.width + x;
			if (this->surveyed[index] == this->generation) continue;
			this->surveyed[index] = this->generation;
			this->texels[index] = TileHorizons(around, massing, x, y).Survey();
		}
	}
}
