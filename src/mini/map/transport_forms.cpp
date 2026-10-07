/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file transport_forms.cpp Depots and the buildings of stations that are not rail stations, as building forms. */

#include "../../stdafx.h"
#include "transport_forms.h"

#include "../../airport.h"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../../direction_func.h"
#include "../../newgrf_airporttiles.h"
#include "../../rail_map.h"
#include "../../road_map.h"
#include "../../station_base.h"
#include "../../station_map.h"
#include "../../table/airporttile_ids.h"
#include "../../tile_map.h"
#include "../../water_map.h"
#include "../core/tones.h"
#include "airfield_marks.h"
#include "site_shapes.h"

#include "../../safeguards.h"

enum class AirportPart : uint8_t { Terminal, Rotunda, Tower, Hangar, Pier, Radar, RadioMast, Cottage, LowBlock, Heliport, End };

using AirportBuilder = void (*)(BuildingForm &form, const SiteLot &lot, uint32_t company);

struct DepotShed {
	Setback setback;
	float height;
};

struct BayShelter {
	Finish finish;
	float height;
	RoofShape roof;
	Material cover;
};

static constexpr DepotShed RAIL_DEPOT = {{0.2f, 0.0f, 0.12f}, 0.4f};
static constexpr DepotShed ROAD_DEPOT = {{0.24f, 0.1f, 0.14f}, 0.32f};
static constexpr uint COMPANY_ROOF_SHARE = 128;
static constexpr uint8_t ONE_TILE = 1;
static constexpr uint8_t SHIP_DEPOT_LENGTH = 2;
static constexpr float SHIP_DEPOT_SIDE = 0.15f;
static constexpr float SHIP_DEPOT_BASE = 0.05f;
static constexpr float SHIP_DEPOT_HEIGHT = 0.3f;
static constexpr uint SHIP_DEPOT_OWNER_SHARE = 96;

static constexpr SiteLook TERMINAL_LOOK = BlockLook(4.0f, Finish::Glass);
static constexpr SiteLook LOW_BLOCK_LOOK = BlockLook(2.0f, Finish::Concrete);
static constexpr SiteLook HELIPORT_LOOK = BlockLook(5.0f, Finish::Glass);
static constexpr SiteLook RADIO_MAST_LOOK = MastLook(8.75f);
static constexpr float ROTUNDA_RADIUS = 0.42f;
static constexpr float ROTUNDA_HEIGHT = 0.75f;
static constexpr float TOWER_BASE_SIDE = 0.62f;
static constexpr float TOWER_BASE_HEIGHT = 0.22f;
static constexpr float TOWER_SHAFT_RADIUS = 0.1f;
static constexpr float TOWER_SHAFT_HEIGHT = 0.9f;
static constexpr float TOWER_SHAFT_TAPER = 0.12f;
static constexpr float TOWER_GALLERY_RADIUS = 0.2f;
static constexpr float TOWER_GALLERY_HEIGHT = 0.05f;
static constexpr float TOWER_CAB_RADIUS = 0.2f;
static constexpr float TOWER_CAB_HEIGHT = 0.15f;
static constexpr float TOWER_CAB_SPLAY = -0.1f;
static constexpr float TOWER_LID_RADIUS = 0.24f;
static constexpr float TOWER_LID_HEIGHT = 0.03f;
static constexpr float TOWER_MAST_RADIUS = 0.012f;
static constexpr float TOWER_MAST_HEIGHT = 0.22f;
static constexpr float HANGAR_INSET = 0.05f;
static constexpr float HANGAR_HEIGHT = 0.45f;
static constexpr float HANGAR_PITCH = 0.35f;
static constexpr DiagDirection DRAWN_HANGAR_EXIT = DIAGDIR_SE;
static constexpr float PIER_WIDTH = 0.4f;
static constexpr float PIER_HEIGHT = 0.3f;
static constexpr float RADAR_POST_SIDE = 0.2f;
static constexpr float RADAR_POST_HEIGHT = 0.2f;
static constexpr Plot RADAR_DISH = Plot::Around(LOT_CENTRE, LOT_CENTRE, 0.125f, 0.025f);
static constexpr float RADAR_DISH_HEIGHT = 0.1f;
static constexpr int AIRPORT_COTTAGE_STOREYS = 1;

static constexpr float WAREHOUSE_INSET = 0.12f;
static constexpr float WAREHOUSE_HEIGHT = 0.35f;

static constexpr float BUOY_RADIUS = 0.07f;
static constexpr float BUOY_HULL_HEIGHT = 0.07f;
static constexpr float BUOY_TOP_HEIGHT = 0.04f;
static constexpr float BUOY_CONE = 0.05f;

static constexpr float BAY_SHELTER_DEPTH = 0.4f;
static constexpr float BAY_BAND_HEIGHT = 0.04f;
static constexpr BayShelter FREIGHT_SHELTER = {Finish::Concrete, 0.4f, RoofShape::Parapet, Material::Gravel};
static constexpr BayShelter PASSENGER_SHELTER = {Finish::Glass, 0.24f, RoofShape::Flat, Material::GlassRoof};

/* Runways, aprons, grass and fences keep End: nothing stands on them. */
static constexpr std::array<AirportPart, NEW_AIRPORTTILE_OFFSET> AIRPORT_PARTS = [] {
	std::array<AirportPart, NEW_AIRPORTTILE_OFFSET> parts{};
	parts.fill(AirportPart::End);
	auto mark = [&parts](AirportPart part, std::initializer_list<AirportTiles> tiles) {
		for (AirportTiles tile : tiles) parts[tile] = part;
	};
	mark(AirportPart::Terminal, {APT_BUILDING_1, APT_BUILDING_2, APT_BUILDING_3});
	mark(AirportPart::Rotunda, {APT_ROUND_TERMINAL});
	mark(AirportPart::Tower, {APT_TOWER, APT_TOWER_FENCE_SW});
	mark(AirportPart::Hangar, {APT_DEPOT_SE, APT_SMALL_DEPOT_SE});
	mark(AirportPart::Pier, {APT_STAND_1, APT_STAND_PIER_NE, APT_PIER_NW_NE, APT_PIER});
	mark(AirportPart::Radar, {APT_RADAR_GRASS_FENCE_SW, APT_RADAR_FENCE_SW, APT_RADAR_FENCE_NE});
	mark(AirportPart::RadioMast, {APT_RADIO_TOWER_FENCE_NE});
	mark(AirportPart::Cottage, {APT_SMALL_BUILDING_1, APT_SMALL_BUILDING_2, APT_SMALL_BUILDING_3});
	mark(AirportPart::LowBlock, {APT_LOW_BUILDING_FENCE_N, APT_LOW_BUILDING_FENCE_NW, APT_LOW_BUILDING});
	mark(AirportPart::Heliport, {APT_HELIPORT});
	return parts;
}();

static uint32_t TileSeed(TileIndex tile)
{
	return Hash32(tile.base());
}

static uint32_t OwnerTint(TileIndex tile)
{
	Owner owner = GetTileOwner(tile);
	return Company::IsValidID(owner) ? _company_rgb[_company_colours[owner]] : COL_OBJ;
}

static uint32_t DepotRoof(uint32_t seed, uint32_t company)
{
	return Mix(RoofTint(Material::MetalSeam, seed), company, COMPANY_ROOF_SHARE);
}

static BuildingForm LandDepotForm(TileIndex tile, DiagDirection exit, const DepotShed &shed)
{
	uint32_t seed = TileSeed(tile);
	BuildingForm form = SiteForm(tile);
	form.Add(Part::Box(Plot{}.SetBack(exit, shed.setback))
		.Facade(Material::Brick, WindowGrid::Doors, shed.height, FinishTint(Finish::Brick, seed), exit)
		.Gable(DiagDirToAxis(exit), INDUSTRIAL_PITCH)
		.Covered(Material::MetalSeam, DepotRoof(seed, OwnerTint(tile))));
	return form;
}

/* A low boathouse with doors onto the water at both ends, its walls in a touch of the owner's colour under a plain metal roof. */
static BuildingForm ShipDepotForm(TileIndex tile)
{
	TileIndex north = GetShipDepotNorthTile(tile);
	Axis axis = GetShipDepotAxis(north);
	uint32_t seed = TileSeed(north);
	BuildingForm form = SiteForm(north, SiteFloor(north), axis == AXIS_X ? SHIP_DEPOT_LENGTH : ONE_TILE, axis == AXIS_Y ? SHIP_DEPOT_LENGTH : ONE_TILE);
	Plot hall = FootprintOf(form).Inset(AxisToDiagDirs(OtherAxis(axis)), SHIP_DEPOT_SIDE);
	uint32_t walls = Mix(FinishTint(Finish::Metal, seed), OwnerTint(north), SHIP_DEPOT_OWNER_SHARE);
	form.Add(Part::Box(hall)
		.On(SHIP_DEPOT_BASE)
		.Facade(Material::Corrugated, WindowGrid::Doors, SHIP_DEPOT_HEIGHT, walls, AxisToDiagDirs(axis))
		.Gable(axis, INDUSTRIAL_PITCH)
		.Covered(Material::MetalSeam, RoofTint(Material::MetalSeam, seed)));
	return form;
}

std::optional<BuildingForm> DepotForm(TileIndex tile)
{
	if (IsRailDepotTile(tile)) return LandDepotForm(tile, GetRailDepotDirection(tile), RAIL_DEPOT);
	if (IsRoadDepotTile(tile)) return LandDepotForm(tile, GetRoadDepotDirection(tile), ROAD_DEPOT);
	if (IsShipDepotTile(tile)) return ShipDepotForm(tile);
	return std::nullopt;
}

static std::optional<AirportPart> AirportPartOf(TileIndex tile)
{
	std::optional<StationGfx> stand_in = AirportStandIn(tile);
	if (!stand_in.has_value() || AIRPORT_PARTS[*stand_in] == AirportPart::End) return std::nullopt;
	return AIRPORT_PARTS[*stand_in];
}

static bool IsConcourse(std::optional<AirportPart> part)
{
	return part == AirportPart::Terminal || part == AirportPart::Pier;
}

static DiagDirections ConcourseSides(TileIndex tile)
{
	StationID station = GetStationIndex(tile);
	return JoinedSides(tile, [station](TileIndex next) {
		return IsAirportTile(next) && GetStationIndex(next) == station && IsConcourse(AirportPartOf(next));
	});
}

static Part GlassMass(const Part &footing, float height, uint32_t seed)
{
	return footing.Facade(Finish::Glass, height, FinishTint(Finish::Glass, seed), {}).Covered(Material::Membrane, RoofTint(Material::Membrane, seed));
}

static void AddTerminal(BuildingForm &form, const SiteLot &lot, uint32_t company)
{
	BuildSite(form, lot, TERMINAL_LOOK, company);
}

static void AddRotunda(BuildingForm &form, const SiteLot &lot, uint32_t)
{
	form.Add(GlassMass(Part::Disc(LOT_CENTRE, LOT_CENTRE, ROTUNDA_RADIUS), ROTUNDA_HEIGHT, lot.seed));
}

/* A control tower rises from a low office on a tapering shaft to a gallery, a cab glazed all round leaning out over it, and a mast on its lid. */
static void AddControlTower(BuildingForm &form, const SiteLot &lot, uint32_t)
{
	uint32_t concrete = FinishTint(Finish::Concrete, lot.seed);
	Part base = Part::Square(LOT_CENTRE, LOT_CENTRE, TOWER_BASE_SIDE).Facade(Finish::Concrete, TOWER_BASE_HEIGHT, concrete, LotFronts(lot)).Covered(Material::Gravel, RoofTint(Material::Gravel, lot.seed));
	Part shaft = Part::Disc(LOT_CENTRE, LOT_CENTRE, TOWER_SHAFT_RADIUS).On(LevelAbove(base)).Height(TOWER_SHAFT_HEIGHT).Taper(TOWER_SHAFT_TAPER).Clad(Material::Concrete, concrete);
	Part gallery = Part::Disc(LOT_CENTRE, LOT_CENTRE, TOWER_GALLERY_RADIUS).On(LevelAbove(shaft)).Height(TOWER_GALLERY_HEIGHT).Clad(Material::Concrete, concrete);
	Part cab = Part::Disc(LOT_CENTRE, LOT_CENTRE, TOWER_CAB_RADIUS).On(LevelAbove(gallery)).Height(TOWER_CAB_HEIGHT).Taper(TOWER_CAB_SPLAY).Clad(Material::Glass, FinishTint(Finish::Glass, lot.seed));
	Part lid = Part::Disc(LOT_CENTRE, LOT_CENTRE, TOWER_LID_RADIUS).On(LevelAbove(cab)).Height(TOWER_LID_HEIGHT).Clad(Material::Metal, SOOT_TINT);
	for (const Part &part : {base, shaft, gallery, cab, lid}) form.Add(part);
	form.Add(Part::Disc(LOT_CENTRE, LOT_CENTRE, TOWER_MAST_RADIUS).On(LevelAbove(lid)).Height(TOWER_MAST_HEIGHT).Detailed().Clad(Material::Metal, COL_PAPER));
}

static DiagDirection HangarExit(TileIndex tile)
{
	if (!IsHangar(tile)) return DRAWN_HANGAR_EXIT;
	return DirToDiagDir(Station::GetByTile(tile)->airport.GetHangarExitDirection(tile));
}

static void AddHangar(BuildingForm &form, const SiteLot &lot, uint32_t)
{
	DiagDirection exit = HangarExit(lot.tile);
	form.Add(Part::Box(Plot{}.Inset(HANGAR_INSET))
		.Facade(Finish::Metal, HANGAR_HEIGHT, FinishTint(Finish::Metal, lot.seed), exit)
		.Gable(DiagDirToAxis(exit), HANGAR_PITCH)
		.Covered(Material::MetalSeam, RoofTint(Material::MetalSeam, lot.seed)));
}

static void AddPier(BuildingForm &form, const SiteLot &lot, uint32_t)
{
	form.Add(GlassMass(Part::Box(Plot{}.Narrowed(OtherAxis(JoinedAxis(lot)), PIER_WIDTH)), PIER_HEIGHT, lot.seed));
}

static void AddRadar(BuildingForm &form, const SiteLot &, uint32_t)
{
	Part post = Part::Square(LOT_CENTRE, LOT_CENTRE, RADAR_POST_SIDE).Height(RADAR_POST_HEIGHT).Clad(Material::Concrete, COL_CONCRETE);
	form.Add(post);
	form.Add(Part::Box(RADAR_DISH).On(LevelAbove(post)).Height(RADAR_DISH_HEIGHT).Clad(Material::Metal, COL_PAPER));
}

static void AddRadioMast(BuildingForm &form, const SiteLot &lot, uint32_t company)
{
	BuildSite(form, lot, RADIO_MAST_LOOK, company);
}

static void AddAirportCottage(BuildingForm &form, const SiteLot &lot, uint32_t)
{
	form.Add(CottageMass(LotPlot(lot), WallTiles(WindowGrid::Cottage, AIRPORT_COTTAGE_STOREYS), SeedRidge(lot.seed), LotFronts(lot), lot.seed));
}

static void AddLowBlock(BuildingForm &form, const SiteLot &lot, uint32_t company)
{
	BuildSite(form, lot, LOW_BLOCK_LOOK, company);
}

static void AddHeliport(BuildingForm &form, const SiteLot &lot, uint32_t company)
{
	SiteLivery livery = LiveryFor(HELIPORT_LOOK.finish, SiteRoof(HELIPORT_LOOK), lot.seed, company);
	Part block = BlockMass(Part::Box(LotPlot(lot)), HELIPORT_LOOK, livery, LotFronts(lot));
	form.Add(block);
	AddHelipad(form, LotPlot(lot), LevelAbove(block));
}

static constexpr std::array<AirportBuilder, to_underlying(AirportPart::End)> AIRPORT_BUILDERS = {
	AddTerminal, AddRotunda, AddControlTower, AddHangar, AddPier, AddRadar, AddRadioMast, AddAirportCottage, AddLowBlock, AddHeliport,
};

static std::optional<BuildingForm> AirportForm(TileIndex tile)
{
	std::optional<AirportPart> part = AirportPartOf(tile);
	if (!part.has_value()) return std::nullopt;
	DiagDirections joined = IsConcourse(part) ? ConcourseSides(tile) : DiagDirections{};
	BuildingForm form = SiteForm(tile);
	AIRPORT_BUILDERS[to_underlying(*part)](form, CompleteLot(tile, TileSeed(tile), joined), OwnerTint(tile));
	return form;
}

static std::optional<BuildingForm> DockForm(TileIndex tile)
{
	if (IsDockWaterPart(tile)) return std::nullopt;
	DiagDirection water = GetDockDirection(tile);
	BuildingForm form = SiteForm(tile);
	form.Add(Part::Box(Plot{}.Inset(WAREHOUSE_INSET))
		.Facade(Finish::Metal, WAREHOUSE_HEIGHT, FinishTint(Finish::Metal, TileSeed(tile)), water)
		.Gable(AlongEdge(water), INDUSTRIAL_PITCH)
		.Covered(Material::MetalSeam, OwnerTint(tile)));
	return form;
}

static std::optional<BuildingForm> BuoyForm(TileIndex tile)
{
	BuildingForm form = SiteForm(tile);
	Part hull = Part::Disc(LOT_CENTRE, LOT_CENTRE, BUOY_RADIUS).Height(BUOY_HULL_HEIGHT).Clad(Material::Metal, COL_STOP);
	form.Add(hull);
	form.Add(Part::Disc(LOT_CENTRE, LOT_CENTRE, BUOY_RADIUS).On(LevelAbove(hull)).Height(BUOY_TOP_HEIGHT).Roof(RoofShape::Cone, BUOY_CONE).Clad(Material::Metal, COL_PAPER));
	return form;
}

static std::optional<BuildingForm> OilRigForm(TileIndex tile)
{
	BuildingForm form = SiteForm(tile);
	BuildSite(form, CompleteLot(tile, TileSeed(tile)), DeckLook(DeckModule::Helipad), COL_STEEL);
	return form;
}

/* The shelter stands at the back of the bay, its front toward the entrance. */
static std::optional<BuildingForm> BayStopForm(TileIndex tile)
{
	if (!IsBayRoadStopTile(tile)) return std::nullopt;
	DiagDirection entrance = GetBayRoadStopDir(tile);
	const BayShelter &shelter = IsTruckStop(tile) ? FREIGHT_SHELTER : PASSENGER_SHELTER;
	uint32_t seed = TileSeed(tile);
	BuildingForm form = SiteForm(tile);
	Part body = Part::Box(Plot{}.Edge(ReverseDiagDir(entrance), BAY_SHELTER_DEPTH))
		.Facade(shelter.finish, shelter.height, FinishTint(shelter.finish, seed), entrance)
		.Roof(shelter.roof)
		.Covered(shelter.cover, RoofTint(shelter.cover, seed));
	form.Add(body);
	form.Add(CrownBand(body, BAY_BAND_HEIGHT, OwnerTint(tile)));
	return form;
}

std::optional<BuildingForm> StationForm(TileIndex tile)
{
	switch (GetStationType(tile)) {
		case StationType::Airport: return AirportForm(tile);
		case StationType::Dock: return DockForm(tile);
		case StationType::Buoy: return BuoyForm(tile);
		case StationType::Oilrig: return OilRigForm(tile);
		case StationType::Truck:
		case StationType::Bus: return BayStopForm(tile);
		default: return std::nullopt;
	}
}
