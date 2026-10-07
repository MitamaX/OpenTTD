/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file object_forms.cpp Objects as building forms: transmitters, lighthouses, statues, owned land, headquarters and NewGRF objects. */

#include "../../stdafx.h"
#include "object_forms.h"

#include <algorithm>

#include "../../newgrf_object.h"
#include "../../object_base.h"
#include "../../object_map.h"
#include "../../tile_map.h"
#include "../../water_map.h"
#include "../core/tones.h"
#include "site_shapes.h"

#include "../../safeguards.h"

using ObjectBuilder = void (*)(BuildingForm &form, const SiteLot &lot, uint32_t trim);

static constexpr uint8_t OBJECT_COLOUR_MASK = 0x0F;

static constexpr float LIGHTHOUSE_RADIUS = 0.17f;
static constexpr float LIGHTHOUSE_TOP_RADIUS = 0.11f;
static constexpr float LIGHTHOUSE_HEIGHT = 1.3f;
static constexpr float LIGHTHOUSE_TAPER = (1.0f - LIGHTHOUSE_TOP_RADIUS / LIGHTHOUSE_RADIUS) / 2.0f;
static constexpr uint LIGHTHOUSE_BANDS = 4;
static constexpr std::array<uint32_t, 2> LIGHTHOUSE_TINTS = {COL_PAPER, COL_STOP};
static constexpr float GALLERY_RADIUS = 0.15f;
static constexpr float GALLERY_HEIGHT = 0.01f;
static constexpr float LANTERN_RADIUS = 0.10f;
static constexpr float LANTERN_HEIGHT = 0.12f;
static constexpr float LANTERN_CAP = 0.07f;
static constexpr uint32_t LANTERN_TINT = 0xFFFFE9A8U;
static constexpr float KEEPER_DEPTH = 0.30f;
static constexpr float KEEPER_LENGTH = 0.45f;
static constexpr float KEEPER_MARGIN = 0.02f;
static constexpr float KEEPER_HEIGHT = 0.25f;
static constexpr uint KEEPER_ACROSS_BIT = 18;
static constexpr uint KEEPER_ALONG_BIT = 19;

static constexpr float PLINTH_INSET = 0.25f;
static constexpr float PLINTH_HEIGHT = 0.2f;
static constexpr float RIM_WIDTH = 0.04f;
static constexpr float FIGURE_RADIUS = 0.07f;
static constexpr float FIGURE_TAPER = 0.14f;
static constexpr float FIGURE_HEIGHT = 0.28f;
static constexpr uint32_t BRONZE_TINT = 0xFF5E8A6EU;

static constexpr float POLE_SIDE = 0.02f;
static constexpr float POLE_HEIGHT = 0.32f;
static constexpr float FLAG_THICKNESS = 0.01f;
static constexpr float FLAG_LENGTH = 0.28f;
static constexpr float FLAG_BASE = 0.2f;
static constexpr float FLAG_HEIGHT = 0.12f;
static constexpr float FLAG_HOIST = LOT_CENTRE + POLE_SIDE / 2.0f;
static constexpr Plot FLAG = {LOT_CENTRE - FLAG_THICKNESS / 2.0f, FLAG_HOIST, LOT_CENTRE + FLAG_THICKNESS / 2.0f, FLAG_HOIST + FLAG_LENGTH};
static constexpr float BOUNDARY_WIDTH = 0.05f;
static constexpr uint BOUNDARY_ALPHA = 140;

static constexpr float HQ_CENTRE = 1.0f;
static constexpr float HQ_COTTAGE_HALF = 0.65f;
static constexpr float HQ_COTTAGE_HEIGHT = 0.25f;
static constexpr float HQ_VILLA_HALF = 0.75f;
static constexpr float HQ_VILLA_HEIGHT = 0.40f;
static constexpr float HQ_OFFICES_HALF = 0.8f;
static constexpr float HQ_OFFICES_HEIGHT = 0.7f;
static constexpr float HQ_PODIUM_HALF = 0.72f;
static constexpr float HQ_TOWER_PODIUM_HEIGHT = 0.3f;
static constexpr float HQ_TOWER_HALF = 0.55f;
static constexpr float HQ_TOWER_HEIGHT = 1.4f;
static constexpr float HQ_CROWN_HEIGHT = 0.08f;
static constexpr float HQ_SKYSCRAPER_PODIUM_HEIGHT = 0.4f;
static constexpr float HQ_SHAFT_HALF = 0.5f;
static constexpr float HQ_SETBACK_HALF = 0.38f;
static constexpr float HQ_SETBACK_HEIGHT = 0.3f;
static constexpr float HQ_ANTENNA_SIDE = 0.06f;
static constexpr float HQ_ANTENNA_HEIGHT = 0.3f;
static constexpr uint HQ_GLASS_TRIM_SHARE = 64;
static constexpr float HQ_GROUNDS_HALF = 0.97f;
static constexpr float HQ_PLANTER_HALF = 0.14f;
static constexpr float HQ_PLANTER_INSET = 0.06f;
static constexpr float HQ_PLANTER_HEIGHT = 0.05f;
static constexpr uint32_t HQ_PLANTER_TINT = 0xFF4E7A3AU;
static constexpr float HQ_FLAG_INSET = 0.08f;
static constexpr std::array<float, 2> HQ_FLAG_OFFSETS = {-0.18f, 0.18f};
static constexpr float HQ_FLAG_POLE_HEIGHT = 0.45f;
static constexpr float HQ_FLAG_LENGTH = 0.16f;
static constexpr float HQ_FLAG_HEIGHT = 0.08f;

static constexpr std::array<Finish, 3> CUSTOM_FINISHES = {Finish::Brick, Finish::Concrete, Finish::Stone};
static constexpr uint8_t CUSTOM_MAST_LEVELS = 6;
static constexpr uint PAVING_TRIM_SHARE = 40;
static constexpr uint PAVING_ALPHA = 160;

static uint32_t ObjectSeed(TileIndex tile)
{
	return Hash32(tile.base() ^ GetObjectRandomBits(tile));
}

static uint32_t ObjectTrim(const Object &object)
{
	return _company_rgb[object.colour & OBJECT_COLOUR_MASK];
}

static void BuildTransmitter(BuildingForm &form, const SiteLot &lot, uint32_t trim)
{
	BuildSite(form, lot, MastLook(ObjectSpec::GetByTile(lot.tile)->height), trim);
}

/* The keeper's house takes a corner clear of the tower and faces out across the tile. */
static Part KeeperHouse(const SiteLot &lot)
{
	DiagDirection across = SeedBits(lot.seed, KEEPER_ACROSS_BIT, 1) == 0 ? DIAGDIR_NE : DIAGDIR_SW;
	DiagDirection along = SeedBits(lot.seed, KEEPER_ALONG_BIT, 1) == 0 ? DIAGDIR_NW : DIAGDIR_SE;
	Plot corner = Plot{}.Edge(across, KEEPER_DEPTH + KEEPER_MARGIN).Edge(along, KEEPER_LENGTH + KEEPER_MARGIN).Inset({across, along}, KEEPER_MARGIN);
	return CottageMass(corner, KEEPER_HEIGHT, AXIS_Y, across, lot.seed);
}

static void BuildLighthouse(BuildingForm &form, const SiteLot &lot, uint32_t)
{
	Spire tower{Part::Disc(LOT_CENTRE, LOT_CENTRE, LIGHTHOUSE_RADIUS).Clad(Material::Render, COL_PAPER), LIGHTHOUSE_HEIGHT, LIGHTHOUSE_TAPER};
	Part gallery = Part::Disc(LOT_CENTRE, LOT_CENTRE, GALLERY_RADIUS).On(LIGHTHOUSE_HEIGHT).Height(GALLERY_HEIGHT).Clad(Material::Metal, SOOT_TINT);
	AddStriped(form, tower, LIGHTHOUSE_BANDS, LIGHTHOUSE_TINTS);
	form.Add(gallery);
	form.Add(Part::Disc(LOT_CENTRE, LOT_CENTRE, LANTERN_RADIUS)
		.On(LevelAbove(gallery))
		.Height(LANTERN_HEIGHT)
		.Roof(RoofShape::Cone, LANTERN_CAP)
		.Clad(Material::Glass, LANTERN_TINT)
		.Covered(Material::MetalSeam, SOOT_TINT));
	form.Add(KeeperHouse(lot));
}

static void BuildStatue(BuildingForm &form, const SiteLot &lot, uint32_t trim)
{
	uint32_t stone = FinishTint(Finish::Stone, lot.seed);
	Plot top = Plot{}.Inset(PLINTH_INSET);
	Part plinth = Part::Box(top).Height(PLINTH_HEIGHT).Clad(Material::Stone, stone);
	float level = LevelAbove(plinth);
	form.Add(plinth);
	form.Add(Part::Decal(top).On(level).Covered(Material::Plain, trim));
	form.Add(Part::Decal(top.Inset(RIM_WIDTH)).On(level).Covered(Material::Stone, stone));
	form.Add(Part::Disc(LOT_CENTRE, LOT_CENTRE, FIGURE_RADIUS).On(level).Height(FIGURE_HEIGHT).Taper(FIGURE_TAPER).Clad(Material::Metal, BRONZE_TINT));
}

static void BuildOwnedLand(BuildingForm &form, const SiteLot &lot, uint32_t trim)
{
	Owner owner = GetTileOwner(lot.tile);
	DiagDirections claimed = JoinedSides(lot.tile, [owner](TileIndex next) {
		return IsObjectTypeTile(next, OBJECT_OWNED_LAND) && GetTileOwner(next) == owner;
	});
	for (DiagDirection side : OpenSides(claimed)) form.Add(Part::Decal(Plot{}.Edge(side, BOUNDARY_WIDTH)).Covered(Material::Plain, WithAlpha(trim, BOUNDARY_ALPHA)));
	form.Add(Part::Square(LOT_CENTRE, LOT_CENTRE, POLE_SIDE).Height(POLE_HEIGHT).Clad(Material::Metal, COL_STEEL));
	form.Add(Part::Box(FLAG).On(FLAG_BASE).Height(FLAG_HEIGHT).Clad(Material::Plain, trim));
}

static Plot HqPlot(float half)
{
	return Plot::Around(HQ_CENTRE, HQ_CENTRE, half, half);
}

static Part HqMass(const Part &footing, Material wall, WindowGrid grid, float height, uint32_t tint, const SiteLot &lot)
{
	return footing.Facade(wall, grid, height, tint, LotFronts(lot)).Parapeted(Material::Membrane, RoofTint(Material::Membrane, lot.seed));
}

static Part HqPodium(const SiteLot &lot, float height)
{
	return HqMass(Part::Box(HqPlot(HQ_PODIUM_HALF)), Material::Concrete, WindowGrid::Shopfront, height, FinishTint(Finish::Concrete, lot.seed), lot);
}

/* A flag in the company's colour flies from a pole standing at this spot, out along the front it stands before. */
static void AddFlagpole(BuildingForm &form, float x, float y, Axis along, uint32_t trim)
{
	Plot flag = along == AXIS_X ? Plot{x, y - FLAG_THICKNESS / 2.0f, x + HQ_FLAG_LENGTH, y + FLAG_THICKNESS / 2.0f} : Plot{x - FLAG_THICKNESS / 2.0f, y, x + FLAG_THICKNESS / 2.0f, y + HQ_FLAG_LENGTH};
	form.Add(Part::Square(x, y, POLE_SIDE).Detailed().Height(HQ_FLAG_POLE_HEIGHT).Clad(Material::Metal, COL_STEEL));
	form.Add(Part::Box(flag).Detailed().On(HQ_FLAG_POLE_HEIGHT - HQ_FLAG_HEIGHT).Height(HQ_FLAG_HEIGHT).Clad(Material::Plain, trim));
}

/* Headquarters stand on a paved forecourt with planted beds at its far corners and the company's flags flying before the front. */
static void AddHqGrounds(BuildingForm &form, const SiteLot &lot, uint32_t trim)
{
	Plot grounds = HqPlot(HQ_GROUNDS_HALF);
	DiagDirections fronts = LotFronts(lot);
	DiagDirection front = fronts.Any() ? *fronts.GetNthSetBit(0) : DIAGDIR_SE;
	Axis along = AlongEdge(front);
	form.Add(Part::Decal(grounds).Covered(Material::Stone, FinishTint(Finish::Stone, lot.seed)));
	Plot back = grounds.Edge(ReverseDiagDir(front), HQ_PLANTER_INSET + 2.0f * HQ_PLANTER_HALF).Inset(ReverseDiagDir(front), HQ_PLANTER_INSET);
	for (float side : {-1.0f, 1.0f}) {
		float mid = HQ_CENTRE + side * (HQ_GROUNDS_HALF - HQ_PLANTER_INSET - HQ_PLANTER_HALF);
		Plot bed = along == AXIS_X ? Plot{mid - HQ_PLANTER_HALF, back.y0, mid + HQ_PLANTER_HALF, back.y1} : Plot{back.x0, mid - HQ_PLANTER_HALF, back.x1, mid + HQ_PLANTER_HALF};
		form.Add(Part::Box(bed).Detailed().Height(HQ_PLANTER_HEIGHT).Clad(Material::Hedge, HQ_PLANTER_TINT));
	}
	Plot kerb = grounds.Edge(front, HQ_FLAG_INSET);
	for (float offset : HQ_FLAG_OFFSETS) {
		float x = along == AXIS_X ? HQ_CENTRE + offset : kerb.Mid(AXIS_X);
		float y = along == AXIS_X ? kerb.Mid(AXIS_Y) : HQ_CENTRE + offset;
		AddFlagpole(form, x, y, along, trim);
	}
}

static void AddCottageHq(BuildingForm &form, const SiteLot &lot, uint32_t)
{
	form.Add(CottageMass(HqPlot(HQ_COTTAGE_HALF), HQ_COTTAGE_HEIGHT, SeedRidge(lot.seed), LotFronts(lot), lot.seed));
}

static void AddVillaHq(BuildingForm &form, const SiteLot &lot, uint32_t)
{
	form.Add(CottageMass(HqPlot(HQ_VILLA_HALF), HQ_VILLA_HEIGHT, SeedRidge(lot.seed), LotFronts(lot), lot.seed));
}

static void AddOfficesHq(BuildingForm &form, const SiteLot &lot, uint32_t)
{
	Part offices = HqMass(Part::Box(HqPlot(HQ_OFFICES_HALF)), Material::Concrete, WindowGrid::Office, HQ_OFFICES_HEIGHT, FinishTint(Finish::Concrete, lot.seed), lot);
	form.Add(offices);
	AddRooftopKit(form, offices, lot.seed);
}

static void AddTowerHq(BuildingForm &form, const SiteLot &lot, uint32_t trim)
{
	Part podium = HqPodium(lot, HQ_TOWER_PODIUM_HEIGHT);
	Part tower = HqMass(Part::Box(HqPlot(HQ_TOWER_HALF)).On(LevelAbove(podium)), Material::Stone, WindowGrid::Office, HQ_TOWER_HEIGHT, FinishTint(Finish::Stone, lot.seed), lot);
	form.Add(podium);
	form.Add(tower);
	form.Add(CrownBand(tower, HQ_CROWN_HEIGHT, trim));
	AddRooftopKit(form, tower, lot.seed);
}

static void AddSkyscraperHq(BuildingForm &form, const SiteLot &lot, uint32_t trim)
{
	uint32_t glass = Mix(FinishTint(Finish::Glass, lot.seed), trim, HQ_GLASS_TRIM_SHARE);
	Part podium = HqPodium(lot, HQ_SKYSCRAPER_PODIUM_HEIGHT);
	float shaft_base = LevelAbove(podium);
	Part shaft = HqMass(Part::Box(HqPlot(HQ_SHAFT_HALF)).On(shaft_base), Material::Glass, WindowGrid::Curtain, TALLEST_BUILDING_TILES - shaft_base, glass, lot);
	Part setback = HqMass(Part::Box(HqPlot(HQ_SETBACK_HALF)).On(LevelAbove(shaft)).Detailed(), Material::Glass, WindowGrid::Curtain, HQ_SETBACK_HEIGHT, glass, lot);
	form.Add(podium);
	form.Add(shaft);
	form.Add(setback);
	AddLatticeMast(form, Part::Square(HQ_CENTRE, HQ_CENTRE, HQ_ANTENNA_SIDE).On(LevelAbove(setback)).Detailed(), HQ_ANTENNA_HEIGHT);
}

static constexpr std::array HEADQUARTERS_STAGES = {AddCottageHq, AddVillaHq, AddOfficesHq, AddTowerHq, AddSkyscraperHq};

/* The game keeps the headquarters' growth stage in the animation frame of its tiles. */
static void BuildHeadquarters(BuildingForm &form, const SiteLot &lot, uint32_t trim)
{
	size_t stage = std::min<size_t>(GetAnimationFrame(lot.tile), HEADQUARTERS_STAGES.size() - 1);
	AddHqGrounds(form, lot, trim);
	HEADQUARTERS_STAGES[stage](form, lot, trim);
}

static constexpr std::array<ObjectBuilder, NEW_OBJECT_OFFSET> ORIGINAL_OBJECTS = {BuildTransmitter, BuildLighthouse, BuildStatue, BuildOwnedLand, BuildHeadquarters};

static SiteLook CustomObjectLook(const ObjectSpec &spec, TileIndex tile, uint32_t seed)
{
	if (IsTileOnWater(tile)) return DeckLook(DeckModule::Bare);
	if (spec.size == OBJECT_SIZE_1X1 && spec.height >= CUSTOM_MAST_LEVELS) return MastLook(spec.height);
	return BlockLook(spec.height, SeedPick(CUSTOM_FINISHES, seed));
}

/* A NewGRF object is drawn tile by tile, its colours and finish taken from the whole object so its tiles agree. */
static BuildingForm CustomObjectForm(TileIndex tile)
{
	const ObjectSpec &spec = *ObjectSpec::GetByTile(tile);
	const Object &object = *Object::GetByTile(tile);
	uint32_t object_seed = ObjectSeed(object.location.tile);
	uint32_t trim = ObjectTrim(object);
	BuildingForm form = SiteForm(tile);
	if (spec.height == 0) {
		form.Add(Part::Decal(Plot{}).Covered(Material::Asphalt, WithAlpha(Mix(COL_ROAD, trim, PAVING_TRIM_SHARE), PAVING_ALPHA)));
		return form;
	}
	ObjectID id = GetObjectIndex(tile);
	DiagDirections joined = JoinedSides(tile, [id](TileIndex next) { return IsTileType(next, MP_OBJECT) && GetObjectIndex(next) == id; });
	SiteLook look = CustomObjectLook(spec, tile, object_seed);
	BuildSite(form, CompleteLot(tile, ObjectSeed(tile), joined), look, LiveryFor(look.finish, SiteRoof(look), object_seed, trim));
	return form;
}

std::optional<BuildingForm> ObjectForm(TileIndex tile)
{
	ObjectType type = GetObjectType(tile);
	if (type >= NEW_OBJECT_OFFSET) return CustomObjectForm(tile);
	const Object &object = *Object::GetByTile(tile);
	const TileArea &area = object.location;
	BuildingForm form = SiteForm(area.tile, SiteFloor(area), static_cast<uint8_t>(area.w), static_cast<uint8_t>(area.h));
	ORIGINAL_OBJECTS[type](form, CompleteLot(area.tile, ObjectSeed(area.tile)), ObjectTrim(object));
	return form;
}
