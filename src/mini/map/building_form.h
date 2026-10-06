/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file building_form.h The solids a building is made of, as the form builders lay them out and the structure meshes stand them up. */

#ifndef MINI_MAP_BUILDING_FORM_H
#define MINI_MAP_BUILDING_FORM_H

#include <algorithm>
#include <array>
#include <cassert>
#include <span>
#include <string_view>

#include "../../core/enum_type.hpp"
#include "../../direction_type.h"
#include "../core/seed.h"
#include "../gpu/draw_list.h"
#include "world_tiles.h"

inline constexpr int TEXELS_PER_TILE = 64;
inline constexpr float MAX_STRUCTURE_TILES = 3.0f;
inline constexpr float TALLEST_BUILDING_TILES = 2.4f;
inline constexpr float PARAPET_HEIGHT = 0.03f;
inline constexpr uint8_t MAX_FOOTPRINT_TILES = 2;
inline constexpr double TINT_JITTER_BASE = 0.94;
inline constexpr double TINT_JITTER_SPAN = 0.12;
inline constexpr uint TINT_JITTER_BITS = 4;

enum class Material : uint8_t {
	Plain, Brick, Render, Timber, Stone, Concrete, Glass, Corrugated, Metal, Planks, Lattice,
	ClayTile, Slate, Shingle, Thatch, MetalSeam, Gravel, Membrane, RoofDeck, GlassRoof,
	Foundation, Asphalt, Hedge, Pickets, End,
};

enum class WindowGrid : uint8_t { None, Cottage, Terrace, Apartment, Shopfront, Office, Curtain, Arched, Industrial, Doors, End };

inline constexpr std::array<std::string_view, to_underlying(Material::End)> MATERIAL_NAMES = {
	"PLAIN", "BRICK", "RENDER", "TIMBER", "STONE", "CONCRETE", "GLASS", "CORRUGATED", "METAL", "PLANKS", "LATTICE",
	"CLAY_TILE", "SLATE", "SHINGLE", "THATCH", "METAL_SEAM", "GRAVEL", "MEMBRANE", "ROOF_DECK", "GLASS_ROOF",
	"FOUNDATION", "ASPHALT", "HEDGE", "PICKETS",
};

inline constexpr std::array<std::string_view, to_underlying(WindowGrid::End)> WINDOW_GRID_NAMES = {
	"NONE", "COTTAGE", "TERRACE", "APARTMENT", "SHOPFRONT", "OFFICE", "CURTAIN", "ARCHED", "INDUSTRIAL", "DOORS",
};

/* A facade's ground band and storeys, the bays whole numbers of which fit a wall, and the windows standing evenly across each bay. */
struct FacadeMetrics {
	uint8_t ground_texels;
	uint8_t storey_texels;
	uint8_t bay_texels;
	uint8_t pitch_texels;
};

inline constexpr std::array<FacadeMetrics, to_underlying(WindowGrid::End)> FACADE_METRICS = {{
	{8, 8, 64, 64}, {8, 8, 64, 16}, {8, 8, 32, 16}, {8, 8, 16, 16}, {12, 8, 16, 16},
	{12, 8, 8, 8}, {12, 8, 8, 8}, {16, 16, 16, 16}, {16, 16, 32, 32}, {16, 16, 32, 32},
}};

struct FacadeStrip {
	Material wall;
	WindowGrid grid;
};

inline constexpr std::array<FacadeStrip, 28> FACADE_STRIPS = {{
	{Material::Brick, WindowGrid::Cottage}, {Material::Render, WindowGrid::Cottage}, {Material::Timber, WindowGrid::Cottage}, {Material::Stone, WindowGrid::Cottage},
	{Material::Brick, WindowGrid::Terrace}, {Material::Render, WindowGrid::Terrace},
	{Material::Brick, WindowGrid::Apartment}, {Material::Render, WindowGrid::Apartment}, {Material::Concrete, WindowGrid::Apartment}, {Material::Stone, WindowGrid::Apartment},
	{Material::Brick, WindowGrid::Shopfront}, {Material::Render, WindowGrid::Shopfront}, {Material::Stone, WindowGrid::Shopfront}, {Material::Concrete, WindowGrid::Shopfront},
	{Material::Concrete, WindowGrid::Office}, {Material::Stone, WindowGrid::Office}, {Material::Brick, WindowGrid::Office},
	{Material::Glass, WindowGrid::Curtain},
	{Material::Stone, WindowGrid::Arched}, {Material::Brick, WindowGrid::Arched}, {Material::Render, WindowGrid::Arched},
	{Material::Brick, WindowGrid::Industrial}, {Material::Corrugated, WindowGrid::Industrial}, {Material::Concrete, WindowGrid::Industrial},
	{Material::Corrugated, WindowGrid::Doors}, {Material::Brick, WindowGrid::Doors}, {Material::Metal, WindowGrid::Doors}, {Material::Concrete, WindowGrid::Doors},
}};

constexpr const FacadeMetrics &FacadeMetricsOf(WindowGrid grid)
{
	return FACADE_METRICS[to_underlying(grid)];
}

constexpr bool HasFacade(Material wall, WindowGrid grid)
{
	return std::ranges::any_of(FACADE_STRIPS, [&](const FacadeStrip &strip) { return strip.wall == wall && strip.grid == grid; });
}

constexpr float WallTiles(WindowGrid grid, int storeys)
{
	const FacadeMetrics &metrics = FacadeMetricsOf(grid);
	return (metrics.ground_texels + std::max(storeys - 1, 0) * metrics.storey_texels) / static_cast<float>(TEXELS_PER_TILE);
}

constexpr float GroundStoreyTiles(WindowGrid grid)
{
	return WallTiles(grid, 1);
}

constexpr int StoreysFor(WindowGrid grid, float wall_tiles)
{
	const FacadeMetrics &metrics = FacadeMetricsOf(grid);
	float upper_storeys = (wall_tiles * TEXELS_PER_TILE - metrics.ground_texels) / metrics.storey_texels;
	return 1 + std::max(0, static_cast<int>(upper_storeys + 0.5f));
}

enum class SolidKind : uint8_t { Box, Cylinder, Decal };
enum class SolidRole : uint8_t { Body, Detail };
/* What a solid stands for, so a detailed model can dress it as one; smoke rises from the top of a smokestack. */
enum class Fixture : uint8_t { None, Chimney, RooftopUnit, WaterTank, Smokestack };
enum class RoofShape : uint8_t { Flat, Parapet, Gable, Hip, Pyramid, Shed, Sawtooth, Vault, Dome, Cone };

struct Solid {
	SolidKind kind = SolidKind::Box;
	SolidRole role = SolidRole::Body;
	Fixture fixture = Fixture::None;
	float x0 = 0.0f;
	float y0 = 0.0f;
	float x1 = 1.0f;
	float y1 = 1.0f;
	float base = 0.0f;
	float wall = 0.0f;
	float rise = 0.0f;
	float taper = 0.0f;
	RoofShape roof = RoofShape::Flat;
	Axis ridge = AXIS_X;
	DiagDirection high_side = DIAGDIR_NE;
	DiagDirections fronts{};
	Material wall_material = Material::Plain;
	WindowGrid windows = WindowGrid::None;
	Material roof_material = Material::Plain;
	uint32_t wall_tint = 0xFFFFFFFFU;
	uint32_t roof_tint = 0xFFFFFFFFU;
	uint32_t glass_tint = 0xFF7F9FB4U;

	float Top() const { return this->base + this->wall + this->rise; }
};

inline constexpr size_t MAX_SOLIDS = 12;

struct BuildingForm {
	int tx = 0;
	int ty = 0;
	uint8_t size_x = 1;
	uint8_t size_y = 1;
	float floor = 0.0f;
	uint8_t count = 0;
	std::array<Solid, MAX_SOLIDS> solids{};

	void Add(const Solid &solid)
	{
		assert(this->count < MAX_SOLIDS);
		this->solids[this->count++] = solid;
	}

	std::span<const Solid> Solids() const { return {this->solids.data(), this->count}; }
};

struct FloraPatch {
	TreeKind kind;
	TreeAge age;
	uint8_t count;
};

constexpr uint32_t ScaledRgb(uint32_t argb, double factor)
{
	auto scaled = [factor](uint channel) { return static_cast<uint>(std::clamp(channel * factor + 0.5, 0.0, static_cast<double>(CHANNEL_MAX))); };
	return PackArgb(Alpha(argb), scaled(Red(argb)), scaled(Green(argb)), scaled(Blue(argb)));
}

constexpr uint32_t TintJitter(uint32_t argb, uint32_t seed, uint first)
{
	return ScaledRgb(argb, TINT_JITTER_BASE + TINT_JITTER_SPAN * SeedShare(seed, first, TINT_JITTER_BITS));
}

#endif /* MINI_MAP_BUILDING_FORM_H */
