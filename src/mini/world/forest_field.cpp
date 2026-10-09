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
#include <limits>
#include <utility>

#include "../../map_func.h"
#include "../../settings_type.h"
#include "../core/seed.h"
#include "../gpu/draw_list.h"
#include "../gpu/frame_profile.h"
#include "../map/tile_shapes.h"
#include "build_slice.h"
#include "seabed.h"

#include "../../safeguards.h"

static constexpr double SLOT_JITTER = 0.15;
static constexpr double EDGE_MARGIN = 0.08;
static constexpr double SPARE_TREE_SCALE = 0.85;
static constexpr uint THICK_WOOD = 3;
static constexpr int FRINGE_MARGIN = 1;
static constexpr double FRINGE_REACH = 0.45;
static constexpr double FRINGE_SCALE = 0.6;
static constexpr double TOY_HUDDLE = 0.3;
static constexpr SeedRange TOY_HUDDLE_SPOT = {0.3, 0.7};
static constexpr SeedRange DYING_WITHER = {0.35, 0.7};
static constexpr uint32_t PLANTING_SALT = 0x7EE5F00DU;
static constexpr int STAND_TILES = 6;
static constexpr double STAND_MIDDLE = 0.5;
static constexpr double STAND_JITTER = 0.35;
static constexpr double STAND_LOYALTY = 0.6;
static constexpr uint32_t STAND_SALT = 0x57A2D5EEU;

/* Spots a tile's trees stand on, spread so neighbours' crowns just meet; the middle one comes last, clear of a park's paths. */
static constexpr std::array<TilePoint, 6> SLOTS = {{
	{0.2, 0.22}, {0.7, 0.16}, {0.84, 0.6}, {0.5, 0.84}, {0.16, 0.7}, {0.5, 0.48},
}};
static constexpr size_t OUTER_SLOTS = SLOTS.size() - 1;

/* How many trees show for the game's count of one to four on a tile: a full tile reads as woodland, not as an orchard. */
static constexpr std::array<uint, FLORA_MOST_TREES + 1> SHOWN_TREES = {0, 1, 3, 4, 6};
static constexpr std::array<uint, FLORA_MOST_TREES + 1> SHOWN_TOYS = {0, 3, 4, 6, 6};

struct CrownBounds {
	double reach;
	double tallest;
	int margin;
};

static constexpr int WholeTiles(double reach)
{
	int tiles = static_cast<int>(reach);
	return tiles < reach ? tiles + 1 : tiles;
}

static constexpr CrownBounds CrownBoundsOf(double largest_scale)
{
	double grown = largest_scale * TREE_THICKET_SCALES[1];
	double reach = (TREE_MODEL_REACH + TREE_LEAN * TREE_MODEL_TALLEST + TREE_SWAY_REACH) * grown;
	return {reach, TREE_MODEL_TALLEST * grown * TREE_STRETCHES[1], WholeTiles(reach)};
}

static constexpr CrownBounds NATURAL_CROWNS = CrownBoundsOf(TREE_LARGEST_NATURAL_SCALE);
static constexpr CrownBounds TOY_CROWNS = CrownBoundsOf(TREE_LARGEST_SCALE);

static const CrownBounds &Crowns()
{
	return _settings_game.game_creation.landscape == LandscapeType::Toyland ? TOY_CROWNS : NATURAL_CROWNS;
}


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

static bool Treeless(int tx, int ty)
{
	return !OnMap(tx, ty) || TreeCount(_world_tiles.GroundAt(TileXY(tx, ty)).flora) == 0;
}

/* How deep within a thick wood a spot of its tile lies, from nothing on a side that opens onto treeless ground to whole a fringe's reach in. */
static double FringeDepth(int tx, int ty, double fx, double fy)
{
	double reach = FRINGE_REACH;
	if (Treeless(tx - 1, ty)) reach = std::min(reach, fx);
	if (Treeless(tx + 1, ty)) reach = std::min(reach, 1.0 - fx);
	if (Treeless(tx, ty - 1)) reach = std::min(reach, fy);
	if (Treeless(tx, ty + 1)) reach = std::min(reach, 1.0 - fy);
	return reach / FRINGE_REACH;
}

using ShapedTree = std::pair<size_t, TreeInstance>;

struct Stand {
	MapVector centre;
	double roll;
};

static Stand StandOf(int bx, int by)
{
	SeedDice dice(SubSeed(Hash32(STAND_SALT + static_cast<uint32_t>(bx)), static_cast<uint32_t>(by)));
	double x = (bx + STAND_MIDDLE + dice.Between(-STAND_JITTER, STAND_JITTER)) * STAND_TILES;
	double y = (by + STAND_MIDDLE + dice.Between(-STAND_JITTER, STAND_JITTER)) * STAND_TILES;
	return {{x, y}, dice.Share()};
}

static double StandRoll(int tx, int ty)
{
	int bx = tx / STAND_TILES;
	int by = ty / STAND_TILES;
	double nearest = std::numeric_limits<double>::infinity();
	double roll = 0.0;
	for (int dy = -1; dy <= 1; dy++) {
		for (int dx = -1; dx <= 1; dx++) {
			Stand stand = StandOf(bx + dx, by + dy);
			double apart = std::hypot(stand.centre.x - (tx + HALF_TILE), stand.centre.y - (ty + HALF_TILE));
			if (apart >= nearest) continue;
			nearest = apart;
			roll = stand.roll;
		}
	}
	return roll;
}

/* The dice a tree rolls, in the order it rolls them, so every tree planted again is the same. */
struct TreeRolls {
	double spread;
	double wither;
	uint8_t shape;
	double turn;
	double tint;
};

class Grove {
public:
	Grove(int tx, int ty, uint8_t flora);
	void Plant(std::vector<ShapedTree> &planted);

private:
	TilePoint Spot(uint tree);
	TreeRolls Roll();
	double Scale(double spread, double fringe, bool spare) const;
	ShapedTree Grown(const TilePoint &spot, double fringe, bool spare);

	int tx;
	int ty;
	TreeKind kind;
	TreeAge age;
	uint count;
	double stand;
	SeedDice dice;
	bool mirror_x = false;
	bool mirror_y = false;
	bool swap = false;
	size_t turn = 0;
	MapVector huddle = {HALF_TILE, HALF_TILE};
	double huddling = 0.0;
};

Grove::Grove(int tx, int ty, uint8_t flora) :
	tx(tx),
	ty(ty),
	kind(static_cast<TreeKind>(flora >> FLORA_KIND_SHIFT)),
	age(static_cast<TreeAge>((flora >> FLORA_AGE_SHIFT) & FLORA_AGE_MASK)),
	count(std::min<uint>(TreeCount(flora), FLORA_MOST_TREES)),
	stand(StandRoll(tx, ty)),
	dice(Hash32(PLANTING_SALT + TileXY(tx, ty).base()))
{
	this->mirror_x = (this->dice.Next() & 1) != 0;
	this->mirror_y = (this->dice.Next() & 1) != 0;
	this->swap = (this->dice.Next() & 1) != 0;
	this->turn = this->dice.Below(OUTER_SLOTS);
	if (this->kind != TreeKind::Toy) return;
	double huddle_x = this->dice.Between(TOY_HUDDLE_SPOT);
	this->huddle = {huddle_x, this->dice.Between(TOY_HUDDLE_SPOT)};
	this->huddling = TOY_HUDDLE;
}

void Grove::Plant(std::vector<ShapedTree> &planted)
{
	bool toy = this->kind == TreeKind::Toy;
	bool thinning = !toy && this->count >= THICK_WOOD;
	uint shown = (toy ? SHOWN_TOYS : SHOWN_TREES)[this->count];
	for (uint tree = 0; tree < shown; tree++) {
		TilePoint spot = this->Spot(tree);
		double fringe = thinning ? FringeDepth(this->tx, this->ty, spot.first, spot.second) : 1.0;
		bool spare = tree >= this->count;
		if (spare && this->dice.Share() > fringe) continue;
		planted.push_back(this->Grown(spot, fringe, spare));
	}
}

TilePoint Grove::Spot(uint tree)
{
	auto [sx, sy] = tree < OUTER_SLOTS ? SLOTS[(tree + this->turn) % OUTER_SLOTS] : SLOTS.back();
	if (this->swap) std::swap(sx, sy);
	auto jittered = [&](double share, bool mirror) {
		return std::clamp((mirror ? 1.0 - share : share) + this->dice.Between(-SLOT_JITTER, SLOT_JITTER), EDGE_MARGIN, 1.0 - EDGE_MARGIN);
	};
	double fx = jittered(sx, this->mirror_x);
	return {fx, jittered(sy, this->mirror_y)};
}

TreeRolls Grove::Roll()
{
	TreeRolls rolls;
	rolls.spread = this->dice.Between(TREE_SPREAD);
	rolls.wither = this->age == TreeAge::Dying && this->kind != TreeKind::Toy ? this->dice.Between(DYING_WITHER) : 0.0;
	double loyal = this->dice.Share();
	double own = this->dice.Share();
	rolls.shape = PickTreeShape(this->kind, _settings_game.game_creation.landscape, loyal < STAND_LOYALTY ? this->stand : own);
	rolls.turn = this->dice.Share();
	rolls.tint = this->dice.Share();
	return rolls;
}

double Grove::Scale(double spread, double fringe, bool spare) const
{
	double aged = TREE_AGE_SCALES[to_underlying(this->age)] * spread;
	double fringed = std::lerp(FRINGE_SCALE, 1.0, fringe);
	double spared = spare ? SPARE_TREE_SCALE : 1.0;
	double sweet = this->kind == TreeKind::Toy ? TOY_TREE_SCALE : 1.0;
	return aged * fringed * spared * sweet;
}

ShapedTree Grove::Grown(const TilePoint &spot, double fringe, bool spare)
{
	TreeRolls rolls = this->Roll();
	double x = this->tx + std::lerp(spot.first, this->huddle.x, this->huddling);
	double y = this->ty + std::lerp(spot.second, this->huddle.y, this->huddling);
	double scale = this->Scale(rolls.spread, fringe, spare);
	TreeShape shape = {this->kind, rolls.shape};
	std::array<uint8_t, 4> look = {ShareByte(rolls.turn), ShareByte(scale / TREE_LARGEST_SCALE), ShareByte(rolls.tint), ShareByte(rolls.wither)};
	return {shape.Index(), {static_cast<float>(x), static_cast<float>(y), static_cast<float>(BedLevel(this->tx, this->ty, x, y)), look}};
}

void ForestField::Plant(Cell &cell, const TileSpan &tiles) const
{
	_frame_profile.Count("forest_plants");
	ProfileScope profile("build", "forest", ProfileClock::Cpu);
	std::vector<ShapedTree> planted;
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			uint8_t flora = _world_tiles.GroundAt(TileXY(tx, ty)).flora;
			if (TreeCount(flora) != 0) Grove(tx, ty, flora).Plant(planted);
		}
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
	this->grid.ForEachTouched(changes, ChangeKind::Flora, FRINGE_MARGIN, unplant);
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
	const CrownBounds &crowns = Crowns();
	return {
		{tiles.tx0 - crowns.reach, tiles.ty0 - crowns.reach, low * rise - DEEPEST_SINK * rise},
		{tiles.tx1 + 1.0 + crowns.reach, tiles.ty1 + 1.0 + crowns.reach, high * rise + crowns.tallest},
	};
}

/* A block is planted once the camera comes near enough to show its trees, or to show their shadows out of its sight, and planted afresh where the flora or the ground changed in it,
 * within a slice of each frame: those in sight first, the nearest of them first, then those casting shadows into the view. Each shows the trees it last grew until then. */
void ForestField::Refresh(const SceneView &camera, TreeDetail coarsest_shown, TreeDetail coarsest_cast)
{
	this->due.clear();
	this->grid.ForEachWithin(camera, FewestPixels(coarsest_shown), Crowns().margin, [&](size_t index) {
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
	this->grid.ForEachWithin(camera, FewestPixels(coarsest), Crowns().margin, [&](size_t index) {
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
