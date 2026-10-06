/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file material_atlas.cpp Paints the building materials and wall strips into the map atlas, once per process, and finds their texture rectangles. */

#include "../../stdafx.h"
#include "material_atlas.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <optional>
#include <vector>

#include "facade_painters.h"
#include "luma_cell.h"
#include "surface_painters.h"

#include "../../safeguards.h"

static constexpr int MATERIAL_COUNT = to_underlying(Material::End);
static constexpr int GRID_COUNT = to_underlying(WindowGrid::End);
static constexpr int FACADE_COUNT = static_cast<int>(FACADE_STRIPS.size());
static constexpr int FIRST_GLAZING_STRIP = FACADE_COUNT;
static constexpr int STRIP_COUNT = FIRST_GLAZING_STRIP + GRID_COUNT;

static constexpr int GridBottom(const AtlasGrid &grid)
{
	return grid.origin_y + grid.rows * grid.pitch_y;
}

static constexpr int Capacity(const AtlasGrid &grid)
{
	return grid.columns * grid.rows;
}

static constexpr bool IsWellFormed(const AtlasGrid &grid)
{
	bool on_grain = grid.origin_y % ATLAS_GRAIN == 0 && grid.pitch_x % ATLAS_GRAIN == 0 && grid.pitch_y % ATLAS_GRAIN == 0;
	bool guttered = grid.pitch_x - grid.content_w == 2 * ATLAS_GUTTER && grid.pitch_y - grid.content_h == 2 * ATLAS_GUTTER;
	return on_grain && guttered && grid.columns * grid.pitch_x <= ATLAS_WIDTH;
}

static constexpr int MATERIAL_TOP = SURFACE_GRID.origin_y;
static constexpr int MATERIAL_BOTTOM = GridBottom(STRIP_GRID);

static_assert(std::has_single_bit(static_cast<uint>(ATLAS_WIDTH)) && std::has_single_bit(static_cast<uint>(ATLAS_HEIGHT)));
static_assert((1 << MAX_MIP_LEVEL) == ATLAS_GRAIN);
static_assert(IsWellFormed(SPRITE_GRID) && IsWellFormed(SURFACE_GRID) && IsWellFormed(STRIP_GRID));
static_assert(GridBottom(SPRITE_GRID) <= SURFACE_GRID.origin_y && GridBottom(SURFACE_GRID) <= STRIP_GRID.origin_y && MATERIAL_BOTTOM <= ATLAS_HEIGHT);
static_assert(SURFACE_GRID.content_w == TEXELS_PER_TILE && SURFACE_GRID.content_h == TEXELS_PER_TILE && Capacity(SURFACE_GRID) >= MATERIAL_COUNT);
static_assert(STRIP_GRID.content_w == TEXELS_PER_TILE && STRIP_GRID.content_h == STRIP_TEXELS && Capacity(STRIP_GRID) >= STRIP_COUNT);

namespace {

	struct MaterialPixels {
		std::vector<uint32_t> band;
		std::array<float, FACADE_STRIPS.size()> facade_means;
	};

}

static std::optional<MaterialPixels> _material_pixels;

static constexpr std::optional<int> FacadeIndex(Material wall, WindowGrid grid)
{
	for (int index = 0; index < FACADE_COUNT; index++) {
		if (FACADE_STRIPS[index].wall == wall && FACADE_STRIPS[index].grid == grid) return index;
	}
	return std::nullopt;
}

static int GlazingIndex(WindowGrid grid)
{
	return FIRST_GLAZING_STRIP + to_underlying(grid);
}

Point ContentOrigin(const AtlasGrid &grid, int index)
{
	return {(index % grid.columns) * grid.pitch_x + ATLAS_GUTTER, grid.origin_y + (index / grid.columns) * grid.pitch_y + ATLAS_GUTTER};
}

UvRect ContentUv(const AtlasGrid &grid, int index)
{
	Point origin = ContentOrigin(grid, index);
	float left = static_cast<float>(origin.x) / ATLAS_WIDTH;
	float top = static_cast<float>(origin.y) / ATLAS_HEIGHT;
	return {left, top, left + static_cast<float>(grid.content_w) / ATLAS_WIDTH, top + static_cast<float>(grid.content_h) / ATLAS_HEIGHT};
}

static void StoreInBand(MaterialPixels &pixels, const LumaCell &cell, const AtlasGrid &grid, int index, GutterMode mode)
{
	Point origin = ContentOrigin(grid, index);
	StoreCell(cell, {origin.x, origin.y - MATERIAL_TOP}, mode, pixels.band, ATLAS_WIDTH);
}

static MaterialPixels PaintMaterials()
{
	MaterialPixels pixels{std::vector<uint32_t>(static_cast<size_t>(MATERIAL_BOTTOM - MATERIAL_TOP) * ATLAS_WIDTH), {}};

	std::vector<LumaCell> surfaces;
	surfaces.reserve(MATERIAL_COUNT);
	for (int material = 0; material < MATERIAL_COUNT; material++) {
		surfaces.push_back(PaintSurface(static_cast<Material>(material)));
		StoreInBand(pixels, surfaces.back(), SURFACE_GRID, material, GutterMode::Wrap);
	}

	for (int index = 0; index < FACADE_COUNT; index++) {
		const FacadeStrip &strip = FACADE_STRIPS[index];
		LumaCell facade = PaintFacadeStrip(surfaces[to_underlying(strip.wall)], strip.wall, strip.grid);
		pixels.facade_means[index] = StoreyMean(facade, strip.grid);
		StoreInBand(pixels, facade, STRIP_GRID, index, GutterMode::WrapAcross);
	}

	for (int grid = 0; grid < GRID_COUNT; grid++) {
		WindowGrid window_grid = static_cast<WindowGrid>(grid);
		StoreInBand(pixels, PaintGlazingStrip(window_grid), STRIP_GRID, GlazingIndex(window_grid), GutterMode::WrapAcross);
	}
	return pixels;
}

void PaintMaterialCells(std::span<uint32_t> atlas)
{
	assert(atlas.size() == static_cast<size_t>(ATLAS_WIDTH) * ATLAS_HEIGHT);
	if (!_material_pixels.has_value()) _material_pixels = PaintMaterials();
	std::ranges::copy(_material_pixels->band, atlas.begin() + static_cast<ptrdiff_t>(MATERIAL_TOP) * ATLAS_WIDTH);
}

UvRect MaterialUv(Material material)
{
	return ContentUv(SURFACE_GRID, to_underlying(material));
}

UvRect FacadeUv(Material wall, WindowGrid grid)
{
	std::optional<int> index = FacadeIndex(wall, grid);
	assert(index.has_value());
	return index.has_value() ? ContentUv(STRIP_GRID, *index) : MaterialUv(wall);
}

UvRect GlazingUv(WindowGrid grid)
{
	return ContentUv(STRIP_GRID, GlazingIndex(grid));
}

float FacadeMean(Material wall, WindowGrid grid)
{
	std::optional<int> index = FacadeIndex(wall, grid);
	if (!_material_pixels.has_value() || !index.has_value()) return SURFACE_MEAN;
	return _material_pixels->facade_means[*index];
}
