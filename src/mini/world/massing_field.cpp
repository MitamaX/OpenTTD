/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file massing_field.cpp The tallest body standing on every tile as a box, one texel per tile, for glass to find what it mirrors. */

#include "../../stdafx.h"
#include "massing_field.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <utility>

#include "../../map_func.h"
#include "../map/world_tiles.h"
#include "structure_mesh.h"

#include "../../safeguards.h"

struct Overlap {
	float low_x;
	float low_y;
	float high_x;
	float high_y;

	float Area() const { return (this->high_x - this->low_x) * (this->high_y - this->low_y); }
};

static std::optional<Overlap> OverlapOf(const Solid &solid, int cell_x, int cell_y)
{
	Overlap overlap = {std::max(solid.x0, static_cast<float>(cell_x)), std::max(solid.y0, static_cast<float>(cell_y)), std::min(solid.x1, cell_x + 1.0f), std::min(solid.y1, cell_y + 1.0f)};
	if (overlap.high_x <= overlap.low_x || overlap.high_y <= overlap.low_y || overlap.Area() < MASSING_LEAST_COVER) return std::nullopt;
	return overlap;
}

static uint16_t Rgb565(uint32_t argb)
{
	return static_cast<uint16_t>((Red(argb) >> 3) << 11 | (Green(argb) >> 2) << 5 | Blue(argb) >> 3);
}

static uint FormTexels(float tiles)
{
	return static_cast<uint>(std::lround(tiles * TEXELS_PER_TILE));
}

static uint InsetSteps(float inset)
{
	return std::min(static_cast<uint>(inset * MASSING_INSET_STEPS), MASSING_INSET_MOST);
}

MassingTexel MassingTexelOf(TileIndex tile, const std::optional<BuildingForm> &form)
{
	if (!form.has_value()) return {};
	int cell_x = static_cast<int>(TileX(tile)) - form->tx;
	int cell_y = static_cast<int>(TileY(tile)) - form->ty;
	const Solid *tallest = nullptr;
	Overlap box{};
	for (const Solid &solid : form->Solids()) {
		if (solid.role != SolidRole::Body || solid.kind == SolidKind::Decal) continue;
		std::optional<Overlap> overlap = OverlapOf(solid, cell_x, cell_y);
		if (!overlap.has_value() || (tallest != nullptr && solid.Top() <= tallest->Top())) continue;
		tallest = &solid;
		box = *overlap;
	}
	if (tallest == nullptr) return {};

	std::array<float, 4> insets = {box.low_x - cell_x, cell_x + 1.0f - box.high_x, box.low_y - cell_y, cell_y + 1.0f - box.high_y};
	uint raised = static_cast<uint>(std::clamp<long>(std::lround(form->floor - _world_tiles.SurfaceAt(tile).Lowest()), 0, MASSING_RAISED_MOST));
	uint traits = (to_underlying(tallest->windows) & MASSING_GRID_MASK) | (cell_x > 0 ? MASSING_FAR_X_BIT : 0) | (cell_y > 0 ? MASSING_FAR_Y_BIT : 0) | raised << MASSING_RAISED_SHIFT;
	for (uint side = 0; side < insets.size(); side++) traits |= InsetSteps(insets[side]) << (MASSING_INSET_SHIFT + side * MASSING_INSET_BITS);

	uint32_t wall = tallest->wall_material == Material::Glass ? tallest->glass_tint : tallest->wall_tint;
	uint heights = FormTexels(tallest->Top()) | FormTexels(tallest->base + tallest->wall) << MASSING_EAVES_SHIFT;
	return {static_cast<uint16_t>(heights), Rgb565(wall), Rgb565(tallest->roof_tint), static_cast<uint16_t>(traits)};
}

void MassingField::Clear(Dimension map)
{
	this->map = map;
	this->texels.assign(static_cast<size_t>(map.width) * map.height, {});
	this->moved.clear();
}

void MassingField::Note(TileIndex tile, const MassingTexel &texel)
{
	uint x = TileX(tile);
	uint y = TileY(tile);
	if (x >= this->map.width || y >= this->map.height) return;
	MassingTexel &kept = this->texels[static_cast<size_t>(y) * this->map.width + x];
	if (kept == texel) return;
	kept = texel;

	int left = static_cast<int>(x) / MASSING_BLOCK_TILES * MASSING_BLOCK_TILES;
	int top = static_cast<int>(y) / MASSING_BLOCK_TILES * MASSING_BLOCK_TILES;
	Rect block = {left, top, std::min<int>(left + MASSING_BLOCK_TILES, this->map.width) - 1, std::min<int>(top + MASSING_BLOCK_TILES, this->map.height) - 1};
	bool repeated = !this->moved.empty() && this->moved.back().left == block.left && this->moved.back().top == block.top;
	if (!repeated) this->moved.push_back(block);
}

std::vector<Rect> MassingField::TakeMoved()
{
	return std::exchange(this->moved, {});
}

const MassingTexel *MassingField::TexelAt(int x, int y) const
{
	if (x < 0 || y < 0 || static_cast<uint>(x) >= this->map.width || static_cast<uint>(y) >= this->map.height) return nullptr;
	const MassingTexel &texel = this->texels[static_cast<size_t>(y) * this->map.width + x];
	return texel.heights == 0 ? nullptr : &texel;
}

static float FormRise(uint texels)
{
	return static_cast<float>(texels) / TEXELS_PER_TILE * FORM_HEIGHT_SCALE;
}

std::optional<MassingBox> MassingField::BoxAt(int x, int y, double rise) const
{
	const MassingTexel *texel = this->TexelAt(x, y);
	if (texel == nullptr) return std::nullopt;
	std::array<float, 4> insets{};
	for (size_t side = 0; side < insets.size(); side++) insets[side] = static_cast<float>((texel->traits >> (MASSING_INSET_SHIFT + side * MASSING_INSET_BITS)) & MASSING_INSET_MOST) / MASSING_INSET_STEPS;
	float lowest = _world_tiles.SurfaceAt(TileXY(x, y)).Lowest() + static_cast<float>((texel->traits >> MASSING_RAISED_SHIFT) & MASSING_RAISED_MOST);
	float foot = lowest * static_cast<float>(rise);
	return MassingBox{x + insets[0], y + insets[2], x + 1.0f - insets[1], y + 1.0f - insets[3], foot, foot + FormRise(texel->heights & MASSING_LOW_BYTE)};
}

std::optional<TileIndex> MassingField::AnchorAt(int x, int y) const
{
	const MassingTexel *texel = this->TexelAt(x, y);
	if (texel == nullptr) return std::nullopt;
	return TileXY(x - ((texel->traits & MASSING_FAR_X_BIT) != 0 ? 1 : 0), y - ((texel->traits & MASSING_FAR_Y_BIT) != 0 ? 1 : 0));
}
