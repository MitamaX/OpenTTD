/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file site_shapes.h The shapes industries, objects and transport structures are built from, and the parts their forms are assembled with. */

#ifndef MINI_MAP_SITE_SHAPES_H
#define MINI_MAP_SITE_SHAPES_H

#include <array>

#include "../../map_func.h"
#include "../../tilearea_type.h"
#include "building_form.h"
#include "material_palette.h"

inline constexpr float GAME_LEVEL_TILES = 0.2f;
inline constexpr float INDUSTRIAL_PITCH = 0.30f;
inline constexpr float DWELLING_PITCH = 0.60f;
inline constexpr float STRUCTURE_INSET = 0.08f;
inline constexpr float LOT_CENTRE = 0.5f;
inline constexpr uint32_t FRONT_SALT = 7;
inline constexpr float SAWTOOTH_RISE = 0.12f;
inline constexpr float ROOFTOP_MARGIN = 0.06f;
inline constexpr uint32_t BULK_TINT = 0xFF6B5E50U;
inline constexpr uint32_t SOOT_TINT = 0xFF2A2D30U;
inline constexpr DiagDirections LOT_SIDES{DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_SW, DIAGDIR_NW};

enum class SiteShape : uint8_t { None, Grove, Heap, Stacks, Pit, Plant, Shed, Sawtooth, Block, Tanks, Silos, Stack, Column, Headframe, Pumpjack, Deck, Fountain, Mast, End };
enum class Finish : uint8_t { Metal, Brick, Concrete, Stone, Glass, Tile, Painted };

struct SiteLook {
	SiteShape shape;
	float levels;
	Finish finish;
	uint8_t variant;
};

struct SiteLivery {
	uint32_t wall;
	uint32_t roof;
	uint32_t trim;
	uint32_t bulk;
};

struct SiteLot {
	TileIndex tile;
	uint32_t seed;
	DiagDirections joined;
	float rise_share;
	bool building;
};

using ShapeBuilder = void (*)(BuildingForm &, const SiteLot &, const SiteLook &, const SiteLivery &);
extern const std::array<ShapeBuilder, to_underlying(SiteShape::End)> SHAPE_BUILDERS;

enum class Pile : uint8_t { Logs, Crates, Bales };
enum class ColumnKind : uint8_t { Single, Pair, Furnace };
enum class DeckModule : uint8_t { Bare, Crane, Derrick, Quarters, Tanks, Helipad };
enum class TankCount : uint8_t { One, Two, Four };

inline constexpr uint8_t TANK_COUNT_MASK = 0x03;
inline constexpr uint8_t ELEVATED_TANK = 0x04;
inline constexpr uint8_t BANDED_STACK = 0x01;
inline constexpr uint8_t FELLED_GROVE = 0x80;

constexpr SiteLook ShapeLook(SiteShape shape, float levels = 0.0f, Finish finish = Finish::Metal, uint8_t variant = 0)
{
	return {shape, levels, finish, variant};
}

constexpr SiteLook GroveLook(TreeKind kind) { return ShapeLook(SiteShape::Grove, 0.0f, Finish::Metal, to_underlying(kind)); }
constexpr SiteLook FelledGroveLook(TreeKind kind) { return ShapeLook(SiteShape::Grove, 0.0f, Finish::Metal, static_cast<uint8_t>(to_underlying(kind) | FELLED_GROVE)); }
constexpr SiteLook HeapLook() { return ShapeLook(SiteShape::Heap); }
constexpr SiteLook StacksLook(Pile pile) { return ShapeLook(SiteShape::Stacks, 0.0f, Finish::Metal, to_underlying(pile)); }
constexpr SiteLook PitLook() { return ShapeLook(SiteShape::Pit); }
constexpr SiteLook PlantLook() { return ShapeLook(SiteShape::Plant); }
constexpr SiteLook ShedLook(float levels, Finish finish = Finish::Metal) { return ShapeLook(SiteShape::Shed, levels, finish); }
constexpr SiteLook SawtoothLook() { return ShapeLook(SiteShape::Sawtooth); }
constexpr SiteLook BlockLook(float levels, Finish finish = Finish::Concrete) { return ShapeLook(SiteShape::Block, levels, finish); }
constexpr SiteLook TanksLook(float levels, TankCount count, Finish finish = Finish::Painted) { return ShapeLook(SiteShape::Tanks, levels, finish, to_underlying(count)); }
constexpr SiteLook WaterTowerLook() { return ShapeLook(SiteShape::Tanks, 0.0f, Finish::Painted, ELEVATED_TANK); }
constexpr SiteLook SilosLook(float levels, uint8_t count) { return ShapeLook(SiteShape::Silos, levels, Finish::Concrete, count); }
constexpr SiteLook StackLook(float levels) { return ShapeLook(SiteShape::Stack, levels, Finish::Concrete); }
constexpr SiteLook BandedStackLook(float levels) { return ShapeLook(SiteShape::Stack, levels, Finish::Concrete, BANDED_STACK); }
constexpr SiteLook ColumnLook(float levels, ColumnKind kind = ColumnKind::Single) { return ShapeLook(SiteShape::Column, levels, Finish::Metal, to_underlying(kind)); }
constexpr SiteLook HeadframeLook(float levels) { return ShapeLook(SiteShape::Headframe, levels); }
constexpr SiteLook PumpjackLook() { return ShapeLook(SiteShape::Pumpjack); }
constexpr SiteLook DeckLook(DeckModule module) { return ShapeLook(SiteShape::Deck, 0.0f, Finish::Metal, to_underlying(module)); }
constexpr SiteLook FountainLook() { return ShapeLook(SiteShape::Fountain); }
constexpr SiteLook MastLook(float levels) { return ShapeLook(SiteShape::Mast, levels); }

/* How far a building stands in from the front it faces, the back behind it and the two sides between. */
struct Setback {
	float front;
	float back;
	float side;
};

constexpr Setback Evenly(float depth)
{
	return {depth, depth, depth};
}

Axis AlongEdge(DiagDirection side);

/* A rectangle of a form's floor plan in tiles from the anchor's north corner; a side is the DiagDirection it faces. */
struct Plot {
	float x0 = 0.0f;
	float y0 = 0.0f;
	float x1 = 1.0f;
	float y1 = 1.0f;

	static constexpr Plot Around(float cx, float cy, float half_x, float half_y) { return {cx - half_x, cy - half_y, cx + half_x, cy + half_y}; }

	float Span(Axis axis) const { return axis == AXIS_X ? this->x1 - this->x0 : this->y1 - this->y0; }
	float Mid(Axis axis) const { return axis == AXIS_X ? (this->x0 + this->x1) / 2.0f : (this->y0 + this->y1) / 2.0f; }
	Plot Inset(DiagDirections sides, float tiles) const;
	Plot Inset(float tiles) const { return this->Inset(LOT_SIDES, tiles); }
	Plot Edge(DiagDirection side, float depth) const;
	Plot Outside(DiagDirection side, float depth) const;
	Plot Narrowed(Axis axis, float span) const;
	Plot Scaled(float share) const;
	Plot SetBack(DiagDirection front, const Setback &setback) const;
};

constexpr Plot FootprintOf(const Solid &solid)
{
	return {solid.x0, solid.y0, solid.x1, solid.y1};
}

constexpr Plot FootprintOf(const BuildingForm &form)
{
	return {0.0f, 0.0f, static_cast<float>(form.size_x), static_cast<float>(form.size_y)};
}

constexpr Solid Within(Solid solid, const Plot &plot)
{
	solid.x0 = plot.x0;
	solid.y0 = plot.y0;
	solid.x1 = plot.x1;
	solid.y1 = plot.y1;
	return solid;
}

class Part {
public:
	static Part Box(const Plot &plot) { return Part(SolidKind::Box, plot); }
	static Part Cylinder(const Plot &plot) { return Part(SolidKind::Cylinder, plot); }
	static Part Decal(const Plot &plot) { return Part(SolidKind::Decal, plot); }
	static Part Square(float cx, float cy, float side) { return Box(Plot::Around(cx, cy, side / 2.0f, side / 2.0f)); }
	static Part Disc(float cx, float cy, float radius) { return Cylinder(Plot::Around(cx, cy, radius, radius)); }

	Part On(float base) const;
	Part Detailed() const;
	Part Fixed(Fixture fixture) const;
	Part Height(float wall) const;
	Part Taper(float taper) const;
	Part Scaled(float share) const;
	Part Roof(RoofShape shape, float rise = 0.0f) const;
	Part Ridge(Axis ridge) const;
	Part HighSide(DiagDirection side) const;
	Part Gable(Axis ridge, float pitch) const;
	Part Parapeted(Material roof, uint32_t tint) const;
	Part Covered(Material roof, uint32_t tint) const;
	Part Clad(Material material, uint32_t tint) const;
	Part Tinted(uint32_t tint) const;
	Part Glazed(uint32_t tint) const;
	Part Facade(Material wall, WindowGrid grid, float height, uint32_t tint, DiagDirections fronts) const;
	Part Facade(Finish finish, float height, uint32_t tint, DiagDirections fronts) const;

	operator const Solid &() const { return this->solid; }

private:
	Part(SolidKind kind, const Plot &plot);

	template <typename Change>
	Part With(Change change) const;

	Solid solid;
};

/* A column whose taper runs its whole height, cut into slices that meet without a step. */
struct Spire {
	Part foot;
	float height;
	float taper;

	Part Slice(float from, float to) const;
};

float SiteFloor(TileIndex tile);
float SiteFloor(const TileArea &area);
BuildingForm SiteForm(TileIndex anchor, float floor, uint8_t size_x = 1, uint8_t size_y = 1);
BuildingForm SiteForm(TileIndex tile);

template <typename Joins>
DiagDirections JoinedSides(TileIndex tile, Joins joins)
{
	DiagDirections joined{};
	for (DiagDirection side : LOT_SIDES) {
		TileIndex next = AddTileIndexDiffCWrap(tile, TileIndexDiffCByDiagDir(side));
		if (next != INVALID_TILE && joins(next)) joined.Set(side);
	}
	return joined;
}

DiagDirections OpenSides(DiagDirections joined);
SiteLot CompleteLot(TileIndex tile, uint32_t seed, DiagDirections joined = {});
Plot LotPlot(const SiteLot &lot);
DiagDirections LotFronts(const SiteLot &lot);
Axis SeedRidge(uint32_t seed);
Axis JoinedAxis(const SiteLot &lot);

Material SiteRoof(const SiteLook &look);
uint32_t FinishTint(Finish finish, uint32_t seed);
uint32_t RoofTint(Material roof, uint32_t seed);
SiteLivery LiveryFor(Finish finish, Material roof, uint32_t seed, uint32_t trim);

float LevelAbove(const Solid &host);
Part BlockMass(const Part &footing, const SiteLook &look, const SiteLivery &livery, DiagDirections fronts);
Part CottageMass(const Plot &plot, float height, Axis ridge, DiagDirections fronts, uint32_t seed);
Part CrownBand(const Solid &host, float height, uint32_t tint);
void AddRooftopKit(BuildingForm &form, const Solid &roof, uint32_t seed);
void AddHelipad(BuildingForm &form, const Plot &deck, float level);
void AddStriped(BuildingForm &form, const Spire &spire, uint bands, const std::array<uint32_t, 2> &tints);
void AddLatticeMast(BuildingForm &form, const Part &foot, float height);
void AddFountain(BuildingForm &form, uint32_t basin_tint);

FloraPatch GroveFlora(const SiteLook &look);
void BuildSite(BuildingForm &form, const SiteLot &lot, const SiteLook &look, const SiteLivery &livery);
void BuildSite(BuildingForm &form, const SiteLot &lot, const SiteLook &look, uint32_t trim);

#endif /* MINI_MAP_SITE_SHAPES_H */
