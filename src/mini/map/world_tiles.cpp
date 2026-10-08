/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file world_tiles.cpp Every tile of the map as the texels the ground shader reads. */

#include "../../stdafx.h"
#include "world_tiles.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "../../bridge_map.h"
#include "../../clear_map.h"
#include "../../core/math_func.hpp"
#include "../../elrail_func.h"
#include "../../landscape.h"
#include "../../map_func.h"
#include "../../rail_map.h"
#include "../../road_map.h"
#include "../../settings_type.h"
#include "../../station_map.h"
#include "../../tile_map.h"
#include "../../track_func.h"
#include "../../tree_map.h"
#include "../../tunnel_map.h"
#include "../../tunnelbridge_map.h"
#include "../../water_map.h"
#include "../gpu/frame_profile.h"
#include "airfield_marks.h"
#include "building_form.h"
#include "house_forms.h"
#include "industry_forms.h"
#include "tile_shapes.h"

#include "../../safeguards.h"

static constexpr uint BLOCK_TILES = 32;
static constexpr uint SWEEP_TILES_PER_SYNC = 512;
static constexpr uint TEMPERATE_CONIFER_EVERY = 3;
static constexpr uint ARCTIC_BROADLEAF_EVERY = 4;
static constexpr uint8_t NO_STYLE = 0;
static constexpr bool STRAIGHT_THROUGH_ENTRANCE = true;

static_assert(sizeof(SurfaceTexel) == 4);
static_assert(sizeof(GroundTexel) == 4);
static_assert(sizeof(WaterTexel) == 4);
static_assert(sizeof(NetworkTexel) == 4);
static_assert(to_underlying(RailLook::End) <= NETWORK_RAIL_LOOK_MASK + 1);

/* The shader's piece tables index the game's track and road bits as they are numbered here. */
static_assert(TRACK_BIT_X == 1 && TRACK_BIT_Y == 2 && TRACK_BIT_UPPER == 4 && TRACK_BIT_LOWER == 8 && TRACK_BIT_LEFT == 16 && TRACK_BIT_RIGHT == 32);
static_assert(ROAD_NW == 1 && ROAD_SW == 2 && ROAD_SE == 4 && ROAD_NE == 8);

WorldTiles _world_tiles;

/* The game folds a tile along the diagonal whose corners stand level, and along the lower pair when both do. */
bool FoldsWestToEast(const SurfaceTexel &surface)
{
	return surface.west == surface.east && (surface.north != surface.south || surface.north > surface.west);
}

double FacetLevel(const SurfaceTexel &surface, double fx, double fy)
{
	auto [north, west, east, south] = surface;
	if (FoldsWestToEast(surface)) {
		if (fx + fy <= 1.0) return north + (west - north) * fx + (east - north) * fy;
		return south + (west - south) * (1.0 - fy) + (east - south) * (1.0 - fx);
	}
	if (fx >= fy) return north + (west - north) * fx + (south - west) * fy;
	return north + (east - north) * fy + (south - east) * fx;
}

static uint HighestLevel(const SurfaceTexel &surface)
{
	return std::max({surface.north, surface.west, surface.east, surface.south});
}

/* Every corner of a halftile foundation's raised half stands at its top, so the step down to the other half becomes a slope. */
static uint8_t CornerLevel(Slope slope, int z, Corner corner)
{
	bool raised_half = IsHalftileSlope(slope) && corner != OppositeCorner(GetHalftileSlopeCorner(slope));
	return static_cast<uint8_t>(z + (raised_half ? GetSlopeMaxZ(slope) : GetSlopeZInCorner(RemoveHalftileSlope(slope), corner)));
}

static SurfaceTexel SurfaceOn(Slope slope, int z)
{
	return {CornerLevel(slope, z, CORNER_N), CornerLevel(slope, z, CORNER_W), CornerLevel(slope, z, CORNER_E), CornerLevel(slope, z, CORNER_S)};
}

/* A tunnel entrance is cut level into the hill at the floor its vehicles drive on. */
static SurfaceTexel PackSurface(TileIndex tile)
{
	if (IsTunnelTile(tile)) return SurfaceOn(SLOPE_FLAT, GetTileZ(tile));
	auto [slope, z] = GetFoundationSlope(tile);
	return SurfaceOn(slope, z);
}

struct Cover {
	GroundMaterial material;
	uint density = GROUND_DENSITY_MASK;
	uint variant = 0;
};

static LandscapeType Landscape()
{
	return _settings_game.game_creation.landscape;
}

static GroundMaterial SnowOrDesert()
{
	switch (Landscape()) {
		case LandscapeType::Arctic: return GroundMaterial::Snow;
		case LandscapeType::Tropic: return GroundMaterial::Desert;
		default: return GroundMaterial::Grass;
	}
}

static Cover ClearCover(TileIndex tile)
{
	if (IsSnowTile(tile)) return {GroundMaterial::Snow, GetClearDensity(tile)};
	switch (GetClearGround(tile)) {
		case CLEAR_ROUGH: return {GroundMaterial::Rough};
		case CLEAR_ROCKS: return {GroundMaterial::Rocks};
		case CLEAR_FIELDS: return {GroundMaterial::Fields, GROUND_DENSITY_MASK, GetFieldType(tile)};
		case CLEAR_DESERT: return {GroundMaterial::Desert, GetClearDensity(tile)};
		default: return {GroundMaterial::Grass, GetClearDensity(tile)};
	}
}

static Cover TreeCover(TileIndex tile)
{
	uint density = GetTreeDensity(tile);
	switch (GetTreeGround(tile)) {
		case TREE_GROUND_ROUGH: return {GroundMaterial::Rough};
		case TREE_GROUND_SNOW_DESERT: return {SnowOrDesert(), density};
		case TREE_GROUND_ROUGH_SNOW: return {GroundMaterial::Snow, density};
		case TREE_GROUND_SHORE: return {GroundMaterial::Shore};
		default: return {GroundMaterial::Grass, density};
	}
}

static Cover RailCover(TileIndex tile)
{
	switch (GetRailGroundType(tile)) {
		case RailGroundType::Barren: return {GroundMaterial::Dirt};
		case RailGroundType::SnowOrDesert: return {SnowOrDesert()};
		case RailGroundType::HalfTileSnow: return {GroundMaterial::Snow, 1};
		case RailGroundType::HalfTileWater: return {GroundMaterial::Shore};
		default: return {GroundMaterial::Grass};
	}
}

static bool IsPaved(Roadside roadside)
{
	switch (roadside) {
		case Roadside::Barren:
		case Roadside::Grass:
		case Roadside::GrassRoadWorks: return false;
		default: return true;
	}
}

static Cover RoadCover(TileIndex tile)
{
	if (IsRoadDepot(tile)) return {GroundMaterial::Paved};
	if (IsOnSnowOrDesert(tile)) return {SnowOrDesert()};
	Roadside roadside = GetRoadside(tile);
	if (IsPaved(roadside)) return {GroundMaterial::Paved};
	return {roadside == Roadside::Barren ? GroundMaterial::Dirt : GroundMaterial::Grass};
}

/* Whatever stands on water keeps the shore bed under it; the water texels draw the water itself. */
static Cover BuiltCover(TileIndex tile, GroundMaterial on_land)
{
	return {HasTileWaterClass(tile) && IsTileOnWater(tile) ? GroundMaterial::Shore : on_land};
}

/* An airport's grass stays grass; the rest is paved and carries the markings its tile is painted with. */
static Cover AirportCover(TileIndex tile)
{
	AirfieldMark mark = AirfieldMarkOf(tile);
	if (mark == AirfieldMark::Grass) return {GroundMaterial::Grass};
	return {GroundMaterial::Paved, GROUND_DENSITY_MASK, to_underlying(mark)};
}

static GroundMaterial Planting(TileIndex tile)
{
	return Landscape() == LandscapeType::Tropic && GetTropicZone(tile) == TROPICZONE_DESERT ? GroundMaterial::Desert : GroundMaterial::Grass;
}

static Cover CoverOf(TileIndex tile, bool planted)
{
	switch (GetTileType(tile)) {
		case MP_CLEAR: return ClearCover(tile);
		case MP_TREES: return TreeCover(tile);
		case MP_RAILWAY: return RailCover(tile);
		case MP_ROAD: return RoadCover(tile);
		case MP_WATER: return {GroundMaterial::Shore};
		case MP_HOUSE: return {planted ? Planting(tile) : GroundMaterial::Paved};
		case MP_STATION: return IsAirport(tile) ? AirportCover(tile) : BuiltCover(tile, GroundMaterial::Paved);
		case MP_INDUSTRY: return planted ? Cover{Planting(tile)} : BuiltCover(tile, GroundMaterial::Dirt);
		case MP_OBJECT: return BuiltCover(tile, GroundMaterial::Grass);
		case MP_TUNNELBRIDGE: return {HasTunnelBridgeSnowOrDesert(tile) ? SnowOrDesert() : GroundMaterial::Grass};
		default: return {GroundMaterial::Grass};
	}
}

static bool IsLush(TileIndex tile)
{
	return Landscape() == LandscapeType::Tropic && GetTropicZone(tile) == TROPICZONE_RAINFOREST;
}

/* The game's species only pick the silhouette here; the forests mix conifers into broadleaf and birches into pine. */
static TreeKind KindOf(TreeType type)
{
	if (type >= TREE_TOYLAND) return TreeKind::Toy;
	if (type >= TREE_SUB_TROPICAL) return TreeKind::Palm;
	if (type == TREE_CACTUS) return TreeKind::Cactus;
	if (type >= TREE_RAINFOREST) return TreeKind::Jungle;
	if (type >= TREE_SUB_ARCTIC) return (type - TREE_SUB_ARCTIC) % ARCTIC_BROADLEAF_EVERY == ARCTIC_BROADLEAF_EVERY - 1 ? TreeKind::Broadleaf : TreeKind::Conifer;
	return (type - TREE_TEMPERATE) % TEMPERATE_CONIFER_EVERY == TEMPERATE_CONIFER_EVERY - 1 ? TreeKind::Conifer : TreeKind::Broadleaf;
}

static TreeAge AgeOf(TreeGrowthStage growth)
{
	switch (growth) {
		case TreeGrowthStage::Growing1: return TreeAge::Sapling;
		case TreeGrowthStage::Growing2:
		case TreeGrowthStage::Growing3: return TreeAge::Young;
		case TreeGrowthStage::Grown: return TreeAge::Grown;
		default: return TreeAge::Dying;
	}
}

static std::optional<FloraPatch> FloraOf(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_TREES: return FloraPatch{KindOf(GetTreeType(tile)), AgeOf(GetTreeGrowth(tile)), static_cast<uint8_t>(GetTreeCount(tile))};
		case MP_HOUSE: return HouseFlora(tile);
		case MP_INDUSTRY: return IndustryFlora(tile);
		default: return std::nullopt;
	}
}

static uint8_t PackFlora(const std::optional<FloraPatch> &flora)
{
	if (!flora.has_value()) return 0;
	uint age = to_underlying(flora->age) << FLORA_AGE_SHIFT;
	uint kind = to_underlying(flora->kind) << FLORA_KIND_SHIFT;
	return static_cast<uint8_t>((flora->count & FLORA_COUNT_MASK) | age | kind);
}

/* Buildings, water works and the heads of bridges and tunnels stand on foundations of their own. */
static bool IsBuiltOn(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_HOUSE:
		case MP_INDUSTRY:
		case MP_OBJECT:
		case MP_STATION:
		case MP_WATER:
		case MP_TUNNELBRIDGE: return true;
		default: return false;
	}
}

/* The tile just inside the map from one on its border. */
static TileIndex InnerNeighbour(TileIndex tile)
{
	return TileXY(Clamp<uint>(TileX(tile), 1, Map::MaxX() - 1), Clamp<uint>(TileY(tile), 1, Map::MaxY() - 1));
}

static GroundTexel PackGround(TileIndex tile);

/* The border carries on the ground just inside it, bare of whatever grows or stands there, so the land runs on to the map's edge. */
static GroundTexel BorderGround(TileIndex tile)
{
	TileIndex inner = InnerNeighbour(tile);
	if (IsTileType(inner, MP_VOID)) return {GroundMaterial::Void, 0, 0, 0};
	GroundTexel ground = PackGround(inner);
	return {ground.material, static_cast<uint8_t>(ground.detail & ~GROUND_BUILT_BIT), 0, ground.variant};
}

static GroundTexel PackGround(TileIndex tile)
{
	if (IsTileType(tile, MP_VOID)) return BorderGround(tile);
	std::optional<FloraPatch> flora = FloraOf(tile);
	Cover cover = CoverOf(tile, flora.has_value());
	uint8_t detail = static_cast<uint8_t>((cover.density & GROUND_DENSITY_MASK) | (IsLush(tile) ? GROUND_LUSH_BIT : 0) | (IsBuiltOn(tile) ? GROUND_BUILT_BIT : 0));
	return {cover.material, detail, PackFlora(flora), static_cast<uint8_t>(cover.variant)};
}

static WaterTexel WaterOf(WaterClass water_class, uint8_t level)
{
	switch (water_class) {
		case WaterClass::Sea: return {level, level, 0, 0};
		case WaterClass::Canal: return {level, 0, level, 0};
		default: return {level, 0, 0, level};
	}
}

static WaterTexel PackWater(TileIndex tile);

/* The border carries on the water just inside it, so shores do not bend away from the map's edge. */
static WaterTexel BorderWater(TileIndex tile)
{
	TileIndex inner = InnerNeighbour(tile);
	return IsTileType(inner, MP_VOID) ? WaterTexel{} : PackWater(inner);
}

static WaterTexel PackWater(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_VOID:
			return BorderWater(tile);

		case MP_TREES:
			return GetTreeGround(tile) == TREE_GROUND_SHORE ? WaterOf(WaterClass::Sea, HALF_WATER) : WaterTexel{};

		case MP_RAILWAY:
			return GetRailGroundType(tile) == RailGroundType::HalfTileWater ? WaterOf(WaterClass::Sea, HALF_WATER) : WaterTexel{};

		case MP_WATER:
			if (IsCoast(tile)) return WaterOf(WaterClass::Sea, HALF_WATER);
			[[fallthrough]];
		case MP_STATION:
		case MP_INDUSTRY:
		case MP_OBJECT:
			return IsTileOnWater(tile) ? WaterOf(GetWaterClass(tile), FULL_WATER) : WaterTexel{};

		default:
			return {};
	}
}

Groundwork GroundworkOf(const GroundTexel &ground, const NetworkTexel &network)
{
	if (network.track != 0 || network.road != 0 || network.tram != 0) return Groundwork::Way;
	return (ground.detail & GROUND_BUILT_BIT) != 0 ? Groundwork::Built : Groundwork::Open;
}

RailLook RailLookOf(RailType railtype)
{
	switch (GetRailTypeInfo(railtype)->label) {
		case RAILTYPE_LABEL_MONO: return RailLook::Monorail;
		case RAILTYPE_LABEL_MAGLEV: return RailLook::Maglev;
		default: return RailLook::Rail;
	}
}

static uint8_t BareRailStyle(RailType railtype)
{
	return to_underlying(RailLookOf(railtype));
}

static uint8_t RailStyle(RailType railtype)
{
	uint8_t catenary = HasRailCatenaryDrawn(railtype) ? NETWORK_CATENARY_BIT : NO_STYLE;
	return static_cast<uint8_t>(BareRailStyle(railtype) | catenary);
}

static NetworkTexel RailPieces(TrackBits track, uint8_t style)
{
	return {track, ROAD_NONE, ROAD_NONE, style};
}

/* The game reports no road bits for a road or tram type the tile does not carry. */
static NetworkTexel RoadPieces(TileIndex tile, TrackBits track, uint8_t style)
{
	return {track, GetAnyRoadBits(tile, RTT_ROAD, STRAIGHT_THROUGH_ENTRANCE), GetAnyRoadBits(tile, RTT_TRAM, STRAIGHT_THROUGH_ENTRANCE), style};
}

/* A depot's track runs out of its door, under the depot building. */
static NetworkTexel RailNetwork(TileIndex tile)
{
	if (IsRailDepot(tile)) return RailPieces(TrackToTrackBits(GetRailDepotTrack(tile)), RailStyle(GetRailType(tile)));
	uint8_t signals = HasSignals(tile) ? NETWORK_SIGNALS_BIT : NO_STYLE;
	return RailPieces(GetTrackBits(tile), static_cast<uint8_t>(RailStyle(GetRailType(tile)) | signals));
}

static uint8_t RoadsideStyle(TileIndex tile)
{
	uint8_t kerb = IsPaved(GetRoadside(tile)) ? NETWORK_KERB_BIT : NO_STYLE;
	return static_cast<uint8_t>(kerb | (GetDisallowedRoadDirections(tile) << NETWORK_ONE_WAY_SHIFT));
}

static NetworkTexel RoadNetwork(TileIndex tile)
{
	switch (GetRoadTileType(tile)) {
		case RoadTileType::Normal: return RoadPieces(tile, TRACK_BIT_NONE, RoadsideStyle(tile));
		case RoadTileType::Crossing: return RoadPieces(tile, GetCrossingRailBits(tile), RailStyle(GetRailType(tile)));
		case RoadTileType::Depot: return RoadPieces(tile, TRACK_BIT_NONE, NO_STYLE);
		default: return {};
	}
}

/* A station's platforms would hide a wire drawn on the ground, so the painter hangs it above them instead. */
static NetworkTexel StationNetwork(TileIndex tile)
{
	if (HasStationRail(tile)) return RailPieces(GetRailStationTrackBits(tile), BareRailStyle(GetRailType(tile)));
	if (IsAnyRoadStop(tile)) return RoadPieces(tile, TRACK_BIT_NONE, NO_STYLE);
	return {};
}

static NetworkTexel TunnelNetwork(TileIndex tile)
{
	if (!IsTunnel(tile)) return {};
	if (GetTunnelBridgeTransportType(tile) == TRANSPORT_ROAD) return RoadPieces(tile, TRACK_BIT_NONE, NO_STYLE);
	return RailPieces(AxisToTrackBits(DiagDirToAxis(GetTunnelBridgeDirection(tile))), RailStyle(GetRailType(tile)));
}

static NetworkTexel GroundNetwork(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_RAILWAY: return RailNetwork(tile);
		case MP_ROAD: return RoadNetwork(tile);
		case MP_STATION: return StationNetwork(tile);
		case MP_TUNNELBRIDGE: return TunnelNetwork(tile);
		default: return {};
	}
}

/* Signals, one way roads and bridges are drawn from the map itself; their bits only mark that a tile's look changed with them. */
static NetworkTexel PackNetwork(TileIndex tile)
{
	NetworkTexel texel = GroundNetwork(tile);
	if (IsBridgeAbove(tile) || IsBridgeTile(tile)) texel.style |= NETWORK_BRIDGE_BIT;
	return texel;
}

static RampTexel PackRamp(TileIndex tile)
{
	if (!IsBridgeTile(tile) || GetTunnelBridgeTransportType(tile) == TRANSPORT_WATER) return {};
	return {static_cast<uint8_t>(GetTunnelBridgeDirection(tile)), static_cast<uint8_t>(GetBridgeHeight(tile))};
}

void WorldTiles::Reset()
{
	this->stale = true;
}

/* The game marks a tile whenever its look may change; the texels follow at the next sync. */
void WorldTiles::Touch(TileIndex tile)
{
	if (this->stale || tile.base() >= this->queued.size() || this->queued[tile.base()]) return;
	this->queued[tile.base()] = true;
	this->pending.push_back(tile);
}

void WorldTiles::Sync()
{
	if (!Map::IsInitialized()) return;
	if (this->stale || this->size != Dimension(Map::SizeX(), Map::SizeY())) {
		this->Rebuild();
		return;
	}

	_frame_profile.Count("touched_tiles", this->pending.size());
	for (TileIndex tile : this->pending) {
		this->queued[tile.base()] = false;
		this->Repack(tile);
	}
	std::swap(this->touched, this->pending);
	this->pending.clear();
	this->Sweep();
}

WorldChanges WorldTiles::TakeChanges()
{
	for (std::vector<uint32_t> &claims : this->claims) std::ranges::fill(claims, 0);
	return std::exchange(this->changes, {});
}

/* A tile may be asked for before the first sync has packed a new world. */
SurfaceTexel WorldTiles::SurfaceAt(TileIndex tile) const
{
	return tile.base() < this->surfaces.size() ? this->surfaces[tile.base()] : SurfaceTexel{};
}

GroundTexel WorldTiles::GroundAt(TileIndex tile) const
{
	return tile.base() < this->ground.size() ? this->ground[tile.base()] : GroundTexel{};
}

WaterTexel WorldTiles::WaterAt(TileIndex tile) const
{
	return tile.base() < this->water.size() ? this->water[tile.base()] : WaterTexel{};
}

NetworkTexel WorldTiles::NetworkAt(TileIndex tile) const
{
	return tile.base() < this->network.size() ? this->network[tile.base()] : NetworkTexel{};
}

RampTexel WorldTiles::RampAt(TileIndex tile) const
{
	return tile.base() < this->ramps.size() ? this->ramps[tile.base()] : RampTexel{};
}

bool WorldTiles::WaysEase(TileIndex tile) const
{
	return tile.base() < this->eases.size() && this->eases[tile.base()];
}

static bool PackEases(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_RAILWAY: return IsPlainRail(tile);
		case MP_ROAD: return IsNormalRoad(tile) && !IsPaved(GetRoadside(tile));
		default: return false;
	}
}

WorldTiles::Texels WorldTiles::Pack(TileIndex tile)
{
	return {PackSurface(tile), PackGround(tile), PackWater(tile), PackNetwork(tile), PackRamp(tile), PackEases(tile)};
}

WorldTiles::Texels WorldTiles::At(size_t i) const
{
	return {this->surfaces[i], this->ground[i], this->water[i], this->network[i], this->ramps[i], this->eases[i]};
}

void WorldTiles::Store(size_t i, const Texels &texels)
{
	this->surfaces[i] = texels.surface;
	this->ground[i] = texels.ground;
	this->water[i] = texels.water;
	this->network[i] = texels.network;
	this->ramps[i] = texels.ramp;
	this->eases[i] = texels.eases;
	this->peak = std::max(this->peak, HighestLevel(texels.surface));
}

void WorldTiles::Rebuild()
{
	this->size = Dimension(Map::SizeX(), Map::SizeY());
	uint count = Map::Size();
	this->surfaces.resize(count);
	this->ground.resize(count);
	this->water.resize(count);
	this->network.resize(count);
	this->ramps.resize(count);
	this->eases.resize(count);
	this->peak = 0;
	for (uint i = 0; i < count; i++) this->Store(i, Pack(TileIndex{i}));

	this->queued.assign(count, false);
	this->pending.clear();
	this->touched.clear();
	for (std::vector<uint32_t> &claims : this->claims) claims.assign(CeilDiv(this->size.width, BLOCK_TILES) * CeilDiv(this->size.height, BLOCK_TILES), 0);
	this->changes = {.whole = true};
	this->sweep_next = 0;
	this->stale = false;
}

void WorldTiles::Repack(TileIndex tile)
{
	size_t i = tile.base();
	Texels packed = Pack(tile);
	Texels stored = this->At(i);
	if (packed == stored) return;

	this->Store(i, packed);
	this->MarkChanged(tile, packed, stored);
}

/* A slow pass over the whole map catches the few changes the game never marks. */
void WorldTiles::Sweep()
{
	uint count = Map::Size();
	for (uint n = 0; n < SWEEP_TILES_PER_SYNC; n++) {
		this->Repack(TileIndex{this->sweep_next});
		this->sweep_next = (this->sweep_next + 1) % count;
	}
}

static ChangeKinds KindsChanged(const GroundTexel &packed, const GroundTexel &stored)
{
	ChangeKinds kinds;
	if (packed.flora != stored.flora) kinds.Set(ChangeKind::Flora);
	if (packed.material != stored.material || packed.detail != stored.detail || packed.variant != stored.variant) kinds.Set(ChangeKind::Cover);
	return kinds;
}

void WorldTiles::MarkChanged(TileIndex tile, const Texels &packed, const Texels &stored)
{
	bool water_changed = packed.water != stored.water;
	bool groundwork_changed = GroundworkOf(packed.ground, packed.network) != GroundworkOf(stored.ground, stored.network);
	bool ways_changed = packed.network != stored.network || packed.ramp != stored.ramp || packed.eases != stored.eases;
	if (packed.ground != stored.ground) _frame_profile.Count("tile_ground");
	if (water_changed) _frame_profile.Count("tile_water");
	if (groundwork_changed) _frame_profile.Count("tile_groundwork");
	if (packed.surface != stored.surface) _frame_profile.Count("tile_surface");
	if (packed.network.style != stored.network.style) _frame_profile.Count("tile_style");
	if (packed.network.track != stored.network.track || packed.network.road != stored.network.road || packed.network.tram != stored.network.tram) _frame_profile.Count("tile_ways");
	if (packed.eases != stored.eases || packed.ramp != stored.ramp) _frame_profile.Count("tile_eases");

	ChangeKinds kinds = KindsChanged(packed.ground, stored.ground);
	if (packed.surface != stored.surface) kinds.Set(ChangeKind::Shape);
	if (water_changed) kinds.Set(ChangeKind::Water);
	if (packed.network != stored.network) kinds.Set(ChangeKind::Ways);
	if (water_changed || groundwork_changed || ways_changed || packed.surface != stored.surface) kinds.Set(ChangeKind::Relief);
	for (ChangeKind kind : kinds) this->Claim(tile, kind);
}

/* The tile joins the span of its block for the kind of change, which starts at the first tile of the block to change so since the changes were last taken. */
void WorldTiles::Claim(TileIndex tile, ChangeKind kind)
{
	std::vector<uint32_t> &claims = this->claims[to_underlying(kind)];
	std::vector<Rect> &spans = this->changes.spans[to_underlying(kind)];
	int tx = static_cast<int>(TileX(tile));
	int ty = static_cast<int>(TileY(tile));
	size_t block = static_cast<size_t>(ty / BLOCK_TILES) * CeilDiv(this->size.width, BLOCK_TILES) + tx / BLOCK_TILES;
	if (claims[block] == 0) {
		spans.push_back({tx, ty, tx, ty});
		claims[block] = static_cast<uint32_t>(spans.size());
		return;
	}
	Rect &span = spans[claims[block] - 1];
	span = TakingIn(span, tx, ty);
}
