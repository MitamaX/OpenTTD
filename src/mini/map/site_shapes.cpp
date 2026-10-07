/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file site_shapes.cpp The builders that turn a site's look into solids, and the parts they share. */

#include "../../stdafx.h"
#include "site_shapes.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <span>

#include "../../direction_func.h"
#include "../../tile_map.h"
#include "../core/tones.h"
#include "tile_shapes.h"

#include "../../safeguards.h"

struct WallFinish {
	Material wall;
	WindowGrid grid;
	Swatch swatch;
};

struct Drum {
	float cx;
	float cy;
	float radius;
	float height_share;
	RoofShape roof;
	float rise;
};

/* Equal cells spread over the tile with equal gaps around them. */
struct Grid {
	uint columns;
	uint rows;
	float width;
	float depth;
};

struct Load {
	Material material;
	uint32_t tint;
};

static constexpr float FULL_RISE = 1.0f;
static constexpr uint UNFINISHED_SHARE = 128;
static constexpr uint8_t GROVE_TREES = 4;

static constexpr uint SWATCH_BITS = 8;
static constexpr uint WALL_SWATCH_FIRST = 0;
static constexpr uint ROOF_SWATCH_FIRST = 8;
static constexpr uint RIDGE_BIT = 16;
static constexpr uint HEAP_BIT = 17;
static constexpr uint PILE_HEIGHT_FIRST = 18;
static constexpr uint PILE_HEIGHT_BITS = 2;
static constexpr uint32_t GLAZING_SALT = 5;
static constexpr uint32_t KIT_SALT = 11;

static constexpr uint KIT_FIELD_BITS = 2;
static constexpr uint KIT_COUNT_FIRST = 0;
static constexpr uint KIT_SHIFT_FIRST = 2;
static constexpr uint KIT_UNIT_FIRST = 4;
static constexpr uint KIT_UNIT_BITS = 3 * KIT_FIELD_BITS;
static constexpr uint ROOFTOP_SLOTS = 3;
static constexpr float ROOFTOP_MIN_SIDE = 0.10f;
static constexpr float ROOFTOP_MAX_SIDE = 0.20f;
static constexpr float ROOFTOP_MIN_HEIGHT = 0.04f;
static constexpr float ROOFTOP_MAX_HEIGHT = 0.08f;
static constexpr uint32_t ROOFTOP_TINT = 0xFF8E9398U;

static constexpr std::array<float, 2> BOTH_SIDES = {-1.0f, 1.0f};
static constexpr float HELIPAD_MARGIN = 0.07f;
static constexpr uint32_t HELIPAD_TINT = 0xFF3F7A57U;
static constexpr float LETTER_POST_OFFSET = 0.45f;
static constexpr float LETTER_POST_REACH = 0.55f;
static constexpr float LETTER_STROKE = 0.08f;

static constexpr float MAST_SIDE = 0.3f;
static constexpr float MAST_TAPER = 0.4f;
static constexpr float MAST_BAND = 0.4f;
static constexpr std::array<uint32_t, 2> MAST_TINTS = {COL_STOP, COL_PAPER};

static constexpr float BASIN_RADIUS = 0.35f;
static constexpr float BASIN_HEIGHT = 0.03f;
static constexpr float POOL_RADIUS = 0.30f;
static constexpr float POOL_DEPTH = 0.001f;
static constexpr uint32_t POOL_TINT = 0xFF4F8FB0U;
static constexpr float JET_RADIUS = 0.03f;
static constexpr float JET_HEIGHT = 0.15f;
static constexpr uint32_t JET_TINT = 0xFFBFD8E6U;

static constexpr Grid PILE_GRID = {3, 2, 0.25f, 0.35f};
static constexpr float PILE_LOW = 0.06f;
static constexpr float PILE_HIGH = 0.10f;
static constexpr uint32_t LOG_TINT = 0xFF8A6542U;
static constexpr uint32_t BALE_TINT = 0xFFD9C27AU;

static constexpr std::array<float, 3> PIT_INSETS = {0.0f, 0.15f, 0.3f};
static constexpr uint PIT_ALPHA = 30;
static constexpr uint32_t PIT_SHADE = WithAlpha(COL_SHADOW, PIT_ALPHA);

static constexpr Grid PLANT_GRID = {2, 2, 0.2f, 0.2f};
static constexpr float PLANT_HEIGHT = 0.08f;

static constexpr float VENT_SIDE = 0.1f;
static constexpr float VENT_CLEARANCE = 0.05f;

static constexpr int SAWTOOTH_STOREYS = 1;

static constexpr float TANK_DOME_RISE = 0.04f;
static constexpr uint TOWER_LEGS = 3;
static constexpr float TOWER_LEG_SIDE = 0.03f;
static constexpr float TOWER_LEG_SPREAD = 0.25f;
static constexpr float TOWER_LEG_HEIGHT = 0.5f;
static constexpr float RAISED_TANK_HEIGHT = 0.25f;

static constexpr float SILO_RADIUS = 0.15f;
static constexpr float SILO_CONE = 0.08f;
static constexpr float CONVEYOR_WIDTH = 0.06f;
static constexpr float CONVEYOR_HEIGHT = 0.04f;

static constexpr float CHIMNEY_PLINTH_SIDE = 0.5f;
static constexpr float CHIMNEY_PLINTH_HEIGHT = 0.2f;
static constexpr float CHIMNEY_RADIUS = 0.09f;
static constexpr float CHIMNEY_TAPER = 0.11f;
static constexpr float CHIMNEY_BAND = 0.08f;
static constexpr uint CHIMNEY_BANDS = 2;

static constexpr float AUX_COLUMN_SHARE = 0.75f;

static constexpr float FRAME_X = 0.6f;
static constexpr float FRAME_SIDE = 0.5f;
static constexpr float FRAME_TAPER = 0.2f;
static constexpr float SHEAVE_DECK_SIDE = 0.3f;
static constexpr float SHEAVE_DECK_HEIGHT = 0.06f;
static constexpr Plot WINDING_HOUSE = {0.02f, 0.2f, 0.33f, 0.8f};
static constexpr float WINDING_HOUSE_HEIGHT = 0.3f;

static constexpr Plot PUMPJACK_BED = Plot::Around(LOT_CENTRE, LOT_CENTRE, 0.3f, 0.125f);
static constexpr float PUMPJACK_BED_HEIGHT = 0.02f;
static constexpr float PUMPJACK_POST_SIDE = 0.06f;
static constexpr float PUMPJACK_POST_HEIGHT = 0.16f;
static constexpr Plot PUMPJACK_BEAM = Plot::Around(LOT_CENTRE, LOT_CENTRE, 0.3f, 0.025f);
static constexpr float PUMPJACK_BEAM_HEIGHT = 0.03f;

static constexpr float DECK_LEVEL = 0.25f;
static constexpr float DECK_THICKNESS = 0.05f;
static constexpr float DECK_LEG_SIDE = 0.06f;
static constexpr float DECK_LEG_INSET = 0.1f;
static constexpr Plot CRANE_POST = Plot::Around(0.3f, 0.3f, 0.03f, 0.03f);
static constexpr float CRANE_HEIGHT = 0.5f;
static constexpr Plot CRANE_BOOM = {CRANE_POST.x0, CRANE_POST.y0, 0.85f, CRANE_POST.y1};
static constexpr float CRANE_BOOM_DEPTH = 0.04f;
static constexpr float DERRICK_SIDE = 0.45f;
static constexpr float DERRICK_TAPER = 0.37f;
static constexpr float DERRICK_HEIGHT = 0.9f;
static constexpr Plot QUARTERS = {0.15f, 0.2f, 0.85f, 0.8f};
static constexpr SiteLook QUARTERS_LOOK = BlockLook(1.25f);
static constexpr float DECK_TANK_HEIGHT = 0.15f;
static constexpr float BRACE_SIDE = 0.02f;
static constexpr float BRACE_LEVEL = 0.11f;
static constexpr float DECK_RAIL_HEIGHT = 0.03f;
static constexpr float DECK_RAIL_THICKNESS = 0.008f;
static constexpr uint32_t SAFETY_YELLOW = 0xFFE3BE2EU;
static constexpr float FLARE_WIDTH = 0.04f;
static constexpr float FLARE_INBOARD = 0.15f;
static constexpr float FLARE_REACH = 0.5f;
static constexpr float FLARE_RISE = 0.25f;
static constexpr float FLARE_TIP = 0.05f;
static constexpr uint32_t FLARE_TINT = 0xFFFF8A2AU;

static constexpr Drum Mound(float cx, float cy, float radius, float rise)
{
	return {cx, cy, radius, 0.0f, RoofShape::Cone, rise};
}

static constexpr Drum Tank(float cx, float cy, float radius)
{
	return {cx, cy, radius, FULL_RISE, RoofShape::Dome, TANK_DOME_RISE};
}

static constexpr Drum Shaft(float cx, float cy, float radius, float height_share = FULL_RISE)
{
	return {cx, cy, radius, height_share, RoofShape::Flat, 0.0f};
}

static constexpr std::array<Drum, 1> LONE_HEAP = {Mound(0.5f, 0.5f, 0.38f, 0.18f)};
static constexpr std::array<Drum, 2> TWIN_HEAPS = {Mound(0.26f, 0.32f, 0.24f, 0.13f), Mound(0.74f, 0.68f, 0.24f, 0.13f)};
static constexpr std::array<std::span<const Drum>, 2> HEAP_LAYOUTS = {LONE_HEAP, TWIN_HEAPS};

static constexpr std::array<Drum, 1> ONE_TANK = {Tank(0.5f, 0.5f, 0.36f)};
static constexpr std::array<Drum, 2> TWO_TANKS = {Tank(0.27f, 0.27f, 0.22f), Tank(0.73f, 0.73f, 0.22f)};
static constexpr std::array<Drum, 4> FOUR_TANKS = {Tank(0.25f, 0.25f, 0.2f), Tank(0.75f, 0.25f, 0.2f), Tank(0.25f, 0.75f, 0.2f), Tank(0.75f, 0.75f, 0.2f)};
static constexpr std::array<std::span<const Drum>, 3> TANK_LAYOUTS = {ONE_TANK, TWO_TANKS, FOUR_TANKS};
static constexpr Drum RAISED_TANK = {0.5f, 0.5f, 0.36f, FULL_RISE, RoofShape::Cone, 0.06f};

static constexpr std::array<Drum, 1> SINGLE_COLUMN = {Shaft(0.5f, 0.5f, 0.12f)};
static constexpr std::array<Drum, 2> PAIRED_COLUMNS = {Shaft(0.38f, 0.5f, 0.12f), Shaft(0.74f, 0.5f, 0.08f, AUX_COLUMN_SHARE)};
static constexpr std::array<Drum, 1> FURNACE = {Drum{0.5f, 0.5f, 0.25f, FULL_RISE, RoofShape::Cone, 0.1f}};
static constexpr std::array<std::span<const Drum>, 3> COLUMN_LAYOUTS = {SINGLE_COLUMN, PAIRED_COLUMNS, FURNACE};

static constexpr std::array<Drum, 2> DECK_TANKS = {Shaft(0.3f, 0.5f, 0.15f), Shaft(0.7f, 0.5f, 0.15f)};

static constexpr size_t FINISH_COUNT = to_underlying(Finish::Painted) + 1;
static constexpr std::array<WallFinish, FINISH_COUNT> WALL_FINISHES = {{
	{Material::Corrugated, WindowGrid::Doors, Swatch::Metal},
	{Material::Brick, WindowGrid::Industrial, Swatch::Brick},
	{Material::Concrete, WindowGrid::Industrial, Swatch::Concrete},
	{Material::Stone, WindowGrid::Arched, Swatch::Stone},
	{Material::Glass, WindowGrid::Curtain, Swatch::Glass},
	{Material::Brick, WindowGrid::Cottage, Swatch::Brick},
	{Material::Timber, WindowGrid::None, Swatch::Timber},
}};

static constexpr std::array<float Plot::*, DIAGDIR_END> SIDE_EDGES = {&Plot::x0, &Plot::y1, &Plot::x1, &Plot::y0};

static const WallFinish &WallFinishOf(Finish finish)
{
	return WALL_FINISHES[to_underlying(finish)];
}

static bool HasWindows(WindowGrid grid)
{
	return grid != WindowGrid::None && grid != WindowGrid::Doors;
}

static float FacadeHeight(WindowGrid grid, float tiles, float ceiling)
{
	if (!HasWindows(grid)) return std::min(tiles, ceiling);
	int storeys = StoreysFor(grid, tiles);
	while (storeys > 1 && WallTiles(grid, storeys) > ceiling) storeys--;
	return WallTiles(grid, storeys);
}

static float LookHeight(const SiteLook &look)
{
	return look.levels * GAME_LEVEL_TILES;
}

Axis AlongEdge(DiagDirection side)
{
	return OtherAxis(DiagDirToAxis(side));
}

static float Inward(DiagDirection side)
{
	return side == DIAGDIR_NE || side == DIAGDIR_NW ? 1.0f : -1.0f;
}

Plot Plot::Inset(DiagDirections sides, float tiles) const
{
	Plot inset = *this;
	for (DiagDirection side : sides) inset.*SIDE_EDGES[side] += Inward(side) * tiles;
	return inset;
}

Plot Plot::Edge(DiagDirection side, float depth) const
{
	Plot edge = *this;
	edge.*SIDE_EDGES[ReverseDiagDir(side)] = this->*SIDE_EDGES[side] + Inward(side) * depth;
	return edge;
}

Plot Plot::Outside(DiagDirection side, float depth) const
{
	Plot outside = *this;
	outside.*SIDE_EDGES[ReverseDiagDir(side)] = this->*SIDE_EDGES[side];
	outside.*SIDE_EDGES[side] -= Inward(side) * depth;
	return outside;
}

Plot Plot::Narrowed(Axis axis, float span) const
{
	return this->Inset(AxisToDiagDirs(axis), (this->Span(axis) - span) / 2.0f);
}

Plot Plot::Scaled(float share) const
{
	return this->Narrowed(AXIS_X, this->Span(AXIS_X) * share).Narrowed(AXIS_Y, this->Span(AXIS_Y) * share);
}

Plot Plot::SetBack(DiagDirection front, const Setback &setback) const
{
	return this->Inset(front, setback.front).Inset(ReverseDiagDir(front), setback.back).Inset(AxisToDiagDirs(AlongEdge(front)), setback.side);
}

Part::Part(SolidKind kind, const Plot &plot)
{
	this->solid.kind = kind;
	this->solid = Within(this->solid, plot);
}

template <typename Change>
Part Part::With(Change change) const
{
	Part part = *this;
	change(part.solid);
	return part;
}

Part Part::On(float base) const
{
	return this->With([base](Solid &solid) { solid.base = base; });
}

Part Part::Detailed() const
{
	return this->With([](Solid &solid) { solid.role = SolidRole::Detail; });
}

Part Part::Fixed(Fixture fixture) const
{
	return this->With([fixture](Solid &solid) {
		solid.role = SolidRole::Detail;
		solid.fixture = fixture;
	});
}

Part Part::Smoking() const
{
	return this->With([](Solid &solid) { solid.fixture = Fixture::Smokestack; });
}

Part Part::Height(float wall) const
{
	return this->With([wall](Solid &solid) { solid.wall = wall; });
}

Part Part::Taper(float taper) const
{
	return this->With([taper](Solid &solid) { solid.taper = taper; });
}

Part Part::Scaled(float share) const
{
	Plot plot = FootprintOf(this->solid).Scaled(share);
	return this->With([&plot](Solid &solid) { solid = Within(solid, plot); });
}

Part Part::Roof(RoofShape shape, float rise) const
{
	return this->With([shape, rise](Solid &solid) {
		solid.roof = shape;
		solid.rise = rise;
	});
}

Part Part::Ridge(Axis ridge) const
{
	return this->With([ridge](Solid &solid) { solid.ridge = ridge; });
}

Part Part::HighSide(DiagDirection side) const
{
	return this->With([side](Solid &solid) { solid.high_side = side; });
}

Part Part::Gable(Axis ridge, float pitch) const
{
	float rise = pitch * FootprintOf(this->solid).Span(OtherAxis(ridge)) / 2.0f;
	return this->Roof(RoofShape::Gable, rise).Ridge(ridge);
}

Part Part::Parapeted(Material roof, uint32_t tint) const
{
	return this->Roof(RoofShape::Parapet).Covered(roof, tint);
}

Part Part::Covered(Material roof, uint32_t tint) const
{
	return this->With([roof, tint](Solid &solid) {
		solid.roof_material = roof;
		solid.roof_tint = tint;
	});
}

Part Part::Clad(Material material, uint32_t tint) const
{
	return this->Covered(material, tint).With([material, tint](Solid &solid) {
		solid.wall_material = material;
		solid.wall_tint = tint;
	});
}

Part Part::Tinted(uint32_t tint) const
{
	return this->With([tint](Solid &solid) {
		solid.wall_tint = tint;
		solid.roof_tint = tint;
	});
}

Part Part::Glazed(uint32_t tint) const
{
	return this->With([tint](Solid &solid) { solid.glass_tint = tint; });
}

Part Part::Facade(Material wall, WindowGrid grid, float height, uint32_t tint, DiagDirections fronts) const
{
	float cap = this->solid.role == SolidRole::Detail ? MAX_STRUCTURE_TILES : TALLEST_BUILDING_TILES;
	return this->Height(FacadeHeight(grid, height, cap - this->solid.base)).With([wall, grid, tint, fronts](Solid &solid) {
		solid.wall_material = wall;
		solid.windows = grid;
		solid.wall_tint = tint;
		solid.fronts = fronts;
		if (wall == Material::Glass) solid.glass_tint = tint;
	});
}

Part Part::Facade(Finish finish, float height, uint32_t tint, DiagDirections fronts) const
{
	const WallFinish &walls = WallFinishOf(finish);
	return this->Facade(walls.wall, walls.grid, height, tint, fronts);
}

static float ShareAt(const Spire &spire, float rise)
{
	return 1.0f - 2.0f * spire.taper * rise / spire.height;
}

Part Spire::Slice(float from, float to) const
{
	const Solid &foot = this->foot;
	float bottom = ShareAt(*this, from);
	return this->foot.Scaled(bottom).On(foot.base + from).Height(to - from).Taper((1.0f - ShareAt(*this, to) / bottom) / 2.0f);
}

/* Like the game, a structure stands at the lowest corner of its tile's own surface, and a foundation levels every corner to its top. */
float SiteFloor(TileIndex tile)
{
	return static_cast<float>(TileGround(tile).Lowest());
}

float SiteFloor(const TileArea &area)
{
	float floor = 0.0f;
	for (TileIndex tile : area) floor = std::max(floor, SiteFloor(tile));
	return floor;
}

BuildingForm SiteForm(TileIndex anchor, float floor, uint8_t size_x, uint8_t size_y)
{
	return {.tx = static_cast<int>(TileX(anchor)), .ty = static_cast<int>(TileY(anchor)), .size_x = size_x, .size_y = size_y, .floor = floor};
}

BuildingForm SiteForm(TileIndex tile)
{
	return SiteForm(tile, SiteFloor(tile));
}

DiagDirections OpenSides(DiagDirections joined)
{
	DiagDirections open = LOT_SIDES;
	return open.Reset(joined);
}

SiteLot CompleteLot(TileIndex tile, uint32_t seed, DiagDirections joined)
{
	return {tile, seed, joined, FULL_RISE, false};
}

Plot LotPlot(const SiteLot &lot)
{
	return Plot{}.Inset(OpenSides(lot.joined), STRUCTURE_INSET);
}

DiagDirections LotFronts(const SiteLot &lot)
{
	DiagDirections open = OpenSides(lot.joined);
	if (open.None()) return {};
	return *open.GetNthSetBit(SubSeed(lot.seed, FRONT_SALT) % open.Count());
}

Axis SeedRidge(uint32_t seed)
{
	return SeedBits(seed, RIDGE_BIT, 1) == 0 ? AXIS_X : AXIS_Y;
}

Axis JoinedAxis(const SiteLot &lot)
{
	if (lot.joined.Any(AxisToDiagDirs(AXIS_Y))) return AXIS_Y;
	if (lot.joined.Any(AxisToDiagDirs(AXIS_X))) return AXIS_X;
	return SeedRidge(lot.seed);
}

static Swatch RoofSwatch(Material roof)
{
	switch (roof) {
		case Material::ClayTile: return Swatch::ClayTile;
		case Material::Slate: return Swatch::Slate;
		case Material::Gravel: return Swatch::Gravel;
		case Material::Membrane: return Swatch::Membrane;
		case Material::Corrugated: return Swatch::Corrugated;
		case Material::GlassRoof: return Swatch::Glass;
		default: return Swatch::Seam;
	}
}

Material SiteRoof(const SiteLook &look)
{
	if (look.finish == Finish::Tile) return Material::ClayTile;
	switch (look.shape) {
		case SiteShape::Block: return Material::Gravel;
		case SiteShape::Sawtooth: return Material::Corrugated;
		default: return Material::MetalSeam;
	}
}

uint32_t FinishTint(Finish finish, uint32_t seed)
{
	return SwatchTint(WallFinishOf(finish).swatch, SeedBits(seed, WALL_SWATCH_FIRST, SWATCH_BITS));
}

uint32_t RoofTint(Material roof, uint32_t seed)
{
	return SwatchTint(RoofSwatch(roof), SeedBits(seed, ROOF_SWATCH_FIRST, SWATCH_BITS));
}

SiteLivery LiveryFor(Finish finish, Material roof, uint32_t seed, uint32_t trim)
{
	uint32_t wall = finish == Finish::Painted ? trim : FinishTint(finish, seed);
	return {wall, RoofTint(roof, seed), trim, BULK_TINT};
}

float LevelAbove(const Solid &host)
{
	return host.Top();
}

/* A band on a parapeted host stands on its coping, as wide as the host itself. */
static float CopingLevel(const Solid &host)
{
	return host.Top() + (host.roof == RoofShape::Parapet ? PARAPET_HEIGHT : 0.0f);
}

Part BlockMass(const Part &footing, const SiteLook &look, const SiteLivery &livery, DiagDirections fronts)
{
	return footing.Facade(look.finish, LookHeight(look), livery.wall, fronts).Parapeted(SiteRoof(look), livery.roof);
}

Part CottageMass(const Plot &plot, float height, Axis ridge, DiagDirections fronts, uint32_t seed)
{
	return Part::Box(plot)
		.Facade(Finish::Tile, height, FinishTint(Finish::Tile, seed), fronts)
		.Gable(ridge, DWELLING_PITCH)
		.Covered(Material::ClayTile, RoofTint(Material::ClayTile, seed));
}

Part CrownBand(const Solid &host, float height, uint32_t tint)
{
	return Part::Box(FootprintOf(host)).On(CopingLevel(host)).Detailed().Height(height).Clad(Material::Metal, tint).Parapeted(host.roof_material, host.roof_tint);
}

static float SlotCentre(const Plot &deck, float slot, uint index)
{
	return deck.x0 + slot * index + slot / 2.0f;
}

/* Units stand in their own slot across the roof, so none overlaps another. */
void AddRooftopKit(BuildingForm &form, const Solid &roof, uint32_t seed)
{
	uint32_t kit = SubSeed(seed, KIT_SALT);
	Plot deck = FootprintOf(roof).Inset(ROOFTOP_MARGIN);
	float slot = deck.Span(AXIS_X) / ROOFTOP_SLOTS;
	uint count = 1 + SeedBits(kit, KIT_COUNT_FIRST, KIT_FIELD_BITS) % ROOFTOP_SLOTS;
	uint shift = SeedBits(kit, KIT_SHIFT_FIRST, KIT_FIELD_BITS);
	for (uint unit = 0; unit < count; unit++) {
		uint first = KIT_UNIT_FIRST + unit * KIT_UNIT_BITS;
		float side = std::min(std::lerp(ROOFTOP_MIN_SIDE, ROOFTOP_MAX_SIDE, SeedShare(kit, first, KIT_FIELD_BITS)), slot);
		float height = std::lerp(ROOFTOP_MIN_HEIGHT, ROOFTOP_MAX_HEIGHT, SeedShare(kit, first + KIT_FIELD_BITS, KIT_FIELD_BITS));
		float cy = std::lerp(deck.y0 + side / 2.0f, deck.y1 - side / 2.0f, SeedShare(kit, first + 2 * KIT_FIELD_BITS, KIT_FIELD_BITS));
		form.Add(Part::Square(SlotCentre(deck, slot, (unit + shift) % ROOFTOP_SLOTS), cy, side).On(LevelAbove(roof)).Fixed(Fixture::RooftopUnit).Height(height).Clad(Material::Metal, ROOFTOP_TINT));
	}
}

static Part Marking(const Plot &plot, float level)
{
	return Part::Decal(plot).On(level).Covered(Material::Plain, COL_PAPER);
}

void AddHelipad(BuildingForm &form, const Plot &deck, float level)
{
	Plot pad = deck.Inset(HELIPAD_MARGIN);
	float cx = pad.Mid(AXIS_X);
	float cy = pad.Mid(AXIS_Y);
	float reach = std::min(pad.Span(AXIS_X), pad.Span(AXIS_Y)) / 2.0f;
	float post_offset = LETTER_POST_OFFSET * reach;
	float stroke = LETTER_STROKE * reach;
	form.Add(Part::Decal(pad).On(level).Covered(Material::Plain, HELIPAD_TINT));
	for (float side : BOTH_SIDES) form.Add(Marking(Plot::Around(cx + side * post_offset, cy, stroke, LETTER_POST_REACH * reach), level));
	form.Add(Marking(Plot::Around(cx, cy, post_offset, stroke), level));
}

void AddStriped(BuildingForm &form, const Spire &spire, uint bands, const std::array<uint32_t, 2> &tints)
{
	float step = spire.height / bands;
	for (uint band = 0; band < bands; band++) form.Add(spire.Slice(band * step, (band + 1) * step).Tinted(tints[band % tints.size()]));
}

void AddLatticeMast(BuildingForm &form, const Part &foot, float height)
{
	Spire mast{foot.Clad(Material::Lattice, COL_STOP), height, MAST_TAPER};
	AddStriped(form, mast, std::max(1U, static_cast<uint>(std::ceil(height / MAST_BAND))), MAST_TINTS);
}

void AddFountain(BuildingForm &form, uint32_t basin_tint)
{
	Part basin = Part::Disc(LOT_CENTRE, LOT_CENTRE, BASIN_RADIUS).Height(BASIN_HEIGHT).Clad(Material::Stone, basin_tint);
	Part pool = Part::Disc(LOT_CENTRE, LOT_CENTRE, POOL_RADIUS).On(LevelAbove(basin)).Height(POOL_DEPTH).Clad(Material::Plain, POOL_TINT);
	form.Add(basin);
	form.Add(pool);
	form.Add(Part::Disc(LOT_CENTRE, LOT_CENTRE, JET_RADIUS).On(LevelAbove(pool)).Height(JET_HEIGHT).Clad(Material::Plain, JET_TINT));
}

FloraPatch GroveFlora(const SiteLook &look)
{
	bool felled = (look.variant & FELLED_GROVE) != 0;
	return {static_cast<TreeKind>(look.variant & ~FELLED_GROVE), felled ? TreeAge::Sapling : TreeAge::Grown, GROVE_TREES};
}

static Part DrumPart(const Drum &drum, float height)
{
	return Part::Disc(drum.cx, drum.cy, drum.radius).Height(height * drum.height_share).Roof(drum.roof, drum.rise);
}

static float Slot(uint index, uint count, float size)
{
	float gap = (1.0f - count * size) / (count + 1);
	return gap + index * (size + gap);
}

template <typename Visit>
static void ForEachCell(const Grid &grid, Visit visit)
{
	for (uint column = 0; column < grid.columns; column++) {
		float x0 = Slot(column, grid.columns, grid.width);
		for (uint row = 0; row < grid.rows; row++) {
			float y0 = Slot(row, grid.rows, grid.depth);
			visit(Plot{x0, y0, x0 + grid.width, y0 + grid.depth}, column * grid.rows + row);
		}
	}
}

static Load PileLoad(Pile pile, const SiteLivery &livery)
{
	switch (pile) {
		case Pile::Logs: return {Material::Timber, LOG_TINT};
		case Pile::Crates: return {Material::Planks, livery.trim};
		default: return {Material::Thatch, BALE_TINT};
	}
}

/* The vent sinks into the slopes far enough that its foot never shows above them. */
static Part RidgeVent(const Solid &shed)
{
	Plot plot = FootprintOf(shed);
	float sink = VENT_SIDE / 2.0f * INDUSTRIAL_PITCH;
	return Part::Square(plot.Mid(AXIS_X), plot.Mid(AXIS_Y), VENT_SIDE).On(shed.Top() - sink).Detailed().Height(VENT_CLEARANCE + sink).Clad(Material::Metal, shed.roof_tint);
}

static void AddBlock(BuildingForm &form, const Part &block, uint32_t seed)
{
	form.Add(block);
	AddRooftopKit(form, block, seed);
}

static void AddWaterTower(BuildingForm &form, uint32_t tint)
{
	for (uint leg = 0; leg < TOWER_LEGS; leg++) {
		double angle = 2.0 * std::numbers::pi * leg / TOWER_LEGS;
		float cx = LOT_CENTRE + TOWER_LEG_SPREAD * static_cast<float>(std::cos(angle));
		float cy = LOT_CENTRE + TOWER_LEG_SPREAD * static_cast<float>(std::sin(angle));
		form.Add(Part::Square(cx, cy, TOWER_LEG_SIDE).Height(TOWER_LEG_HEIGHT).Clad(Material::Metal, tint));
	}
	form.Add(DrumPart(RAISED_TANK, RAISED_TANK_HEIGHT).On(TOWER_LEG_HEIGHT).Clad(Material::Metal, tint));
}

static float SiloCentre(uint index, uint count)
{
	return Slot(index, count, 2.0f * SILO_RADIUS) + SILO_RADIUS;
}

static void AddCrane(BuildingForm &form, float level, uint32_t tint)
{
	Part post = Part::Box(CRANE_POST).On(level).Height(CRANE_HEIGHT).Clad(Material::Metal, tint);
	form.Add(post);
	form.Add(Part::Box(CRANE_BOOM).On(LevelAbove(post)).Height(CRANE_BOOM_DEPTH).Clad(Material::Metal, tint));
}

/* A lattice boom reaches out over the open sea from the deck, its flare tip burning orange at the end. */
static void AddFlareBoom(BuildingForm &form, const Plot &deck, float level, const SiteLot &lot, uint32_t tint)
{
	DiagDirections sea = JoinedSides(lot.tile, [](TileIndex next) { return IsTileType(next, MP_WATER); });
	if (sea.None()) return;
	DiagDirection side = *sea.begin();
	Plot boom = deck.Narrowed(AlongEdge(side), FLARE_WIDTH).Edge(side, FLARE_INBOARD).Inset(side, -FLARE_REACH);
	form.Add(Part::Box(boom).On(level + FLARE_RISE).Height(FLARE_WIDTH).Detailed().Clad(Material::Lattice, tint));
	form.Add(Part::Box(boom.Edge(side, FLARE_TIP)).On(level + FLARE_RISE + FLARE_WIDTH).Height(FLARE_TIP).Detailed().Clad(Material::Plain, FLARE_TINT));
}

static void AddDeckModule(BuildingForm &form, const Solid &platform, const SiteLot &lot, const SiteLook &look, const SiteLivery &livery)
{
	float level = LevelAbove(platform);
	switch (static_cast<DeckModule>(look.variant)) {
		case DeckModule::Crane:
			AddCrane(form, level, livery.trim);
			break;

		case DeckModule::Derrick:
			form.Add(Part::Square(LOT_CENTRE, LOT_CENTRE, DERRICK_SIDE).On(level).Height(DERRICK_HEIGHT).Taper(DERRICK_TAPER).Clad(Material::Lattice, livery.wall));
			AddFlareBoom(form, FootprintOf(platform), level, lot, livery.wall);
			break;

		case DeckModule::Quarters:
			AddBlock(form, BlockMass(Part::Box(QUARTERS).On(level), QUARTERS_LOOK, livery, LotFronts(lot)), lot.seed);
			break;

		case DeckModule::Tanks:
			for (const Drum &tank : DECK_TANKS) form.Add(DrumPart(tank, DECK_TANK_HEIGHT).On(level).Clad(Material::Metal, livery.trim));
			break;

		case DeckModule::Helipad:
			AddHelipad(form, FootprintOf(platform), level);
			break;

		default:
			break;
	}
}

static void BuildNothing(BuildingForm &, const SiteLot &, const SiteLook &, const SiteLivery &)
{
}

static void BuildHeap(BuildingForm &form, const SiteLot &lot, const SiteLook &look, const SiteLivery &livery)
{
	for (const Drum &mound : HEAP_LAYOUTS[SeedBits(lot.seed, HEAP_BIT, 1)]) form.Add(DrumPart(mound, LookHeight(look)).Clad(Material::Gravel, livery.bulk));
}

static void BuildStacks(BuildingForm &form, const SiteLot &lot, const SiteLook &look, const SiteLivery &livery)
{
	Load load = PileLoad(static_cast<Pile>(look.variant), livery);
	ForEachCell(PILE_GRID, [&](const Plot &cell, uint index) {
		float share = SeedShare(lot.seed, PILE_HEIGHT_FIRST + index * PILE_HEIGHT_BITS, PILE_HEIGHT_BITS);
		form.Add(Part::Box(cell).Height(std::lerp(PILE_LOW, PILE_HIGH, share)).Clad(load.material, load.tint));
	});
}

static void BuildPit(BuildingForm &form, const SiteLot &, const SiteLook &, const SiteLivery &)
{
	for (float inset : PIT_INSETS) form.Add(Part::Decal(Plot{}.Inset(inset)).Covered(Material::Plain, PIT_SHADE));
}

static void BuildPlant(BuildingForm &form, const SiteLot &, const SiteLook &, const SiteLivery &)
{
	ForEachCell(PLANT_GRID, [&form](const Plot &cell, uint) { form.Add(Part::Box(cell).Height(PLANT_HEIGHT).Clad(Material::Metal, COL_STEEL)); });
}

static void BuildShed(BuildingForm &form, const SiteLot &lot, const SiteLook &look, const SiteLivery &livery)
{
	Part shed = Part::Box(LotPlot(lot))
		.Facade(look.finish, LookHeight(look), livery.wall, LotFronts(lot))
		.Gable(JoinedAxis(lot), INDUSTRIAL_PITCH)
		.Covered(SiteRoof(look), livery.roof);
	form.Add(shed);
	form.Add(RidgeVent(shed));
}

static void BuildSawtooth(BuildingForm &form, const SiteLot &lot, const SiteLook &, const SiteLivery &livery)
{
	form.Add(Part::Box(LotPlot(lot))
		.Facade(Material::Corrugated, WindowGrid::Industrial, WallTiles(WindowGrid::Industrial, SAWTOOTH_STOREYS), livery.wall, LotFronts(lot))
		.Roof(RoofShape::Sawtooth, SAWTOOTH_RISE)
		.Covered(Material::Corrugated, livery.roof)
		.Glazed(FinishTint(Finish::Glass, SubSeed(lot.seed, GLAZING_SALT))));
}

static void BuildBlock(BuildingForm &form, const SiteLot &lot, const SiteLook &look, const SiteLivery &livery)
{
	AddBlock(form, BlockMass(Part::Box(LotPlot(lot)), look, livery, LotFronts(lot)), lot.seed);
}

static void BuildTanks(BuildingForm &form, const SiteLot &, const SiteLook &look, const SiteLivery &livery)
{
	if ((look.variant & ELEVATED_TANK) != 0) {
		AddWaterTower(form, livery.wall);
		return;
	}
	for (const Drum &tank : TANK_LAYOUTS[look.variant & TANK_COUNT_MASK]) form.Add(DrumPart(tank, LookHeight(look)).Clad(Material::Metal, livery.wall));
}

static void BuildSilos(BuildingForm &form, const SiteLot &, const SiteLook &look, const SiteLivery &livery)
{
	uint count = look.variant;
	float height = LookHeight(look);
	for (uint silo = 0; silo < count; silo++) {
		form.Add(Part::Disc(LOT_CENTRE, SiloCentre(silo, count), SILO_RADIUS).Height(height).Roof(RoofShape::Cone, SILO_CONE).Clad(Material::Concrete, livery.wall));
	}
	Plot conveyor = {LOT_CENTRE - CONVEYOR_WIDTH / 2.0f, SiloCentre(0, count), LOT_CENTRE + CONVEYOR_WIDTH / 2.0f, SiloCentre(count - 1, count)};
	form.Add(Part::Box(conveyor).On(height + SILO_CONE).Detailed().Height(CONVEYOR_HEIGHT).Clad(Material::Metal, COL_STEEL));
}

static void BuildStack(BuildingForm &form, const SiteLot &, const SiteLook &look, const SiteLivery &livery)
{
	Part plinth = Part::Square(LOT_CENTRE, LOT_CENTRE, CHIMNEY_PLINTH_SIDE).Height(CHIMNEY_PLINTH_HEIGHT).Clad(Material::Concrete, livery.wall);
	float height = LookHeight(look);
	Spire chimney{Part::Disc(LOT_CENTRE, LOT_CENTRE, CHIMNEY_RADIUS).On(LevelAbove(plinth)).Clad(Material::Concrete, livery.wall), height, CHIMNEY_TAPER};
	form.Add(plinth);
	if ((look.variant & BANDED_STACK) == 0) {
		form.Add(chimney.Slice(0.0f, height).Covered(Material::Concrete, SOOT_TINT).Smoking());
		return;
	}
	float bands_from = height - CHIMNEY_BANDS * CHIMNEY_BAND;
	form.Add(chimney.Slice(0.0f, bands_from));
	form.Add(chimney.Slice(bands_from, bands_from + CHIMNEY_BAND).Tinted(COL_STOP));
	form.Add(chimney.Slice(bands_from + CHIMNEY_BAND, height).Tinted(COL_PAPER).Covered(Material::Concrete, SOOT_TINT).Smoking());
}

static void BuildColumn(BuildingForm &form, const SiteLot &, const SiteLook &look, const SiteLivery &livery)
{
	for (const Drum &column : COLUMN_LAYOUTS[look.variant]) form.Add(DrumPart(column, LookHeight(look)).Clad(Material::Metal, livery.wall));
}

static void BuildHeadframe(BuildingForm &form, const SiteLot &lot, const SiteLook &look, const SiteLivery &livery)
{
	Part frame = Part::Square(FRAME_X, LOT_CENTRE, FRAME_SIDE).Height(LookHeight(look)).Taper(FRAME_TAPER).Clad(Material::Lattice, livery.wall);
	form.Add(frame);
	form.Add(Part::Square(FRAME_X, LOT_CENTRE, SHEAVE_DECK_SIDE).On(LevelAbove(frame)).Height(SHEAVE_DECK_HEIGHT).Clad(Material::Metal, livery.wall));
	form.Add(Part::Box(WINDING_HOUSE)
		.Facade(look.finish, WINDING_HOUSE_HEIGHT, livery.wall, LotFronts(lot))
		.Gable(AXIS_Y, INDUSTRIAL_PITCH)
		.Covered(SiteRoof(look), livery.roof));
}

static void BuildPumpjack(BuildingForm &form, const SiteLot &, const SiteLook &, const SiteLivery &livery)
{
	Part bed = Part::Box(PUMPJACK_BED).Height(PUMPJACK_BED_HEIGHT).Clad(Material::Concrete, COL_CONCRETE);
	Part post = Part::Square(LOT_CENTRE, LOT_CENTRE, PUMPJACK_POST_SIDE).On(LevelAbove(bed)).Height(PUMPJACK_POST_HEIGHT).Clad(Material::Metal, livery.wall);
	form.Add(bed);
	form.Add(post);
	form.Add(Part::Box(PUMPJACK_BEAM).On(LevelAbove(post)).Height(PUMPJACK_BEAM_HEIGHT).Clad(Material::Metal, livery.trim));
}

/* A rig's deck stands on braced legs and is railed along its open sides. */
static void BuildDeck(BuildingForm &form, const SiteLot &lot, const SiteLook &look, const SiteLivery &livery)
{
	Plot deck = LotPlot(lot);
	Plot legs = deck.Inset(DECK_LEG_INSET);
	for (float x : {legs.x0, legs.x1}) {
		for (float y : {legs.y0, legs.y1}) form.Add(Part::Square(x, y, DECK_LEG_SIDE).Height(DECK_LEVEL).Clad(Material::Metal, livery.wall));
	}
	for (DiagDirection side : LOT_SIDES) form.Add(Part::Box(legs.Edge(side, BRACE_SIDE)).On(BRACE_LEVEL).Height(BRACE_SIDE).Detailed().Clad(Material::Metal, livery.wall));
	Part platform = Part::Box(deck).On(DECK_LEVEL).Height(DECK_THICKNESS).Clad(Material::Metal, livery.wall).Covered(Material::RoofDeck, livery.roof);
	form.Add(platform);
	for (DiagDirection side : OpenSides(lot.joined)) {
		form.Add(Part::Box(deck.Edge(side, DECK_RAIL_THICKNESS)).On(LevelAbove(platform)).Height(DECK_RAIL_HEIGHT).Detailed().Clad(Material::Metal, SAFETY_YELLOW));
	}
	AddDeckModule(form, platform, lot, look, livery);
}

static void BuildFountain(BuildingForm &form, const SiteLot &, const SiteLook &, const SiteLivery &livery)
{
	AddFountain(form, livery.wall);
}

static void BuildMast(BuildingForm &form, const SiteLot &, const SiteLook &look, const SiteLivery &)
{
	AddLatticeMast(form, Part::Square(LOT_CENTRE, LOT_CENTRE, MAST_SIDE), std::min(LookHeight(look), TALLEST_BUILDING_TILES));
}

const std::array<ShapeBuilder, to_underlying(SiteShape::End)> SHAPE_BUILDERS = {
	BuildNothing, BuildNothing, BuildHeap, BuildStacks, BuildPit, BuildPlant, BuildShed, BuildSawtooth, BuildBlock,
	BuildTanks, BuildSilos, BuildStack, BuildColumn, BuildHeadframe, BuildPumpjack, BuildDeck, BuildFountain, BuildMast,
};

static uint32_t UnfinishedTint(uint32_t tint)
{
	return WithAlpha(Mix(tint, COL_BP_NO, UNFINISHED_SHARE), Alpha(tint));
}

/* A wall cut to a share of its height no longer ends on a storey, so it loses its windows. */
static Solid UnfinishedSolid(Solid solid, float rise_share)
{
	solid.base *= rise_share;
	solid.wall *= rise_share;
	solid.rise *= rise_share;
	solid.windows = WindowGrid::None;
	solid.wall_tint = UnfinishedTint(solid.wall_tint);
	solid.roof_tint = UnfinishedTint(solid.roof_tint);
	solid.glass_tint = UnfinishedTint(solid.glass_tint);
	return solid;
}

static void ShowUnfinished(BuildingForm &form, float rise_share)
{
	BuildingForm finished = form;
	form.count = 0;
	for (const Solid &solid : finished.Solids()) {
		if (solid.role == SolidRole::Body) form.Add(UnfinishedSolid(solid, rise_share));
	}
}

void BuildSite(BuildingForm &form, const SiteLot &lot, const SiteLook &look, const SiteLivery &livery)
{
	SHAPE_BUILDERS[to_underlying(look.shape)](form, lot, look, livery);
	if (lot.building) ShowUnfinished(form, lot.rise_share);
}

void BuildSite(BuildingForm &form, const SiteLot &lot, const SiteLook &look, uint32_t trim)
{
	BuildSite(form, lot, look, LiveryFor(look.finish, SiteRoof(look), lot.seed, trim));
}
