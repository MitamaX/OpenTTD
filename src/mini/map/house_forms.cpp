/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file house_forms.cpp Town houses as building forms: site, front, cladding, masses, roofs, details and construction. */

#include "../../stdafx.h"
#include "house_forms.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include "../../direction_func.h"
#include "../../house.h"
#include "../../map_func.h"
#include "../../road_map.h"
#include "../../settings_type.h"
#include "../../tile_map.h"
#include "../../town.h"
#include "../../town_map.h"
#include "../core/tones.h"
#include "house_kinds.h"
#include "material_palette.h"
#include "site_shapes.h"

#include "../../safeguards.h"

static constexpr size_t SLOT_CHOICES = 4;
static constexpr size_t CLIMATES = to_underlying(LandscapeType::Toyland) + 1;
static constexpr size_t ERAS = to_underlying(Era::End);

using MaterialSlots = std::array<Material, SLOT_CHOICES>;
using RoofSlots = std::array<RoofShape, SLOT_CHOICES>;

struct HouseCover {
	Material material;
	uint32_t tint;
};

struct TepeeCone {
	float x;
	float y;
	float rise;
};

struct HouseSite {
	TileIndex north;
	HouseID id;
	uint8_t size_x;
	uint8_t size_y;
	HouseKind kind;
	WindowGrid grid;
	int storeys;
	uint32_t seed;
	LandscapeType climate;
	Era era;
	DiagDirection front;
	bool on_road;
	HouseCover wall;
	uint32_t glass;
};

using HouseBuilder = void (*)(BuildingForm &form, const HouseSite &site);

template <typename T>
static constexpr std::array<T, SLOT_CHOICES> Only(T choice)
{
	return {choice, choice, choice, choice};
}

static constexpr uint8_t WIDE_HOUSE_TILES = 2;
static constexpr std::array<DiagDirection, DIAGDIR_END> FRONT_PREFERENCE = {DIAGDIR_SW, DIAGDIR_SE, DIAGDIR_NW, DIAGDIR_NE};
static constexpr float CONTAIN_TOLERANCE = 1e-4f;

static constexpr MaterialSlots COTTAGE_WALLS = {Material::Render, Material::Render, Material::Timber, Material::Stone};
static constexpr MaterialSlots TERRACE_WALLS = {Material::Brick, Material::Brick, Material::Brick, Material::Render};
static constexpr MaterialSlots HOTEL_WALLS = {Material::Concrete, Material::Render, Material::Concrete, Material::Stone};
static constexpr MaterialSlots HALL_WALLS = {Material::Stone, Material::Render, Material::Stone, Material::Render};
static constexpr MaterialSlots EARLY_TOWER_WALLS = {Material::Concrete, Material::Concrete, Material::Glass, Material::Stone};
static constexpr MaterialSlots LATE_TOWER_WALLS = {Material::Glass, Material::Glass, Material::Concrete, Material::Glass};
static constexpr TimerGameCalendar::Year LATE_TOWER_YEAR{1975};
static constexpr std::array<MaterialSlots, ERAS> HOUSE_WALLS = {{
	{Material::Brick, Material::Render, Material::Brick, Material::Render},
	{Material::Brick, Material::Render, Material::Render, Material::Brick},
	{Material::Render, Material::Render, Material::Brick, Material::Render},
}};
static constexpr std::array<MaterialSlots, ERAS> FLATS_WALLS = {{
	{Material::Brick, Material::Brick, Material::Render, Material::Stone},
	{Material::Brick, Material::Render, Material::Brick, Material::Render},
	{Material::Concrete, Material::Concrete, Material::Render, Material::Brick},
}};
static constexpr std::array<MaterialSlots, ERAS> SHOPS_WALLS = {{
	{Material::Brick, Material::Render, Material::Stone, Material::Brick},
	{Material::Brick, Material::Render, Material::Stone, Material::Brick},
	{Material::Concrete, Material::Render, Material::Concrete, Material::Concrete},
}};
static constexpr std::array<MaterialSlots, ERAS> OFFICE_WALLS = {{
	{Material::Stone, Material::Brick, Material::Stone, Material::Concrete},
	{Material::Stone, Material::Brick, Material::Stone, Material::Concrete},
	{Material::Concrete, Material::Concrete, Material::Glass, Material::Stone},
}};

static constexpr MaterialSlots TILED_ROOFS = {Material::ClayTile, Material::ClayTile, Material::Slate, Material::Slate};
static constexpr MaterialSlots THATCHED_ROOFS = {Material::Thatch, Material::Thatch, Material::ClayTile, Material::Slate};
static constexpr MaterialSlots ARCTIC_ROOFS = {Material::MetalSeam, Material::MetalSeam, Material::Slate, Material::Shingle};

static constexpr std::array<RoofSlots, CLIMATES> DWELLING_ROOFS = {{
	{RoofShape::Gable, RoofShape::Gable, RoofShape::Hip, RoofShape::Hip},
	{RoofShape::Gable, RoofShape::Gable, RoofShape::Gable, RoofShape::Hip},
	{RoofShape::Parapet, RoofShape::Parapet, RoofShape::Flat, RoofShape::Hip},
	{RoofShape::Pyramid, RoofShape::Gable, RoofShape::Gable, RoofShape::Hip},
}};
static constexpr std::array<RoofSlots, ERAS> BLOCK_ROOFS = {{
	{RoofShape::Hip, RoofShape::Gable, RoofShape::Parapet, RoofShape::Hip},
	{RoofShape::Hip, RoofShape::Parapet, RoofShape::Parapet, RoofShape::Hip},
	Only(RoofShape::Parapet),
}};
static constexpr RoofSlots TOY_BLOCK_ROOFS = {RoofShape::Pyramid, RoofShape::Parapet, RoofShape::Pyramid, RoofShape::Parapet};
static constexpr RoofSlots WAREHOUSE_ROOFS = {RoofShape::Sawtooth, RoofShape::Sawtooth, RoofShape::Gable, RoofShape::Flat};

static constexpr float DEFAULT_SETBACK = 0.06f;
static constexpr float SQUARE_TOLERANCE = 0.08f;
static constexpr std::array<float, CLIMATES> CLIMATE_PITCH = {DWELLING_PITCH, 0.78f, 0.40f, 0.84f};

static constexpr Setback COTTAGE_SETBACK = {0.18f, 0.22f, 0.16f};
static constexpr Setback HOUSE_SETBACK = {0.14f, 0.20f, 0.12f};
static constexpr float DWELLING_JITTER = 0.08f;
static constexpr std::array<std::optional<DiagDirection>, SLOT_CHOICES> ANNEX_SIDES = {std::nullopt, DIAGDIR_NW, DIAGDIR_SE, std::nullopt};
static constexpr float ANNEX_WIDTH = 0.26f;
static constexpr float ANNEX_REAR_SHARE = 0.3f;
static constexpr float ANNEX_END_GAP = 0.03f;
static constexpr float ANNEX_WALL_SHARE = 0.9f;

static constexpr float CHIMNEY_SIDE = 0.05f;
static constexpr float CHIMNEY_END_OFFSET = 0.25f;
static constexpr float CHIMNEY_ABOVE_RIDGE = 0.05f;

static constexpr float ROOFTOP_GAP = 0.04f;
static constexpr float TANK_SIDE = 0.10f;
static constexpr float TANK_HEIGHT = 0.10f;
static constexpr uint32_t TANK_TINT = 0xFFB8B8B0U;

static constexpr Setback TERRACE_SETBACK = {0.12f, 0.16f, 0.0f};
static constexpr float FLATS_SETBACK = 0.10f;
static constexpr float FLATS_JITTER = 0.06f;

static constexpr Setback SHOPS_SETBACK = {0.03f, 0.10f, 0.02f};
static constexpr float AWNING_DEPTH = 0.03f;
static constexpr float AWNING_THICKNESS = 0.015f;

static constexpr float PENTHOUSE_LENGTH = 0.16f;
static constexpr float PENTHOUSE_WIDTH = 0.24f;
static constexpr float PENTHOUSE_HEIGHT = 0.10f;

static constexpr float PODIUM_SETBACK = 0.04f;
static constexpr int TOWER_PODIUM_STOREYS = 2;
static constexpr float SHAFT_INSET = 0.10f;
static constexpr float GLASS_TOWER_SETBACK = 0.14f;
static constexpr float CROWN_INSET = 0.2f;
static constexpr float CROWN_HEIGHT = 0.06f;
static constexpr double CROWN_SHADE = 0.9;
static constexpr int HOTEL_PODIUM_STOREYS = 1;
static constexpr float SLAB_SHORT_INSET = 0.14f;
static constexpr float SLAB_LONG_INSET = 0.06f;
static constexpr float HOTEL_SIGN_LENGTH = 0.3f;
static constexpr float HOTEL_SIGN_DEPTH = 0.04f;
static constexpr float HOTEL_SIGN_HEIGHT = 0.06f;

static constexpr float NAVE_LENGTH = 0.74f;
static constexpr float NAVE_WIDTH = 0.36f;
static constexpr float NAVE_PITCH = 0.84f;
static constexpr int NAVE_STOREYS = 1;
static constexpr float BELFRY_SIDE = 0.24f;
static constexpr float BELFRY_HEIGHT = 0.56f;
static constexpr std::array<float, CLIMATES> SPIRE_RISE = {0.42f, 0.50f, 0.14f, 0.42f};
static constexpr uint32_t WHITEWASH = SwatchTint(Swatch::Adobe, 0);

static constexpr float LOBBY_SHARE = 0.4f;
static constexpr int LOBBY_STOREYS = 1;
static constexpr float AUDITORIUM_HEIGHT = 0.30f;
static constexpr float THEATRE_DOME_INSET = 0.05f;
static constexpr float THEATRE_DOME_RISE = 0.16f;

static constexpr float CINEMA_SETBACK = 0.05f;
static constexpr int CINEMA_STOREYS = 2;
static constexpr float BLADE_DEPTH = 0.03f;
static constexpr float BLADE_LENGTH = 0.16f;
static constexpr float BLADE_HEIGHT = 0.22f;

static constexpr float MALL_SETBACK = 0.05f;
static constexpr int MALL_STOREYS = 2;
static constexpr uint SKYLIGHT_ROWS = 2;
static constexpr float SKYLIGHT_WIDTH = 0.08f;
static constexpr float MALL_CANOPY_LENGTH = 0.5f;

static constexpr float WAREHOUSE_SETBACK = 0.05f;
static constexpr float WAREHOUSE_HEIGHT = 0.30f;

static constexpr float FIELD_INSET = 0.36f;
static constexpr float FIELD_SHARE = 1.0f - 2 * FIELD_INSET;
static constexpr uint32_t FIELD_TINT = 0xFF5E9A48U;
static constexpr float END_STAND_DEPTH = 0.32f;
static constexpr float SIDE_STAND_DEPTH = 0.30f;
static constexpr float STAND_HEIGHT = 0.06f;
static constexpr float STAND_RISE = 0.16f;
static constexpr float MAST_SIDE = 0.03f;
static constexpr float MAST_TOP = 0.6f;

static constexpr float PATH_WIDTH = 0.06f;
static constexpr uint32_t PATH_TINT = 0xFFBFAF8AU;
static constexpr uint8_t PARK_TREES = 3;

static constexpr float PLINTH_SIDE = 0.36f;
static constexpr float PLINTH_HEIGHT = 0.06f;
static constexpr uint32_t PLINTH_TINT = 0xFFC8C0AEU;
static constexpr float FIGURE_SIDE = 0.10f;
static constexpr float FIGURE_HEIGHT = 0.22f;
static constexpr uint32_t FIGURE_TINT = 0xFF5F7A64U;

static constexpr float IGLOO_RADIUS = 0.30f;
static constexpr float IGLOO_RISE = 0.26f;
static constexpr uint32_t IGLOO_TINT = 0xFFE8F2F8U;
static constexpr float IGLOO_DOOR_WIDTH = 0.12f;
static constexpr float IGLOO_DOOR_DEPTH = 0.14f;
static constexpr float IGLOO_DOOR_HEIGHT = 0.10f;

static constexpr float TEPEE_RADIUS = 0.18f;
static constexpr uint32_t TEPEE_TINT = 0xFFD9C3A0U;
static constexpr std::array<TepeeCone, 2> TEPEE_CONES = {{{0.30f, 0.32f, 0.48f}, {0.68f, 0.64f, 0.42f}}};

static constexpr float TEAPOT_RADIUS = 0.30f;
static constexpr float TEAPOT_WALL = 0.12f;
static constexpr float TEAPOT_RISE = 0.20f;
static constexpr float SPOUT_LENGTH = 0.16f;
static constexpr float SPOUT_WIDTH = 0.05f;
static constexpr float SPOUT_BASE = 0.10f;
static constexpr float SPOUT_HEIGHT = 0.06f;
static constexpr float KNOB_RADIUS = 0.025f;
static constexpr float KNOB_HEIGHT = 0.04f;

static constexpr float PIGGY_HALF_WIDTH = 0.18f;
static constexpr float PIGGY_HALF_LENGTH = 0.30f;
static constexpr float PIGGY_WALL = 0.06f;
static constexpr float PIGGY_RISE = 0.26f;
static constexpr uint32_t PIGGY_TINT = 0xFFF0A3B8U;
static constexpr float SNOUT_WIDTH = 0.10f;
static constexpr float SNOUT_DEPTH = 0.06f;
static constexpr float SNOUT_HEIGHT = 0.08f;

static constexpr uint8_t SITE_STAGE = 0;
static constexpr uint8_t FRAME_STAGE = 1;
static constexpr float CONSTRUCTION_TICKS = 8.0f;
static constexpr float FRAME_START_SHARE = 0.2f;
static constexpr float PAD_MARGIN = 0.05f;
static constexpr uint32_t PAD_TINT = 0xFF7D6952U;
static constexpr float SLAB_HEIGHT = 0.02f;
static constexpr uint32_t SLAB_TINT = 0xFFA7A39BU;
static constexpr float PILE_SIDE = 0.08f;
static constexpr float PILE_MARGIN = 0.04f;
static constexpr std::array<std::pair<DiagDirection, DiagDirection>, 2> PILE_CORNERS = {{{DIAGDIR_NE, DIAGDIR_NW}, {DIAGDIR_SW, DIAGDIR_SE}}};
static constexpr uint32_t FRAME_TIMBER = 0xFFB98E5AU;
static constexpr uint32_t FRAME_STEEL = 0xFF8C9399U;
static constexpr uint32_t RAW_PLASTER = 0xFFBDB6AAU;
static constexpr uint RAW_PLASTER_SHARE = 120;
static constexpr uint32_t UNDERLAY_TINT = 0xFF5A5652U;
static constexpr float SCAFFOLD_NEAR = 0.01f;
static constexpr float SCAFFOLD_FAR = 0.03f;
static constexpr float SCAFFOLD_ABOVE = 0.04f;
static constexpr uint32_t SCAFFOLD_TINT = 0xFFD2A95AU;

static Axis LongAxis(const Plot &plot)
{
	return plot.Span(AXIS_X) >= plot.Span(AXIS_Y) ? AXIS_X : AXIS_Y;
}

static Plot Cell(const Plot &plot, Axis axis, uint index, uint count)
{
	float step = plot.Span(axis) / count;
	DiagDirection high = AxisToDiagDir(axis);
	return plot.Inset(ReverseDiagDir(high), index * step).Inset(high, (count - 1 - index) * step);
}

static Plot Union(const Plot &a, const Plot &b)
{
	return {std::min(a.x0, b.x0), std::min(a.y0, b.y0), std::max(a.x1, b.x1), std::max(a.y1, b.y1)};
}

static bool Contains(const Plot &outer, const Plot &inner)
{
	return inner.x0 > outer.x0 - CONTAIN_TOLERANCE && inner.y0 > outer.y0 - CONTAIN_TOLERANCE
		&& inner.x1 < outer.x1 + CONTAIN_TOLERANCE && inner.y1 < outer.y1 + CONTAIN_TOLERANCE;
}

static Setback Jittered(const Setback &setback, uint32_t seed, float span)
{
	float jitter = span * SETBACK_JITTER.ShareOf(seed) - span / 2;
	return {setback.front + jitter, setback.back + jitter, setback.side + jitter};
}

static void Place(BuildingForm &form, Solid solid)
{
	solid.x0 = std::max(solid.x0, 0.0f);
	solid.y0 = std::max(solid.y0, 0.0f);
	solid.x1 = std::min(solid.x1, static_cast<float>(form.size_x));
	solid.y1 = std::min(solid.y1, static_cast<float>(form.size_y));
	form.Add(solid);
}

static WindowGrid WearableGrid(Material wall, WindowGrid wanted)
{
	if (wanted == WindowGrid::None || HasFacade(wall, wanted)) return wanted;
	auto strip = std::ranges::find(FACADE_STRIPS, wall, &FacadeStrip::wall);
	return strip == FACADE_STRIPS.end() ? WindowGrid::None : strip->grid;
}

static Part Walled(const Plot &plot, const HouseSite &site, WindowGrid wanted, float wall)
{
	return Part::Box(plot).Facade(site.wall.material, WearableGrid(site.wall.material, wanted), wall, site.wall.tint, site.front).Glazed(site.glass);
}

static Part Storeyed(const Plot &plot, const HouseSite &site, WindowGrid wanted, int storeys)
{
	return Walled(plot, site, wanted, WallTiles(WearableGrid(site.wall.material, wanted), storeys));
}

static uint32_t Weathered(uint32_t tint, uint32_t seed)
{
	return TintJitter(tint, seed, TINT_JITTER_FIRST);
}

static Swatch WallSwatch(Material material, LandscapeType climate)
{
	if (climate == LandscapeType::Toyland) return Swatch::Candy;
	switch (material) {
		case Material::Brick: return Swatch::Brick;
		case Material::Render: return climate == LandscapeType::Tropic ? Swatch::Adobe : Swatch::Render;
		case Material::Timber: return Swatch::Timber;
		case Material::Stone: return Swatch::Stone;
		case Material::Corrugated: return Swatch::Metal;
		default: return Swatch::Concrete;
	}
}

static Swatch RoofSwatch(Material material, LandscapeType climate)
{
	if (climate == LandscapeType::Toyland) return Swatch::CandyRoof;
	switch (material) {
		case Material::Slate: return Swatch::Slate;
		case Material::Shingle: return Swatch::Shingle;
		case Material::Thatch: return Swatch::Thatch;
		case Material::MetalSeam: return Swatch::Seam;
		case Material::Gravel: return Swatch::Gravel;
		case Material::Membrane: return Swatch::Membrane;
		case Material::RoofDeck: return Swatch::Plaster;
		case Material::Corrugated: return Swatch::Corrugated;
		default: return Swatch::ClayTile;
	}
}

static HouseCover Covering(const HouseSite &site, Material material, Swatch swatch)
{
	return {material, Weathered(SwatchTint(swatch, ROOF_COLOUR.Of(site.seed)), site.seed)};
}

static HouseCover RoofCover(const HouseSite &site, Material material)
{
	return Covering(site, material, RoofSwatch(material, site.climate));
}

static HouseCover CopperCover(const HouseSite &site)
{
	return Covering(site, Material::MetalSeam, Swatch::Copper);
}

static uint32_t AccentTint(const HouseSite &site)
{
	return SwatchTint(Swatch::Awning, ACCENT_COLOUR.Of(site.seed));
}

static MaterialSlots WallSlots(const HouseSite &site)
{
	size_t era = to_underlying(site.era);
	switch (site.kind) {
		case HouseKind::Cottage: return COTTAGE_WALLS;
		case HouseKind::House: return HOUSE_WALLS[era];
		case HouseKind::Terrace: return TERRACE_WALLS;
		case HouseKind::Flats: return FLATS_WALLS[era];
		case HouseKind::Shops: return SHOPS_WALLS[era];
		case HouseKind::Office: return OFFICE_WALLS[era];
		case HouseKind::Tower: return HouseSpec::Get(site.id)->min_year < LATE_TOWER_YEAR ? EARLY_TOWER_WALLS : LATE_TOWER_WALLS;
		case HouseKind::Hotel: return HOTEL_WALLS;
		case HouseKind::GlassTower: return Only(Material::Glass);
		case HouseKind::Warehouse: return Only(Material::Corrugated);
		case HouseKind::Theatre:
		case HouseKind::Cinema: return HALL_WALLS;
		case HouseKind::Church: return Only(Material::Stone);
		case HouseKind::Mall:
		case HouseKind::Stadium: return Only(Material::Concrete);
		default: return Only(Material::Plain);
	}
}

static Material WallMaterialOf(const HouseSite &site)
{
	Material slotted = SeedPick(WallSlots(site), WALL_MATERIAL_SLOT.Of(site.seed));
	switch (site.kind) {
		case HouseKind::Cottage:
		case HouseKind::House:
		case HouseKind::Terrace:
			return site.climate == LandscapeType::Arctic ? Material::Timber : slotted;

		case HouseKind::Church:
			return site.climate == LandscapeType::Arctic || site.climate == LandscapeType::Tropic ? Material::Render : slotted;

		default:
			return slotted;
	}
}

static void Clad(HouseSite &site)
{
	site.glass = SwatchTint(Swatch::Glass, SubSeed(site.seed, GLASS_SALT));
	Material material = WallMaterialOf(site);
	uint32_t tint = material == Material::Glass ? site.glass : SwatchTint(WallSwatch(material, site.climate), WALL_COLOUR.Of(site.seed));

	if (std::optional<RemapTint> remap = GameRemapTint(site.north, site.id); remap.has_value()) {
		material = remap->wall.value_or(material);
		tint = remap->argb;
		if (site.kind == HouseKind::GlassTower) site.glass = remap->argb;
	}
	site.wall = {material, Weathered(tint, site.seed)};
}

static bool IsTownRoad(int x, int y)
{
	if (x < 0 || y < 0) return false;
	TileIndex tile = TileXY(x, y);
	return IsTileType(tile, MP_ROAD) && !IsRoadDepot(tile);
}

static uint RoadsBeside(const HouseSite &site, DiagDirection side)
{
	TileIndexDiffC step = TileIndexDiffCByDiagDir(side);
	int north_x = TileX(site.north);
	int north_y = TileY(site.north);
	uint roads = 0;
	for (int dx = 0; dx < site.size_x; dx++) {
		for (int dy = 0; dy < site.size_y; dy++) {
			int x = dx + step.x;
			int y = dy + step.y;
			bool beyond = x < 0 || y < 0 || x >= site.size_x || y >= site.size_y;
			if (beyond && IsTownRoad(north_x + x, north_y + y)) roads++;
		}
	}
	return roads;
}

static void FaceRoad(HouseSite &site)
{
	uint most = 0;
	site.front = FRONT_PREFERENCE.front();
	for (DiagDirection side : FRONT_PREFERENCE) {
		uint roads = RoadsBeside(site, side);
		if (roads <= most) continue;
		most = roads;
		site.front = side;
	}
	site.on_road = most > 0;
}

static HouseSite IdentifyHouse(TileIndex tile)
{
	HouseID north_id = GetHouseType(tile);
	TileIndex north = tile + GetHouseNorthPart(north_id);
	const HouseSpec *spec = HouseSpec::Get(north_id);

	HouseSite site{};
	site.north = north;
	site.id = north_id;
	site.size_x = spec->building_flags.Any(BUILDING_2_TILES_X) ? WIDE_HOUSE_TILES : 1;
	site.size_y = spec->building_flags.Any(BUILDING_2_TILES_Y) ? WIDE_HOUSE_TILES : 1;
	site.seed = HouseSeed(north, north_id);

	double pop_per_tile = spec->population / static_cast<double>(site.size_x * site.size_y);
	site.kind = KindOfHouse(north_id, pop_per_tile);
	std::optional<KindRule> rule = RuleOf(site.kind);
	site.grid = rule.has_value() ? rule->grid : WindowGrid::None;
	site.storeys = rule.has_value() ? StoreysOf(*rule, pop_per_tile, site.seed) : 1;
	return site;
}

static HouseSite ReadHouseSite(TileIndex tile)
{
	HouseSite site = IdentifyHouse(tile);
	site.climate = _settings_game.game_creation.landscape;
	site.era = EraOf(*HouseSpec::Get(site.id));
	FaceRoad(site);
	Clad(site);
	return site;
}

static float ClimatePitch(const HouseSite &site)
{
	return CLIMATE_PITCH[to_underlying(site.climate)];
}

static RoofSlots BlockRoofSlots(const HouseSite &site)
{
	switch (site.climate) {
		case LandscapeType::Toyland: return TOY_BLOCK_ROOFS;
		case LandscapeType::Tropic: return Only(RoofShape::Parapet);
		default: return BLOCK_ROOFS[to_underlying(site.era)];
	}
}

static RoofSlots RoofShapeSlots(const HouseSite &site)
{
	switch (site.kind) {
		case HouseKind::Cottage:
		case HouseKind::House: return DWELLING_ROOFS[to_underlying(site.climate)];
		case HouseKind::Terrace: return Only(site.climate == LandscapeType::Tropic ? RoofShape::Parapet : RoofShape::Gable);
		case HouseKind::Flats:
		case HouseKind::Shops: return BlockRoofSlots(site);
		case HouseKind::Warehouse: return WAREHOUSE_ROOFS;
		default: return Only(RoofShape::Parapet);
	}
}

static RoofShape RoofShapeOf(const HouseSite &site)
{
	return SeedPick(RoofShapeSlots(site), ROOF_SHAPE_SLOT.Of(site.seed));
}

static Material FlatRoofMaterial(const HouseSite &site)
{
	if (site.climate == LandscapeType::Tropic) return Material::RoofDeck;
	return site.era == Era::Modern ? Material::Membrane : Material::Gravel;
}

static MaterialSlots PitchedRoofSlots(const HouseSite &site)
{
	switch (site.climate) {
		case LandscapeType::Arctic: return ARCTIC_ROOFS;
		case LandscapeType::Tropic:
		case LandscapeType::Toyland: return Only(Material::ClayTile);
		default: return site.kind == HouseKind::Cottage && site.era == Era::Traditional ? THATCHED_ROOFS : TILED_ROOFS;
	}
}

static Material RoofMaterialOf(const HouseSite &site, RoofShape shape)
{
	switch (shape) {
		case RoofShape::Flat:
		case RoofShape::Parapet: return FlatRoofMaterial(site);
		case RoofShape::Sawtooth: return Material::Corrugated;
		default: return site.kind == HouseKind::Warehouse ? Material::Corrugated : SeedPick(PitchedRoofSlots(site), ROOF_MATERIAL_SLOT.Of(site.seed));
	}
}

static Axis FrontParallel(const HouseSite &site)
{
	if (site.on_road) return AlongEdge(site.front);
	return RIDGE_FALLBACK.Of(site.seed) != 0 ? AXIS_Y : AXIS_X;
}

static Axis RidgeOf(const Plot &plot, RoofShape shape, const HouseSite &site)
{
	float excess = plot.Span(AXIS_X) - plot.Span(AXIS_Y);
	if (shape == RoofShape::Gable && std::abs(excess) <= SQUARE_TOLERANCE) return FrontParallel(site);
	return LongAxis(plot);
}

static float RiseOf(const Plot &plot, Axis ridge, RoofShape shape, float pitch)
{
	switch (shape) {
		case RoofShape::Gable:
		case RoofShape::Hip:
		case RoofShape::Pyramid: return pitch * plot.Span(OtherAxis(ridge)) / 2;
		case RoofShape::Sawtooth: return SAWTOOTH_RISE;
		default: return 0.0f;
	}
}

static Solid RoofedAlong(const Part &mass, const HouseSite &site, RoofShape shape, float pitch, Axis ridge)
{
	HouseCover cover = RoofCover(site, RoofMaterialOf(site, shape));
	return mass.Roof(shape, RiseOf(FootprintOf(mass), ridge, shape, pitch)).Ridge(ridge).Covered(cover.material, cover.tint);
}

static Solid Roofed(const Part &mass, const HouseSite &site, RoofShape shape, float pitch = 0.0f)
{
	return RoofedAlong(mass, site, shape, pitch, RidgeOf(FootprintOf(mass), shape, site));
}

static Solid RaiseBlock(BuildingForm &form, const Plot &plot, const HouseSite &site, WindowGrid grid, int storeys)
{
	Solid block = Roofed(Storeyed(plot, site, grid, storeys), site, RoofShapeOf(site), ClimatePitch(site));
	Place(form, block);
	return block;
}

static Solid RaiseBlock(BuildingForm &form, const Plot &plot, const HouseSite &site)
{
	return RaiseBlock(form, plot, site, site.grid, site.storeys);
}

static float DepthInside(const Plot &plot, Axis axis, float at)
{
	return plot.Span(axis) / 2 - std::abs(at - plot.Mid(axis));
}

static float RoofLevelAt(const Solid &roofed, float x, float y)
{
	Plot roof = FootprintOf(roofed);
	const std::array<float, AXIS_END> at = {x, y};
	Axis across = OtherAxis(roofed.ridge);
	float depth = DepthInside(roof, across, at[across]);
	if (roofed.roof != RoofShape::Gable) depth = std::min(depth, DepthInside(roof, roofed.ridge, at[roofed.ridge]));
	return roofed.base + roofed.wall + roofed.rise * depth / (roof.Span(across) / 2);
}

static float LowestRoofUnder(const Solid &roofed, const Plot &plot)
{
	return std::min({
		RoofLevelAt(roofed, plot.x0, plot.y0), RoofLevelAt(roofed, plot.x1, plot.y0),
		RoofLevelAt(roofed, plot.x0, plot.y1), RoofLevelAt(roofed, plot.x1, plot.y1),
	});
}

static Plot SquareOnRidge(const Solid &roofed, float offset, float side)
{
	Plot roof = FootprintOf(roofed);
	float along = roof.Mid(roofed.ridge) - roof.Span(roofed.ridge) / 2 + offset;
	float across = roof.Mid(OtherAxis(roofed.ridge));
	if (roofed.ridge == AXIS_X) return Plot::Around(along, across, side / 2, side / 2);
	return Plot::Around(across, along, side / 2, side / 2);
}

static void AddChimney(BuildingForm &form, const Solid &roofed, float offset, const HouseSite &site)
{
	Plot stack = SquareOnRidge(roofed, offset, CHIMNEY_SIDE);
	float base = LowestRoofUnder(roofed, stack);
	float top = roofed.Top() + CHIMNEY_ABOVE_RIDGE;
	Place(form, Part::Box(stack).On(base).Detailed().Height(top - base).Clad(Material::Brick, SwatchTint(Swatch::Brick, WALL_COLOUR.Of(site.seed))));
}

static float ChimneyOffset(const Solid &roofed, uint32_t seed)
{
	return END_CHOICE.Of(seed) != 0 ? FootprintOf(roofed).Span(roofed.ridge) - CHIMNEY_END_OFFSET : CHIMNEY_END_OFFSET;
}

static void AddWaterTank(BuildingForm &form, const Solid &roofed)
{
	Plot roof = FootprintOf(roofed);
	Place(form, Part::Square(roof.Mid(AXIS_X), roof.Mid(AXIS_Y), TANK_SIDE).On(LevelAbove(roofed)).Detailed().Height(TANK_HEIGHT).Clad(Material::Metal, TANK_TINT));
}

static bool IsDwelling(HouseKind kind)
{
	return kind == HouseKind::Cottage || kind == HouseKind::House || kind == HouseKind::Flats;
}

static void DressRoof(BuildingForm &form, const Solid &roofed, const HouseSite &site)
{
	switch (roofed.roof) {
		case RoofShape::Parapet:
			if (site.kind == HouseKind::Flats && site.climate == LandscapeType::Tropic) {
				AddWaterTank(form, roofed);
			} else {
				AddRooftopKit(form, roofed, site.seed);
			}
			break;

		case RoofShape::Gable:
		case RoofShape::Hip:
			if (IsDwelling(site.kind) && CHIMNEY.Of(site.seed) != 0) AddChimney(form, roofed, ChimneyOffset(roofed, site.seed), site);
			break;

		default:
			break;
	}
}

static void AddAwning(BuildingForm &form, const Solid &mass, const HouseSite &site, float length)
{
	Plot awning = FootprintOf(mass).Outside(site.front, AWNING_DEPTH).Narrowed(AlongEdge(site.front), length);
	float ground = GroundStoreyTiles(mass.windows);
	Place(form, Part::Box(awning).On(ground - AWNING_THICKNESS).Detailed().Height(AWNING_THICKNESS).Clad(Material::Plain, AccentTint(site)));
}

static Solid Annex(const Solid &dwelling, const Plot &plot, DiagDirection side, const HouseSite &site)
{
	Plot body = FootprintOf(dwelling);
	Plot area = plot.Edge(side, ANNEX_WIDTH);
	area.x0 = body.x0 + ANNEX_REAR_SHARE * body.Span(AXIS_X);
	area.x1 = body.x1 - ANNEX_END_GAP;
	RoofShape roof = site.climate == LandscapeType::Tropic ? RoofShape::Flat : RoofShape::Gable;
	return RoofedAlong(Walled(area, site, WindowGrid::None, dwelling.wall * ANNEX_WALL_SHARE), site, roof, ClimatePitch(site), AXIS_X);
}

static void BuildDwelling(BuildingForm &form, const HouseSite &site)
{
	const Setback &setback = site.kind == HouseKind::Cottage ? COTTAGE_SETBACK : HOUSE_SETBACK;
	Plot plot = FootprintOf(form).SetBack(site.front, Jittered(setback, site.seed, DWELLING_JITTER));
	std::optional<DiagDirection> annex_side = SeedPick(ANNEX_SIDES, PLAN_VARIANT.Of(site.seed));
	Solid dwelling = RaiseBlock(form, annex_side.has_value() ? plot.Inset(*annex_side, ANNEX_WIDTH) : plot, site);
	if (annex_side.has_value()) Place(form, Annex(dwelling, plot, *annex_side, site));
	DressRoof(form, dwelling, site);
}

static void BuildTerrace(BuildingForm &form, const HouseSite &site)
{
	Solid row = RaiseBlock(form, FootprintOf(form).SetBack(site.front, TERRACE_SETBACK), site);
	if (row.roof == RoofShape::Gable) {
		AddChimney(form, row, CHIMNEY_SIDE / 2, site);
		AddChimney(form, row, FootprintOf(row).Span(row.ridge) - CHIMNEY_SIDE / 2, site);
	}
	DressRoof(form, row, site);
}

static void BuildFlats(BuildingForm &form, const HouseSite &site)
{
	Solid block = RaiseBlock(form, FootprintOf(form).SetBack(site.front, Jittered(Evenly(FLATS_SETBACK), site.seed, FLATS_JITTER)), site);
	DressRoof(form, block, site);
}

static void BuildShops(BuildingForm &form, const HouseSite &site)
{
	Solid shop = RaiseBlock(form, FootprintOf(form).SetBack(site.front, SHOPS_SETBACK), site);
	AddAwning(form, shop, site, FootprintOf(shop).Span(AlongEdge(site.front)));
	DressRoof(form, shop, site);
}

static void BuildOffice(BuildingForm &form, const HouseSite &site)
{
	Solid office = RaiseBlock(form, FootprintOf(form).SetBack(site.front, Evenly(DEFAULT_SETBACK)), site);
	Plot roof = FootprintOf(office);
	DiagDirection end = AxisToDiagDir(LongAxis(roof));
	Plot penthouse = roof.Inset(ROOFTOP_MARGIN).Edge(end, PENTHOUSE_LENGTH).Narrowed(AlongEdge(end), PENTHOUSE_WIDTH);
	uint32_t concrete = SwatchTint(Swatch::Concrete, WALL_COLOUR.Of(site.seed));
	Place(form, Part::Box(penthouse).On(LevelAbove(office)).Detailed().Height(PENTHOUSE_HEIGHT).Clad(Material::Concrete, concrete));
	DressRoof(form, Within(office, roof.Inset(end, PENTHOUSE_LENGTH + ROOFTOP_GAP)), site);
}

static Solid AddPodium(BuildingForm &form, const HouseSite &site, int storeys)
{
	Plot plot = FootprintOf(form).SetBack(site.front, Evenly(PODIUM_SETBACK));
	Solid podium = Roofed(Storeyed(plot, site, WindowGrid::Shopfront, storeys), site, RoofShape::Flat);
	Place(form, podium);
	return podium;
}

static Solid AddSlab(BuildingForm &form, const Plot &plot, const Solid &podium, const HouseSite &site)
{
	float base = podium.Top();
	float top = WallTiles(WearableGrid(site.wall.material, site.grid), site.storeys);
	Solid slab = Roofed(Storeyed(plot, site, site.grid, site.storeys).On(base).Height(top - base), site, RoofShape::Parapet);
	Place(form, slab);
	return slab;
}

static void BuildTower(BuildingForm &form, const HouseSite &site)
{
	Solid podium = AddPodium(form, site, std::min(TOWER_PODIUM_STOREYS, site.storeys));
	Solid shaft = AddSlab(form, FootprintOf(podium).Inset(SHAFT_INSET), podium, site);
	DressRoof(form, shaft, site);
}

static void BuildGlassTower(BuildingForm &form, const HouseSite &site)
{
	Solid tower = RaiseBlock(form, FootprintOf(form).SetBack(site.front, Evenly(GLASS_TOWER_SETBACK)), site);
	Plot crown = FootprintOf(tower).Inset(CROWN_INSET);
	Place(form, Part::Box(crown).On(LevelAbove(tower)).Detailed().Height(CROWN_HEIGHT).Clad(Material::Glass, ScaledRgb(site.wall.tint, CROWN_SHADE)));
}

static void BuildHotel(BuildingForm &form, const HouseSite &site)
{
	Solid podium = AddPodium(form, site, HOTEL_PODIUM_STOREYS);
	Plot base = FootprintOf(podium);
	Axis long_axis = LongAxis(base);
	Plot plot = base.Inset(AxisToDiagDirs(long_axis), SLAB_LONG_INSET).Inset(AxisToDiagDirs(OtherAxis(long_axis)), SLAB_SHORT_INSET);
	Solid slab = AddSlab(form, plot, podium, site);
	Plot sign = plot.Edge(site.front, HOTEL_SIGN_DEPTH).Narrowed(AlongEdge(site.front), HOTEL_SIGN_LENGTH);
	Place(form, Part::Box(sign).On(LevelAbove(slab)).Detailed().Height(HOTEL_SIGN_HEIGHT).Clad(Material::Plain, AccentTint(site)));
	DressRoof(form, slab, site);
}

static HouseCover SpireCover(const HouseSite &site)
{
	if (site.climate == LandscapeType::Toyland) return RoofCover(site, Material::ClayTile);
	return SPIRE_MATERIAL.Of(site.seed) != 0 ? CopperCover(site) : RoofCover(site, Material::Slate);
}

static Solid Belfry(const Plot &plot, const HouseSite &site)
{
	float rise = SPIRE_RISE[to_underlying(site.climate)];
	if (site.climate == LandscapeType::Tropic) {
		return Part::Cylinder(plot).Height(BELFRY_HEIGHT).Clad(Material::Render, WHITEWASH).Roof(RoofShape::Dome, rise);
	}
	HouseCover spire = SpireCover(site);
	return Part::Box(plot).Height(BELFRY_HEIGHT).Clad(Material::Stone, site.wall.tint).Roof(RoofShape::Pyramid, rise).Covered(spire.material, spire.tint);
}

static void BuildChurch(BuildingForm &form, const HouseSite &site)
{
	Axis axis = FrontParallel(site);
	Axis across = OtherAxis(axis);
	DiagDirection belfry_end = END_CHOICE.Of(site.seed) != 0 ? AxisToDiagDir(axis) : ReverseDiagDir(AxisToDiagDir(axis));
	Plot row = FootprintOf(form).Narrowed(axis, NAVE_LENGTH + BELFRY_SIDE);
	Part nave = Storeyed(row.Inset(belfry_end, BELFRY_SIDE).Narrowed(across, NAVE_WIDTH), site, WindowGrid::Arched, NAVE_STOREYS);
	Place(form, RoofedAlong(nave, site, RoofShape::Gable, NAVE_PITCH, axis));
	Place(form, Belfry(row.Edge(belfry_end, BELFRY_SIDE).Narrowed(across, BELFRY_SIDE), site));
}

static void BuildTheatre(BuildingForm &form, const HouseSite &site)
{
	Plot plot = FootprintOf(form).SetBack(site.front, Evenly(DEFAULT_SETBACK));
	float lobby_depth = LOBBY_SHARE * plot.Span(DiagDirToAxis(site.front));
	Solid hall = Roofed(Walled(plot.Inset(site.front, lobby_depth), site, WindowGrid::None, AUDITORIUM_HEIGHT), site, RoofShape::Parapet);
	HouseCover copper = CopperCover(site);
	Place(form, Roofed(Storeyed(plot.Edge(site.front, lobby_depth), site, WindowGrid::Arched, LOBBY_STOREYS), site, RoofShape::Parapet));
	Place(form, hall);
	Place(form, Part::Cylinder(FootprintOf(hall).Inset(THEATRE_DOME_INSET)).On(LevelAbove(hall)).Clad(site.wall.material, site.wall.tint)
		.Roof(RoofShape::Dome, THEATRE_DOME_RISE).Covered(copper.material, copper.tint));
}

static void BuildCinema(BuildingForm &form, const HouseSite &site)
{
	Solid hall = RaiseBlock(form, FootprintOf(form).SetBack(site.front, Evenly(CINEMA_SETBACK)), site, WindowGrid::Shopfront, CINEMA_STOREYS);
	Plot blade = FootprintOf(hall).Outside(site.front, BLADE_DEPTH).Narrowed(AlongEdge(site.front), BLADE_LENGTH);
	Place(form, Part::Box(blade).On(GroundStoreyTiles(hall.windows)).Detailed().Height(BLADE_HEIGHT).Clad(Material::Plain, AccentTint(site)));
	DressRoof(form, hall, site);
}

static void BuildMall(BuildingForm &form, const HouseSite &site)
{
	Solid mall = RaiseBlock(form, FootprintOf(form).SetBack(site.front, Evenly(MALL_SETBACK)), site, WindowGrid::Shopfront, MALL_STOREYS);
	Plot roof = FootprintOf(mall);
	Axis along = LongAxis(roof);
	Axis across = OtherAxis(along);
	for (uint row = 0; row < SKYLIGHT_ROWS; row++) {
		Plot skylight = Cell(roof, across, row, SKYLIGHT_ROWS).Narrowed(across, SKYLIGHT_WIDTH).Inset(AxisToDiagDirs(along), ROOFTOP_MARGIN);
		Place(form, Part::Decal(skylight).On(LevelAbove(mall)).Covered(Material::GlassRoof, site.glass));
	}
	AddRooftopKit(form, Within(mall, roof.Narrowed(across, roof.Span(across) / SKYLIGHT_ROWS - SKYLIGHT_WIDTH)), site.seed);
	AddAwning(form, mall, site, MALL_CANOPY_LENGTH);
}

static void BuildWarehouse(BuildingForm &form, const HouseSite &site)
{
	Part shed = Walled(FootprintOf(form).SetBack(site.front, Evenly(WAREHOUSE_SETBACK)), site, WindowGrid::Doors, WAREHOUSE_HEIGHT);
	Place(form, Roofed(shed, site, RoofShapeOf(site), INDUSTRIAL_PITCH));
}

static Solid Stand(const Plot &plot, DiagDirection outward, uint32_t tint)
{
	return Part::Box(plot).Height(STAND_HEIGHT).Clad(Material::Concrete, tint).Roof(RoofShape::Shed, STAND_RISE).HighSide(outward);
}

static void AddMasts(BuildingForm &form, const Solid &stand, DiagDirection outward)
{
	for (DiagDirection end : {DIAGDIR_NW, DIAGDIR_SE}) {
		Plot corner = FootprintOf(stand).Edge(outward, MAST_SIDE).Edge(end, MAST_SIDE);
		Place(form, Part::Box(corner).On(stand.Top()).Detailed().Height(MAST_TOP - stand.Top()).Clad(Material::Metal, COL_STEEL));
	}
}

static void BuildStadium(BuildingForm &form, const HouseSite &site)
{
	Plot ground = FootprintOf(form);
	Place(form, Part::Decal(ground.Scaled(FIELD_SHARE)).Covered(Material::Plain, FIELD_TINT));

	Plot between = ground;
	for (DiagDirection end : {DIAGDIR_NE, DIAGDIR_SW}) {
		float depth = END_STAND_DEPTH * ground.Span(AXIS_X);
		Solid stand = Stand(ground.Edge(end, depth), end, site.wall.tint);
		Place(form, stand);
		AddMasts(form, stand, end);
		between = between.Inset(end, depth);
	}
	for (DiagDirection side : {DIAGDIR_NW, DIAGDIR_SE}) {
		Place(form, Stand(between.Edge(side, SIDE_STAND_DEPTH * ground.Span(AXIS_Y)), side, site.wall.tint));
	}
}

static void BuildPark(BuildingForm &form, const HouseSite &)
{
	Plot ground = FootprintOf(form);
	for (Axis axis : {AXIS_X, AXIS_Y}) {
		Place(form, Part::Decal(ground.Narrowed(axis, PATH_WIDTH)).Covered(Material::Gravel, PATH_TINT));
	}
}

static void BuildStatue(BuildingForm &form, const HouseSite &)
{
	Part plinth = Part::Square(LOT_CENTRE, LOT_CENTRE, PLINTH_SIDE).Height(PLINTH_HEIGHT).Clad(Material::Stone, PLINTH_TINT);
	Place(form, plinth);
	Place(form, Part::Square(LOT_CENTRE, LOT_CENTRE, FIGURE_SIDE).On(LevelAbove(plinth)).Height(FIGURE_HEIGHT).Clad(Material::Metal, FIGURE_TINT));
}

static void BuildFountain(BuildingForm &form, const HouseSite &site)
{
	AddFountain(form, SwatchTint(Swatch::Stone, site.seed));
}

static void BuildIgloo(BuildingForm &form, const HouseSite &site)
{
	Part dome = Part::Disc(LOT_CENTRE, LOT_CENTRE, IGLOO_RADIUS).Clad(Material::Stone, IGLOO_TINT).Roof(RoofShape::Dome, IGLOO_RISE);
	Plot door = FootprintOf(dome).Outside(site.front, IGLOO_DOOR_DEPTH).Narrowed(AlongEdge(site.front), IGLOO_DOOR_WIDTH);
	Place(form, dome);
	Place(form, Part::Box(door).Height(IGLOO_DOOR_HEIGHT).Clad(Material::Stone, IGLOO_TINT));
}

static void BuildTepee(BuildingForm &form, const HouseSite &)
{
	for (const TepeeCone &cone : TEPEE_CONES) {
		Place(form, Part::Disc(cone.x, cone.y, TEPEE_RADIUS).Clad(Material::Render, TEPEE_TINT).Roof(RoofShape::Cone, cone.rise));
	}
}

static void BuildTeapot(BuildingForm &form, const HouseSite &site)
{
	HouseCover glaze = RoofCover(site, Material::Plain);
	Part pot = Part::Disc(LOT_CENTRE, LOT_CENTRE, TEAPOT_RADIUS).Height(TEAPOT_WALL).Clad(Material::Plain, site.wall.tint)
		.Roof(RoofShape::Dome, TEAPOT_RISE).Covered(glaze.material, glaze.tint);
	Plot spout = FootprintOf(pot).Outside(DIAGDIR_SE, SPOUT_LENGTH).Narrowed(AXIS_X, SPOUT_WIDTH);
	Place(form, pot);
	Place(form, Part::Box(spout).On(SPOUT_BASE).Height(SPOUT_HEIGHT).Clad(Material::Plain, site.wall.tint));
	Place(form, Part::Disc(LOT_CENTRE, LOT_CENTRE, KNOB_RADIUS).On(LevelAbove(pot)).Detailed().Height(KNOB_HEIGHT).Clad(Material::Plain, glaze.tint));
}

static void BuildPiggyBank(BuildingForm &form, const HouseSite &)
{
	Plot belly = Plot::Around(LOT_CENTRE, LOT_CENTRE, PIGGY_HALF_WIDTH, PIGGY_HALF_LENGTH);
	Part body = Part::Cylinder(belly).Height(PIGGY_WALL).Clad(Material::Plain, PIGGY_TINT).Roof(RoofShape::Dome, PIGGY_RISE);
	Plot snout = belly.Outside(DIAGDIR_SE, SNOUT_DEPTH).Narrowed(AXIS_X, SNOUT_WIDTH);
	Place(form, body);
	Place(form, Part::Box(snout).Height(SNOUT_HEIGHT).Clad(Material::Plain, PIGGY_TINT));
}

static bool IsBody(const Solid &solid)
{
	return solid.role == SolidRole::Body && solid.kind != SolidKind::Decal;
}

static std::optional<Plot> BodyBounds(const BuildingForm &form)
{
	std::optional<Plot> bounds;
	for (const Solid &solid : form.Solids()) {
		if (!IsBody(solid)) continue;
		bounds = bounds.has_value() ? Union(*bounds, FootprintOf(solid)) : FootprintOf(solid);
	}
	return bounds;
}

static float HighestEave(const BuildingForm &form)
{
	float highest = 0.0f;
	for (const Solid &solid : form.Solids()) {
		if (IsBody(solid)) highest = std::max(highest, solid.base + solid.wall);
	}
	return highest;
}

static void LaySite(BuildingForm &form, const Plot &bounds)
{
	Place(form, Part::Decal(bounds.Inset(-PAD_MARGIN)).Covered(Material::Gravel, PAD_TINT));
	Place(form, Part::Box(bounds).Height(SLAB_HEIGHT).Clad(Material::Concrete, SLAB_TINT));
	Plot yard = bounds.Inset(PILE_MARGIN);
	for (const auto &[side, end] : PILE_CORNERS) {
		Plot pile = yard.Edge(side, PILE_SIDE).Edge(end, PILE_SIDE);
		Place(form, Part::Box(pile).On(SLAB_HEIGHT).Detailed().Height(PILE_SIDE).Clad(Material::Planks, FRAME_TIMBER));
	}
}

static float FrameShare(TileIndex north)
{
	return FRAME_START_SHARE + (1.0f - FRAME_START_SHARE) * (GetHouseConstructionTick(north) + 1) / CONSTRUCTION_TICKS;
}

static uint32_t FrameTint(HouseKind kind)
{
	switch (kind) {
		case HouseKind::Office:
		case HouseKind::Tower:
		case HouseKind::GlassTower:
		case HouseKind::Hotel:
		case HouseKind::Mall: return FRAME_STEEL;
		default: return FRAME_TIMBER;
	}
}

static Solid Framed(Solid solid, float share, uint32_t tint)
{
	solid.base *= share;
	solid.wall *= share;
	solid.rise = 0.0f;
	solid.roof = RoofShape::Flat;
	solid.wall_material = Material::Lattice;
	solid.windows = WindowGrid::None;
	solid.wall_tint = tint;
	solid.roof_material = Material::Plain;
	solid.roof_tint = SLAB_TINT;
	return solid;
}

static Solid Sheathed(Solid solid)
{
	solid.windows = WindowGrid::None;
	solid.wall_tint = Mix(solid.wall_tint, RAW_PLASTER, RAW_PLASTER_SHARE);
	solid.roof_material = Material::Plain;
	solid.roof_tint = UNDERLAY_TINT;
	return solid;
}

static void AddScaffold(BuildingForm &form, DiagDirection front)
{
	std::optional<Plot> bounds = BodyBounds(form);
	if (!bounds.has_value()) return;

	Plot scaffold = bounds->Outside(front, SCAFFOLD_FAR).Edge(front, SCAFFOLD_FAR - SCAFFOLD_NEAR);
	if (!Contains(FootprintOf(form), scaffold)) return;
	Place(form, Part::Box(scaffold).Height(HighestEave(form) + SCAFFOLD_ABOVE).Clad(Material::Lattice, SCAFFOLD_TINT));
}

static void RaiseShell(BuildingForm &form, const BuildingForm &finished, const HouseSite &site, uint8_t stage)
{
	float share = FrameShare(site.north);
	for (const Solid &solid : finished.Solids()) {
		if (IsBody(solid)) Place(form, stage == FRAME_STAGE ? Framed(solid, share, FrameTint(site.kind)) : Sheathed(solid));
	}
	AddScaffold(form, site.front);
}

static void ShowConstruction(BuildingForm &form, const HouseSite &site)
{
	uint8_t stage = GetHouseBuildingStage(site.north);
	if (stage == TOWN_HOUSE_COMPLETED) return;

	BuildingForm finished = form;
	form.count = 0;
	if (stage == SITE_STAGE) {
		LaySite(form, BodyBounds(finished).value_or(FootprintOf(form).Inset(DEFAULT_SETBACK)));
	} else {
		RaiseShell(form, finished, site, stage);
	}
}

static constexpr std::array<HouseBuilder, to_underlying(HouseKind::End)> HOUSE_BUILDERS = {
	BuildDwelling, BuildDwelling, BuildTerrace, BuildFlats, BuildShops, BuildOffice, BuildTower, BuildGlassTower, BuildHotel,
	BuildChurch, BuildTheatre, BuildCinema, BuildMall, BuildWarehouse, BuildStadium, BuildPark, BuildStatue, BuildFountain,
	BuildIgloo, BuildTepee, BuildTeapot, BuildPiggyBank,
};

std::optional<BuildingForm> HouseForm(TileIndex tile)
{
	HouseSite site = ReadHouseSite(tile);
	BuildingForm form = SiteForm(site.north, SiteFloor(TileArea(site.north, site.size_x, site.size_y)), site.size_x, site.size_y);
	HOUSE_BUILDERS[to_underlying(site.kind)](form, site);
	ShowConstruction(form, site);
	if (form.count == 0) return std::nullopt;
	return form;
}

std::optional<FloraPatch> HouseFlora(TileIndex tile)
{
	if (IdentifyHouse(tile).kind != HouseKind::Park) return std::nullopt;
	bool toyland = _settings_game.game_creation.landscape == LandscapeType::Toyland;
	return FloraPatch{toyland ? TreeKind::Toy : TreeKind::Broadleaf, TreeAge::Grown, PARK_TREES};
}
