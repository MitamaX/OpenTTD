/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sea_banks.cpp Earth banked up out of the sea against tiles standing above the shore, rounding down from their tops into the seabed. */

#include "../../stdafx.h"
#include "sea_banks.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <numbers>
#include <optional>

#include "../../direction_func.h"
#include "../../map_func.h"
#include "../map/tile_shapes.h"
#include "shore_relief.h"
#include "tile_relief.h"

#include "../../safeguards.h"

static constexpr double BANK_RUN = 0.6;
static constexpr double BANK_RUN_PER_LEVEL = 0.3;
static constexpr double LONGEST_BANK_RUN = 1.2;
static constexpr double BRINK_OVERHANG = 0.25;
static constexpr double BRINK_TOP_SLOPE = 2.0;
static constexpr double BANK_PINNING_REACH = 0.35;
static constexpr int BANK_GATHER_REACH = 4;
static constexpr int QUAY_WATER_WIDTH = 1;
static constexpr double HALF_TILE_DIAGONAL = HALF_TILE * std::numbers::sqrt2;
static constexpr double BRINK_SITE_REACH = LONGEST_BANK_RUN + BRINK_OVERHANG + HALF_TILE_DIAGONAL;
static constexpr double PIN_SITE_REACH = BANK_PINNING_REACH + HALF_TILE_DIAGONAL + SLOPE_PROBE;

static double BankProfile(double share)
{
	double rest = 1.0 - share;
	return rest * rest * rest * (1.0 + 3.0 * share);
}

double Brink::Lift(const MapVector &at, double ground) const
{
	MapVector offset = at - this->from;
	double out = Dot(offset, this->outward);
	if (out < -BRINK_OVERHANG) return 0.0;
	MapVector along = this->to - this->from;
	double length = Length(along);
	double share_along = ShareAlong(at, this->from, this->to);
	double past_end = std::abs(Dot(offset, along) / length - share_along * length);
	double reach = std::min(BANK_RUN + BANK_RUN_PER_LEVEL * std::lerp(this->rise[0], this->rise[1], share_along), LONGEST_BANK_RUN);
	double share_out = std::hypot(std::max(out, 0.0), past_end) / reach;
	if (share_out >= 1.0) return 0.0;
	double top = std::min({std::lerp(this->top[0], this->top[1], share_along), this->cap[0] + BRINK_TOP_SLOPE * share_along * length, this->cap[1] + BRINK_TOP_SLOPE * (1.0 - share_along) * length});
	return std::max(top - ground, 0.0) * BankProfile(share_out);
}

BankSite::BankSite(std::vector<Brink> brinks, std::vector<Segment> pins) : brinks(std::move(brinks)), pins(std::move(pins))
{
}

double BankSite::Lift(double x, double y, double ground) const
{
	MapVector at = {x, y};
	double lift = 0.0;
	for (const Brink &brink : this->brinks) lift = std::max(lift, brink.Lift(at, ground));
	if (lift <= 0.0) return 0.0;
	double nearest_pin = std::numeric_limits<double>::infinity();
	for (const Segment &pin : this->pins) nearest_pin = std::min(nearest_pin, pin.Distance(at));
	return lift * SmoothStep(0.0, BANK_PINNING_REACH, nearest_pin);
}

static bool SamePlace(const MapVector &a, const MapVector &b)
{
	return a.x == b.x && a.y == b.y;
}

static bool NarrowWaterBeyond(int nx, int ny, const TileIndexDiffC &across)
{
	for (int width = 1; width <= QUAY_WATER_WIDTH; width++) {
		if (WaterFormOf(nx + across.x * width, ny + across.y * width) != WaterForm::Open) return true;
	}
	return false;
}

static Segment SideOf(int tx, int ty, const TileIndexDiffC &across)
{
	MapVector start = {static_cast<double>(tx + std::max<int>(across.x, 0)), static_cast<double>(ty + std::max<int>(across.y, 0))};
	MapVector run = {static_cast<double>(std::abs(across.y)), static_cast<double>(std::abs(across.x))};
	return {start, start + run};
}

static void AddBrinks(std::vector<Brink> &brinks, const Seabed &bed, int tx, int ty)
{
	for (DiagDirection side = DIAGDIR_BEGIN; side < DIAGDIR_END; side++) {
		std::optional<StepFace> step = StepFaceOf(tx, ty, side);
		if (!step.has_value()) continue;
		TileIndexDiffC across = TileIndexDiffCByDiagDir(side);
		int nx = tx + across.x;
		int ny = ty + across.y;
		if (!LiesAlongShore(bed, nx, ny) || NarrowWaterBeyond(nx, ny, across)) continue;
		const std::array<WorldPoint, 4> &ring = step->ring;
		if (SamePlace(Flat(ring[0]), Flat(ring[3]))) continue;
		std::array<double, 2> top = {ring[0].level, ring[3].level};
		brinks.push_back({Flat(ring[0]), Flat(ring[3]), step->outward, top, {ring[0].level - ring[1].level, ring[3].level - ring[2].level}, top});
	}
}

static void AddPins(std::vector<Segment> &pins, const Seabed &bed, int tx, int ty)
{
	for (DiagDirection side = DIAGDIR_BEGIN; side < DIAGDIR_END; side++) {
		TileIndexDiffC across = TileIndexDiffCByDiagDir(side);
		int nx = tx + across.x;
		int ny = ty + across.y;
		if (!OnMap(nx, ny) || LiesAlongShore(bed, nx, ny)) continue;
		Segment edge = SideOf(tx, ty, across);
		std::optional<StepFace> above = StepFaceOf(nx, ny, ReverseDiagDir(side));
		if (!above.has_value()) {
			pins.push_back(edge);
			continue;
		}
		double first = ShareAlong(Flat(above->ring[0]), edge.from, edge.to);
		double last = ShareAlong(Flat(above->ring[3]), edge.from, edge.to);
		MapVector run = edge.to - edge.from;
		double low = std::min(first, last);
		double high = std::max(first, last);
		if (low > 0.0) pins.push_back({edge.from, edge.from + run * low});
		if (high < 1.0) pins.push_back({edge.from + run * high, edge.to});
	}
}

static void CapMeetingEnds(std::vector<Brink> &brinks)
{
	for (Brink &brink : brinks) {
		for (const Brink &other : brinks) {
			for (size_t end = 0; end < 2; end++) {
				const MapVector &at = end == 0 ? brink.from : brink.to;
				if (SamePlace(at, other.from)) brink.cap[end] = std::min(brink.cap[end], other.top[0]);
				if (SamePlace(at, other.to)) brink.cap[end] = std::min(brink.cap[end], other.top[1]);
			}
		}
	}
}

const SeaBanks::Gathered &SeaBanks::Gather(const Seabed &bed) const
{
	if (this->gathered.has_value()) return *this->gathered;
	Gathered &found = this->gathered.emplace();
	int last_x = static_cast<int>(Map::MaxX());
	int last_y = static_cast<int>(Map::MaxY());
	for (int ty = std::max(this->tiles.ty0 - BANK_GATHER_REACH, 0); ty <= std::min(this->tiles.ty1 + BANK_GATHER_REACH, last_y); ty++) {
		for (int tx = std::max(this->tiles.tx0 - BANK_GATHER_REACH, 0); tx <= std::min(this->tiles.tx1 + BANK_GATHER_REACH, last_x); tx++) {
			AddBrinks(found.brinks, bed, tx, ty);
			if (LiesAlongShore(bed, tx, ty)) AddPins(found.pins, bed, tx, ty);
		}
	}
	CapMeetingEnds(found.brinks);
	return found;
}

BankSite SeaBanks::Around(const Seabed &bed, int tx, int ty) const
{
	const Gathered &all = this->Gather(bed);
	MapVector middle = {tx + HALF_TILE, ty + HALF_TILE};
	std::vector<Brink> brinks;
	std::ranges::copy_if(all.brinks, std::back_inserter(brinks), [&](const Brink &brink) { return SegmentDistance(middle, brink.from, brink.to) <= BRINK_SITE_REACH; });
	std::vector<Segment> pins;
	std::ranges::copy_if(all.pins, std::back_inserter(pins), [&](const Segment &pin) { return pin.Distance(middle) <= PIN_SITE_REACH; });
	return BankSite(std::move(brinks), std::move(pins));
}
