/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file world_tiles.h Every tile of the map as the texels the ground shader reads. */

#ifndef MINI_MAP_WORLD_TILES_H
#define MINI_MAP_WORLD_TILES_H

#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "../../core/geometry_type.hpp"
#include "../../rail_type.h"
#include "../../tile_type.h"

enum class GroundMaterial : uint8_t {
	Void,
	Grass,
	Rough,
	Rocks,
	Fields,
	Snow,
	Desert,
	Shore,
	Paved,
	Dirt,
	End,
};

inline constexpr std::array<std::string_view, static_cast<size_t>(GroundMaterial::End)> GROUND_MATERIAL_NAMES = {
	"VOID", "GRASS", "ROUGH", "ROCKS", "FIELDS", "SNOW", "DESERT", "SHORE", "PAVED", "DIRT",
};

enum class TreeKind : uint8_t {
	Broadleaf,
	Conifer,
	Jungle,
	Palm,
	Cactus,
	Toy,
	End,
};

inline constexpr std::array<std::string_view, static_cast<size_t>(TreeKind::End)> TREE_KIND_NAMES = {
	"BROADLEAF", "CONIFER", "JUNGLE", "PALM", "CACTUS", "TOY",
};

enum class TreeAge : uint8_t {
	Sapling,
	Young,
	Grown,
	Dying,
	End,
};

enum class RailLook : uint8_t {
	Rail,
	Monorail,
	Maglev,
	End,
};

RailLook RailLookOf(RailType railtype);

inline constexpr uint8_t GROUND_DENSITY_MASK = 0x03;
inline constexpr uint8_t GROUND_LUSH_BIT = 0x04;
inline constexpr uint8_t GROUND_BUILT_BIT = 0x08;
inline constexpr uint8_t FLORA_COUNT_MASK = 0x07;
inline constexpr uint8_t FLORA_MOST_TREES = 4;
inline constexpr uint8_t FLORA_AGE_SHIFT = 3;
inline constexpr uint8_t FLORA_AGE_MASK = 0x03;
inline constexpr uint8_t FLORA_KIND_SHIFT = 5;
inline constexpr uint8_t NETWORK_RAIL_LOOK_MASK = 0x03;
inline constexpr uint8_t NETWORK_CATENARY_BIT = 0x04;
inline constexpr uint8_t NETWORK_KERB_BIT = 0x08;
inline constexpr uint8_t NETWORK_SIGNALS_BIT = 0x10;
inline constexpr uint8_t NETWORK_ONE_WAY_SHIFT = 5;
inline constexpr uint8_t NETWORK_BRIDGE_BIT = 0x80;
inline constexpr uint8_t FULL_WATER = 0xFF;
inline constexpr uint8_t HALF_WATER = 0x80;

struct SurfaceTexel {
	uint8_t north;
	uint8_t west;
	uint8_t east;
	uint8_t south;

	bool operator==(const SurfaceTexel &) const = default;
};

bool FoldsWestToEast(const SurfaceTexel &surface);
double FacetLevel(const SurfaceTexel &surface, double fx, double fy);

struct GroundTexel {
	GroundMaterial material;
	uint8_t detail;
	uint8_t flora;
	uint8_t variant;

	bool operator==(const GroundTexel &) const = default;
};

/** What stands on a tile, as far as the face where it steps above a neighbour goes: open ground, a way, or a building on its foundation. */
enum class Groundwork : uint8_t {
	Open,
	Way,
	Built,
};

struct NetworkTexel;
Groundwork GroundworkOf(const GroundTexel &ground, const NetworkTexel &network);

struct WaterTexel {
	uint8_t level;
	uint8_t sea;
	uint8_t canal;
	uint8_t river;

	bool operator==(const WaterTexel &) const = default;
};

struct NetworkTexel {
	uint8_t track;
	uint8_t road;
	uint8_t tram;
	uint8_t style;

	bool operator==(const NetworkTexel &) const = default;
};

inline constexpr uint8_t NO_RAMP = 0xFF;

/** A bridge head's ramp: the side it climbs toward onto the bridge, and the level its deck meets the bridge at. */
struct RampTexel {
	uint8_t onto = NO_RAMP;
	uint8_t deck = 0;

	bool Present() const { return this->onto != NO_RAMP; }
	bool operator==(const RampTexel &) const = default;
};

/* The blocks where any texel changed, and the fewer where the ground's shape, its water or the ways banked up on it did. */
struct WorldChanges {
	std::vector<Rect> areas;
	std::vector<Rect> reliefs;
	bool whole = false;
	bool water = false;
};

class WorldTiles {
public:
	void Reset();
	void Touch(TileIndex tile);
	void Sync();
	WorldChanges TakeChanges();
	/* The tiles the game marked that the last sync read again, whether their texels changed or not. */
	std::span<const TileIndex> Touched() const { return this->touched; }

	Dimension Size() const { return this->size; }
	uint Peak() const { return this->peak; }
	const SurfaceTexel *Surfaces() const { return this->surfaces.data(); }
	const GroundTexel *Ground() const { return this->ground.data(); }
	const WaterTexel *Water() const { return this->water.data(); }
	const NetworkTexel *Network() const { return this->network.data(); }
	SurfaceTexel SurfaceAt(TileIndex tile) const;
	GroundTexel GroundAt(TileIndex tile) const;
	WaterTexel WaterAt(TileIndex tile) const;
	NetworkTexel NetworkAt(TileIndex tile) const;
	RampTexel RampAt(TileIndex tile) const;
	/* Whether a tile's ways may be eased off its ground: plain track and country roads, not stations, depots, crossings, bridges, tunnels or town streets. */
	bool WaysEase(TileIndex tile) const;
	/* A count that moves on whenever the ground's shape or the ways on it change. */
	uint64_t WaysRevision() const { return this->ways_revision; }

private:
	struct Texels {
		SurfaceTexel surface;
		GroundTexel ground;
		WaterTexel water;
		NetworkTexel network;
		RampTexel ramp;
		bool eases;

		bool operator==(const Texels &) const = default;
	};

	static Texels Pack(TileIndex tile);
	Texels At(size_t i) const;
	void Store(size_t i, const Texels &texels);
	void Rebuild();
	void Repack(TileIndex tile);
	void Sweep();
	void MarkChanged(TileIndex tile, const Texels &packed, const Texels &stored);
	std::optional<Rect> ClaimBlock(TileIndex tile, std::vector<bool> &claimed) const;

	Dimension size{};
	uint peak = 0;
	std::vector<SurfaceTexel> surfaces;
	std::vector<GroundTexel> ground;
	std::vector<WaterTexel> water;
	std::vector<NetworkTexel> network;
	std::vector<RampTexel> ramps;
	std::vector<bool> eases;
	std::vector<TileIndex> pending;
	std::vector<TileIndex> touched;
	std::vector<bool> queued;
	std::vector<bool> changed_blocks;
	std::vector<bool> relief_blocks;
	WorldChanges changes;
	uint sweep_next = 0;
	uint64_t ways_revision = 0;
	bool stale = true;
};

extern WorldTiles _world_tiles;

#endif /* MINI_MAP_WORLD_TILES_H */
