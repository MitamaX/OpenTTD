/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file forest_field.cpp Every tree standing on the map, planted block by block from the world's flora, and the ones a view draws at each detail. */

#include "../../stdafx.h"
#include "forest_field.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "../../map_func.h"
#include "../core/seed.h"
#include "../gpu/draw_list.h"
#include "../gpu/frame_profile.h"
#include "../map/tile_shapes.h"
#include "build_slice.h"
#include "seabed.h"

#include "../../safeguards.h"

static constexpr double CROWN_REACH = 0.3;
static constexpr int CROWN_MARGIN = 1;
static_assert(CROWN_REACH <= CROWN_MARGIN);
static constexpr double SLOT_JITTER = 0.09;
static constexpr double EDGE_MARGIN = 0.08;
static constexpr double SPARE_TREE_SCALE = 0.85;
static constexpr double TOY_SCALE = 1.4;
static constexpr double TOY_HUDDLE = 0.3;
static constexpr SeedRange TOY_HUDDLE_SPOT = {0.3, 0.7};
static constexpr uint32_t PLANTING_SALT = 0x7EE5F00DU;

/* Spots a tile's trees stand on, spread so neighbours' crowns just meet; the middle one comes last, clear of a park's paths. */
static constexpr std::array<TilePoint, 6> SLOTS = {{
	{0.2, 0.22}, {0.7, 0.16}, {0.84, 0.6}, {0.5, 0.84}, {0.16, 0.7}, {0.5, 0.48},
}};
static constexpr size_t OUTER_SLOTS = SLOTS.size() - 1;

/* How many trees show for the game's count of one to four on a tile: a full tile reads as woodland, not as an orchard. */
static constexpr std::array<uint, FLORA_MOST_TREES + 1> SHOWN_TREES = {0, 1, 3, 4, 6};
static constexpr std::array<uint, FLORA_MOST_TREES + 1> SHOWN_TOYS = {0, 3, 4, 6, 6};


/* The finest and the coarsest detail a tree this many tile pixels across shows in, counting the bands where details crossfade, down to the coarsest asked for; none when first passes last. */
struct DetailSpan {
	size_t first;
	size_t last;

	bool Shows() const { return this->first <= this->last && this->first < TREE_DETAILS; }
};

static DetailSpan DetailsAt(double tile_pixels, TreeDetail coarsest)
{
	double octave = std::log2(std::max(tile_pixels, 1e-6));
	auto floor = [](size_t detail) { return std::log2(TREE_DETAIL_FLOORS[detail]); };
	size_t first = 0;
	while (first < TREE_DETAILS && octave < floor(first) - TREE_CROSSFADE_OCTAVES) first++;
	size_t last = 0;
	while (last < static_cast<size_t>(coarsest) && octave < floor(last) + TREE_CROSSFADE_OCTAVES) last++;
	return {first, last};
}

/* The fewest pixels a tile spans where a tree still shows in the coarsest detail asked for. */
static double FewestPixels(TreeDetail coarsest)
{
	return TREE_DETAIL_FLOORS[static_cast<size_t>(coarsest)] * std::exp2(-TREE_CROSSFADE_OCTAVES);
}

static uint8_t ShareByte(double share)
{
	return static_cast<uint8_t>(std::lround(std::clamp(share, 0.0, 1.0) * CHANNEL_MAX));
}

/* A tile's trees, the same every time it is planted: each on a slot of its own, the slots turned, mirrored and shuffled tile by tile. */
static void PlantTile(int tx, int ty, std::vector<std::pair<size_t, TreeInstance>> &planted)
{
	TileIndex tile = TileXY(tx, ty);
	uint8_t flora = _world_tiles.GroundAt(tile).flora;
	uint count = std::min<uint>(flora & FLORA_COUNT_MASK, FLORA_MOST_TREES);
	if (count == 0) return;

	TreeKind kind = static_cast<TreeKind>(flora >> FLORA_KIND_SHIFT);
	TreeAge age = static_cast<TreeAge>((flora >> FLORA_AGE_SHIFT) & FLORA_AGE_MASK);
	SeedDice dice(Hash32(PLANTING_SALT + tile.base()));
	bool mirror_x = (dice.Next() & 1) != 0;
	bool mirror_y = (dice.Next() & 1) != 0;
	bool swap = (dice.Next() & 1) != 0;
	size_t turn = dice.Next() % OUTER_SLOTS;
	bool toy = kind == TreeKind::Toy;
	MapVector huddle = toy ? MapVector{dice.Between(TOY_HUDDLE_SPOT), dice.Between(TOY_HUDDLE_SPOT)} : MapVector{HALF_TILE, HALF_TILE};
	uint shown = (toy ? SHOWN_TOYS : SHOWN_TREES)[count];
	for (uint tree = 0; tree < shown; tree++) {
		auto [sx, sy] = tree < OUTER_SLOTS ? SLOTS[(tree + turn) % OUTER_SLOTS] : SLOTS.back();
		if (swap) std::swap(sx, sy);
		double fx = std::clamp((mirror_x ? 1.0 - sx : sx) + dice.Between(-SLOT_JITTER, SLOT_JITTER), EDGE_MARGIN, 1.0 - EDGE_MARGIN);
		double fy = std::clamp((mirror_y ? 1.0 - sy : sy) + dice.Between(-SLOT_JITTER, SLOT_JITTER), EDGE_MARGIN, 1.0 - EDGE_MARGIN);
		double x = tx + (toy ? std::lerp(fx, huddle.x, TOY_HUDDLE) : fx);
		double y = ty + (toy ? std::lerp(fy, huddle.y, TOY_HUDDLE) : fy);
		double scale = TREE_AGE_SCALES[to_underlying(age)] * dice.Between(0.82, 1.15) * (tree < count ? 1.0 : SPARE_TREE_SCALE) * (toy ? TOY_SCALE : 1.0);
		double wither = age == TreeAge::Dying ? dice.Between(0.35, 0.7) : 0.0;
		TreeShape shape = {kind, static_cast<uint8_t>(dice.Next() % TREE_SHAPES)};
		TreeInstance instance = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(BedLevel(tx, ty, x, y)), {ShareByte(dice.Share()), ShareByte(scale / TREE_LARGEST_SCALE), ShareByte(dice.Share()), ShareByte(wither)}};
		planted.emplace_back(shape.Index(), instance);
	}
}

void ForestField::Plant(Cell &cell, const TileSpan &tiles) const
{
	_frame_profile.Count("forest_plants");
	ProfileScope profile("build", "forest", ProfileClock::Cpu);
	std::vector<std::pair<size_t, TreeInstance>> planted;
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) PlantTile(tx, ty, planted);
	}
	std::ranges::stable_sort(planted, {}, [](const auto &entry) { return entry.first; });

	cell.trees.clear();
	cell.runs.clear();
	cell.low = 0.0;
	cell.high = 0.0;
	for (const auto &[shape, tree] : planted) {
		if (cell.runs.empty() || cell.runs.back().shape != shape) cell.runs.push_back({shape, cell.trees.size(), 0});
		cell.runs.back().count++;
		cell.low = cell.trees.empty() ? tree.level : std::min<double>(cell.low, tree.level);
		cell.high = cell.trees.empty() ? tree.level : std::max<double>(cell.high, tree.level);
		cell.trees.push_back(tree);
	}
	cell.planted = true;
	cell.stale = false;
}

/* A block's trees grow from the flora of its own tiles, on a bed laid by the shape and the water of the tiles out to the shelf's edge. */
void ForestField::Sync(const WorldChanges &changes)
{
	this->frame++;
	this->Evict();

	Dimension size = _world_tiles.Size();
	if (changes.whole || size != this->grid.Map()) {
		this->grid.Lay(size);
		this->cells = std::vector<Cell>(this->grid.Count());
		this->keep.Clear();
		return;
	}
	auto unplant = [&](size_t index) { this->cells[index].stale = true; };
	this->grid.ForEachTouched(changes, ChangeKind::Flora, 0, unplant);
	this->grid.ForEachTouched(changes, {ChangeKind::Shape, ChangeKind::Water}, SHELF_TILES, unplant);
}

static double FarthestDistance(const Vec3 &eye, const Vec3 &low, const Vec3 &high)
{
	auto reach = [](double from, double a, double b) { return std::max(std::abs(from - a), std::abs(from - b)); };
	return Length({reach(eye.x, low.x, high.x), reach(eye.y, low.y, high.y), reach(eye.z, low.z, high.z)});
}

/* A block lying wholly within one detail hands over its runs whole; one across a crossfade sorts its trees one by one. */
void ForestField::GatherCell(const Cell &cell, const SceneView &camera, double near_pixels, double far_pixels, TreeDetail coarsest, TreeBatch &batch) const
{
	DetailSpan near = DetailsAt(near_pixels, coarsest);
	DetailSpan far = DetailsAt(far_pixels, coarsest);
	if (near.first == far.last) {
		TreeDetail detail = static_cast<TreeDetail>(near.first);
		for (const Run &run : cell.runs) batch.Add(TreeMeshIndex(run.shape, detail), std::span<const TreeInstance>(cell.trees).subspan(run.first, run.count));
		return;
	}

	double rise = LevelRise();
	for (const Run &run : cell.runs) {
		for (size_t index = run.first; index < run.first + run.count; index++) {
			const TreeInstance &tree = cell.trees[index];
			DetailSpan span = DetailsAt(camera.TilePixelsAt(Length(camera.eye - Vec3{tree.x, tree.y, tree.level * rise})), coarsest);
			if (!span.Shows()) continue;
			for (size_t detail = span.first; detail <= span.last; detail++) batch.Add(TreeMeshIndex(run.shape, static_cast<TreeDetail>(detail)), tree);
		}
	}
}

/* Until it is planted, a block is bounded by the whole height of the map. */
std::pair<Vec3, Vec3> ForestField::Bounds(const Cell &cell, size_t index) const
{
	double rise = LevelRise();
	TileSpan tiles = this->grid.TilesOf(index);
	double low = cell.planted ? cell.low : 0.0;
	double high = cell.planted ? cell.high : _world_tiles.Peak();
	return {
		{tiles.tx0 - CROWN_REACH, tiles.ty0 - CROWN_REACH, low * rise - DEEPEST_SINK * rise},
		{tiles.tx1 + 1.0 + CROWN_REACH, tiles.ty1 + 1.0 + CROWN_REACH, high * rise + TREE_TALLEST},
	};
}

/* A block is planted once the camera comes near enough to show its trees, or to show their shadows out of its sight, and planted afresh where the flora or the ground changed in it,
 * within a slice of each frame: those in sight first, the nearest of them first, then those casting shadows into the view. Each shows the trees it last grew until then. */
void ForestField::Refresh(const SceneView &camera, TreeDetail coarsest_shown, TreeDetail coarsest_cast)
{
	this->due.clear();
	this->grid.ForEachWithin(camera, FewestPixels(coarsest_shown), CROWN_MARGIN, [&](size_t index) {
		const Cell &cell = this->cells[index];
		if (!cell.stale) return true;
		auto [low, high] = this->Bounds(cell, index);
		double pixels = camera.NearestTilePixels(low, high);
		bool unseen = !BoxMeets(camera.frustum, low, high);
		if (DetailsAt(pixels, unseen ? coarsest_cast : coarsest_shown).Shows()) this->due.push_back({unseen, pixels, index});
		return true;
	});
	std::ranges::sort(this->due, {}, [](const Due &entry) { return std::make_pair(entry.unseen, -entry.pixels); });

	BuildSlice slice;
	for (const Due &entry : this->due) {
		if (slice.Spent()) return;
		this->Plant(this->cells[entry.index], this->grid.TilesOf(entry.index));
		this->keep.Hold(entry.index);
	}
}

/* Only the blocks near enough for a tile to span the coarsest detail's fewest pixels are looked at. */
void ForestField::Gather(const SceneView &camera, const Frustum &frustum, TreeDetail coarsest, TreeBatch &batch)
{
	this->grid.ForEachWithin(camera, FewestPixels(coarsest), CROWN_MARGIN, [&](size_t index) {
		Cell &cell = this->cells[index];
		if (!cell.planted) return true;
		auto [low, high] = this->Bounds(cell, index);
		if (!BoxMeets(frustum, low, high)) return true;

		double near_pixels = camera.NearestTilePixels(low, high);
		if (!DetailsAt(near_pixels, coarsest).Shows()) return true;
		cell.drawn = this->frame;
		if (!cell.trees.empty()) this->GatherCell(cell, camera, near_pixels, camera.TilePixelsAt(FarthestDistance(camera.eye, low, high)), coarsest, batch);
		return true;
	});
}

/* Past what the field may keep, the blocks longest out of every view let their trees go, so a big map only holds the forests about the view. */
void ForestField::Evict()
{
	auto bytes = [](const Cell &cell) { return cell.trees.size() * sizeof(TreeInstance) + cell.runs.size() * sizeof(Run); };
	this->keep.Trim(this->cells, this->frame, bytes, &Cell::drawn, [](Cell &cell) { cell = Cell{}; });
}

void ForestField::Release()
{
	this->cells.clear();
	this->keep.Clear();
	this->grid.Clear();
}
