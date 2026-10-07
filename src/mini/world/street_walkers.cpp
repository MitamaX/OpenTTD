/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file street_walkers.cpp People strolling along town pavements, laid out from the map's own roads and moved every frame; they only ever look, never live. */

#include "../../stdafx.h"
#include "street_walkers.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../../map_func.h"
#include "../../road_type.h"
#include "../core/seed.h"
#include "../map/network_style.h"
#include "../map/tile_shapes.h"
#include "figure_models.h"
#include "road_models.h"
#include "../gpu/draw_list.h"

#include "../../safeguards.h"

/* A walk runs at most this many tiles either way of the tile its walker belongs to. */
static constexpr int RUN_REACH = 6;
static constexpr int DENSITY_REACH = 2;
static constexpr double DENSITY_TILES = (2 * DENSITY_REACH + 1) * (2 * DENSITY_REACH + 1);
static constexpr double SPARSE_WALKERS = 0.5;
static constexpr double CROWDED_WALKERS = 2.6;
static constexpr double PAVEMENT_LINE = (ROAD_HALF + HALF_TILE) / 2.0;
static constexpr double LINE_JITTER = 0.035;
static constexpr double STANDING_SHARE = 0.15;
static constexpr SeedRange PACE = {0.12, 0.24};
static constexpr double STRIDE = 0.045;
static constexpr double BOB = 0.004;
static constexpr size_t MOST_WALKERS = 8000;
static constexpr int MOST_BUILDS = 4;
static constexpr uint32_t WALKER_SALT = 0x5EED11U;

static constexpr std::array<uint32_t, 12> SHIRTS = {
	0xC0392B, 0x2E86C1, 0xF4D03F, 0x27AE60, 0xECF0F1, 0x34495E, 0xE67E22, 0x8E44AD, 0x95A5A6, 0xD98880, 0x1ABC9C, 0xF5F0E1,
};
static constexpr std::array<uint32_t, 7> TROUSERS = {0x2C3E50, 0x1E1E20, 0x5D6D7E, 0x6E4B2A, 0x3B5998, 0xBFA77A, 0x7A2E2E};

static std::array<uint8_t, 4> Paint(uint32_t rgb)
{
	return {static_cast<uint8_t>(Red(rgb)), static_cast<uint8_t>(Green(rgb)), static_cast<uint8_t>(Blue(rgb)), static_cast<uint8_t>(CHANNEL_MAX)};
}

static MapVector AlongAxis(Axis axis)
{
	return axis == AXIS_X ? MapVector{1.0, 0.0} : MapVector{0.0, 1.0};
}

/* A town street running straight on through the tile along the axis, its pavements unbroken by rails or a bridge. */
static bool WalkedThrough(int tx, int ty, Axis axis)
{
	if (!OnMap(tx, ty)) return false;
	NetworkTexel network = _world_tiles.NetworkAt(TileXY(tx, ty));
	uint through = axis == AXIS_X ? ROAD_X : ROAD_Y;
	bool kerbed = (network.style & NETWORK_KERB_BIT) != 0 && (network.style & NETWORK_BRIDGE_BIT) == 0;
	return kerbed && network.track == 0 && (network.road & through) == through;
}

static bool IsBuilt(int tx, int ty)
{
	if (!OnMap(tx, ty)) return false;
	TileIndex tile = TileXY(tx, ty);
	return GroundworkOf(_world_tiles.GroundAt(tile), _world_tiles.NetworkAt(tile)) == Groundwork::Built;
}

static double Crowding(int tx, int ty)
{
	int built = 0;
	for (int dy = -DENSITY_REACH; dy <= DENSITY_REACH; dy++) {
		for (int dx = -DENSITY_REACH; dx <= DENSITY_REACH; dx++) built += IsBuilt(tx + dx, ty + dy) ? 1 : 0;
	}
	return built / DENSITY_TILES;
}

/* How many tiles on from a tile the street runs straight on along a heading, up to the reach of a walk. */
static int RunOn(int tx, int ty, MapVector step, Axis axis)
{
	int run = 0;
	while (run < RUN_REACH && WalkedThrough(tx + static_cast<int>(step.x) * (run + 1), ty + static_cast<int>(step.y) * (run + 1), axis)) run++;
	return run;
}

void StreetWalkers::Sync(const WorldChanges &changes)
{
	if (changes.whole || this->grid.Map() != _world_tiles.Size()) {
		this->grid.Lay(_world_tiles.Size());
		this->blocks = std::vector<Block>(this->grid.Count());
		return;
	}
	this->grid.ForEachTouched(changes.areas, RUN_REACH + DENSITY_REACH, [&](size_t index) { this->blocks[index].stale = true; });
}

/* Each pavement beside a house gets its walkers, pacing the stretch of street that runs straight on through their tile, or standing about on it. */
void StreetWalkers::Build(size_t index)
{
	Block &block = this->blocks[index];
	block.walkers.clear();
	block.stale = false;
	TileSpan tiles = this->grid.TilesOf(index);
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			for (Axis axis : {AXIS_X, AXIS_Y}) {
				if (!WalkedThrough(tx, ty, axis)) continue;
				MapVector along = AlongAxis(axis);
				MapVector across = {-along.y, along.x};
				MapVector centre = {tx + HALF_TILE, ty + HALF_TILE};
				MapVector from = centre - along * (RunOn(tx, ty, along * -1.0, axis) + HALF_TILE);
				MapVector to = centre + along * (RunOn(tx, ty, along, axis) + HALF_TILE);
				for (int side : {-1, 1}) {
					if (!IsBuilt(tx + static_cast<int>(across.x) * side, ty + static_cast<int>(across.y) * side)) continue;
					SeedDice dice(Hash32(WALKER_SALT ^ (TileXY(tx, ty).base() * 4 + to_underlying(axis) * 2 + (side > 0 ? 1 : 0))));
					int count = static_cast<int>(SPARSE_WALKERS + CROWDED_WALKERS * Crowding(tx, ty) + dice.Share());
					for (int walker = 0; walker < count; walker++) {
						MapVector start = from + across * (side * (PAVEMENT_LINE + dice.Between(-LINE_JITTER, LINE_JITTER)));
						Walker placed = {
							static_cast<float>(start.x), static_cast<float>(start.y), static_cast<float>(along.x), static_cast<float>(along.y),
							static_cast<float>(std::hypot(to.x - from.x, to.y - from.y)), static_cast<float>(dice.Share()), static_cast<float>(dice.Between(PACE)),
							Paint(SHIRTS[dice.Below(SHIRTS.size())]), Paint(TROUSERS[dice.Below(TROUSERS.size())]),
						};
						if (dice.Share() < STANDING_SHARE) placed.pace = 0.0f;
						block.walkers.push_back(placed);
					}
				}
			}
		}
	}
}

/* Only blocks within the distance at which a tile still spans the fewest pixels are visited, so the work stays near the eye however big the map. */
void StreetWalkers::Gather(const SceneView &view, const Frustum &frustum, double fewest_pixels, VehicleBatch &batch)
{
	if (this->grid.Map() != _world_tiles.Size()) this->Sync(WorldChanges{.whole = true});
	if (this->blocks.empty()) return;
	double reach = view.focal / fewest_pixels + RUN_REACH;
	Dimension map = this->grid.Map();
	auto tile_at = [](double at, uint size) { return std::clamp(static_cast<int>(at), 0, static_cast<int>(size) - 1); };
	int tx0 = tile_at(view.eye.x - reach, map.width);
	int tx1 = tile_at(view.eye.x + reach, map.width);
	int ty0 = tile_at(view.eye.y - reach, map.height);
	int ty1 = tile_at(view.eye.y + reach, map.height);
	double top = (_world_tiles.Peak() + 1.0) * LevelRise();
	int builds = 0;
	size_t gathered = 0;
	for (int by = ty0 - ty0 % BLOCK_TILES; by <= ty1 && gathered < MOST_WALKERS; by += BLOCK_TILES) {
		for (int bx = tx0 - tx0 % BLOCK_TILES; bx <= tx1 && gathered < MOST_WALKERS; bx += BLOCK_TILES) {
			size_t index = this->grid.IndexOf(bx, by);
			TileSpan tiles = this->grid.TilesOf(index);
			Vec3 low = {static_cast<double>(tiles.tx0 - RUN_REACH), static_cast<double>(tiles.ty0 - RUN_REACH), 0.0};
			Vec3 high = {static_cast<double>(tiles.tx1 + 1 + RUN_REACH), static_cast<double>(tiles.ty1 + 1 + RUN_REACH), top};
			if (!BoxMeets(frustum, low, high) || view.NearestTilePixels(low, high) < fewest_pixels) continue;
			Block &block = this->blocks[index];
			if (block.stale) {
				if (builds >= MOST_BUILDS) continue;
				this->Build(index);
				builds++;
			}
			gathered += GatherBlock(view, block.walkers, fewest_pixels, batch);
		}
	}
}

/* A walker turns about at either end of its stretch; one standing about keeps to its spot, facing along the street one way or the other. */
size_t StreetWalkers::GatherBlock(const SceneView &view, std::span<const Walker> walkers, double fewest_pixels, VehicleBatch &batch)
{
	size_t gathered = 0;
	for (const Walker &walker : walkers) {
		bool walking = walker.pace > 0.0f;
		double walked = view.clock * walker.pace + walker.phase * walker.length;
		double lap = std::fmod(walked / walker.length, 2.0);
		double share = walking ? (lap < 1.0 ? lap : 2.0 - lap) : walker.phase;
		double heading = (walking ? lap < 1.0 : walker.phase < 0.5) ? 1.0 : -1.0;
		double x = walker.x + walker.along_x * share * walker.length;
		double y = walker.y + walker.along_y * share * walker.length;
		double z = GroundLevel(x, y) * LevelRise() + PAVEMENT_TOP;
		if (view.TilePixelsAt(Length(view.eye - Vec3{x, y, z})) < fewest_pixels) continue;

		double steps = walked / STRIDE;
		FigurePose pose = walking ? WALK_CYCLE[static_cast<size_t>(std::fmod(steps, 2.0) * 2.0) % WALK_CYCLE.size()] : FigurePose::Upright;
		double bob = walking ? BOB * std::abs(std::sin(steps * std::numbers::pi)) : 0.0;
		double yaw = std::atan2(walker.along_y * heading, walker.along_x * heading);
		batch.Add(to_underlying(pose), VehicleInstance{
			static_cast<float>(x), static_cast<float>(y), static_cast<float>(z + bob), static_cast<float>(yaw), 0.0f, 0.0f, 1.0f,
			walker.shirt, walker.trousers, {},
		});
		gathered++;
	}
	return gathered;
}

void StreetWalkers::Release()
{
	this->grid.Clear();
	this->blocks.clear();
}
