/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file material_palette.h The swatches building forms take their colours from, as 0xAARRGGBB. */

#ifndef MINI_MAP_MATERIAL_PALETTE_H
#define MINI_MAP_MATERIAL_PALETTE_H

#include <array>
#include <cstdint>
#include <span>

#include "../../core/enum_type.hpp"

enum class Swatch : uint8_t {
	Brick, Render, Timber, Stone, Concrete, Glass, Metal, Adobe, Candy,
	ClayTile, Slate, Shingle, Thatch, Seam, Gravel, Membrane, Plaster, Copper, Corrugated, CandyRoof,
	Frame, Awning, End,
};

inline constexpr uint32_t BRICK_SWATCH[] = {0xFFA8553FU, 0xFF8E4A3AU, 0xFFB86F4EU, 0xFFC49A6CU, 0xFFA0503AU};
inline constexpr uint32_t RENDER_SWATCH[] = {0xFFE8DCC4U, 0xFFDCD3C3U, 0xFFE6D2A8U, 0xFFC9C2B4U, 0xFFD9B8A0U, 0xFFB8C2B0U, 0xFFAFC0CCU};
inline constexpr uint32_t TIMBER_SWATCH[] = {0xFF8B2F25U, 0xFFC79A3AU, 0xFFE6E2D8U, 0xFF6A4B35U, 0xFF9DB4C4U, 0xFF5E7A5AU};
inline constexpr uint32_t STONE_SWATCH[] = {0xFFCFC3A8U, 0xFFB9AE96U, 0xFFA59F92U};
inline constexpr uint32_t CONCRETE_SWATCH[] = {0xFFB9B5ACU, 0xFF9FA3A6U, 0xFFC8C4BAU};
inline constexpr uint32_t GLASS_SWATCH[] = {0xFF6F8FA8U, 0xFF5A7590U, 0xFF7FA3A0U, 0xFF8A97A8U};
inline constexpr uint32_t METAL_SWATCH[] = {0xFF9AA3AAU, 0xFF7E8C96U, 0xFFB0A58EU};
inline constexpr uint32_t ADOBE_SWATCH[] = {0xFFF0EBE0U, 0xFFD9C29AU, 0xFFCF9B5AU, 0xFFE3B5A4U, 0xFF7FB8B0U};
inline constexpr uint32_t CANDY_SWATCH[] = {0xFFF29BC0U, 0xFF9BE3C4U, 0xFFF5E27AU, 0xFF8EC9F2U, 0xFFC6A6EEU, 0xFFFFB37AU};
inline constexpr uint32_t CLAY_TILE_SWATCH[] = {0xFFB0563AU, 0xFF9C4B33U, 0xFFC0703FU, 0xFF8A4A36U, 0xFF7C3A2CU, 0xFFC98A55U};
inline constexpr uint32_t SLATE_SWATCH[] = {0xFF4E5560U, 0xFF5D6670U, 0xFF434A52U, 0xFF3E4A5AU, 0xFF6B6F72U};
inline constexpr uint32_t SHINGLE_SWATCH[] = {0xFF6B5446U, 0xFF4F5A4CU, 0xFF5E5E62U, 0xFF7A4A3EU};
inline constexpr uint32_t THATCH_SWATCH[] = {0xFFB59A62U, 0xFF9E8452U};
inline constexpr uint32_t SEAM_SWATCH[] = {0xFF7A2E2AU, 0xFF3E5B45U, 0xFF35383CU, 0xFF5B6E80U};
inline constexpr uint32_t GRAVEL_SWATCH[] = {0xFF8C8A84U, 0xFF77746EU, 0xFF9A968CU, 0xFFA89F8EU};
inline constexpr uint32_t MEMBRANE_SWATCH[] = {0xFF55575AU, 0xFF62656AU, 0xFF8A8D90U, 0xFFBFC0BCU};
inline constexpr uint32_t PLASTER_SWATCH[] = {0xFFD8CDB8U, 0xFFCDBF9FU};
inline constexpr uint32_t COPPER_SWATCH[] = {0xFF6E9C8AU};
inline constexpr uint32_t CORRUGATED_SWATCH[] = {0xFF8C9399U, 0xFF9A7B5CU};
inline constexpr uint32_t CANDY_ROOF_SWATCH[] = {0xFFE0464EU, 0xFF3F7FE0U, 0xFFF28C38U, 0xFF8E5BD8U, 0xFF4CC26AU, 0xFFF2C94CU};
inline constexpr uint32_t FRAME_SWATCH[] = {0xFF8A9096U, 0xFF4A4E54U};
inline constexpr uint32_t AWNING_SWATCH[] = {0xFFB8463CU, 0xFF3F6E9AU, 0xFF4F8A4BU, 0xFFD9A441U};

inline constexpr std::array<std::span<const uint32_t>, to_underlying(Swatch::End)> SWATCH_TINTS = {{
	BRICK_SWATCH, RENDER_SWATCH, TIMBER_SWATCH, STONE_SWATCH, CONCRETE_SWATCH, GLASS_SWATCH, METAL_SWATCH, ADOBE_SWATCH, CANDY_SWATCH,
	CLAY_TILE_SWATCH, SLATE_SWATCH, SHINGLE_SWATCH, THATCH_SWATCH, SEAM_SWATCH, GRAVEL_SWATCH, MEMBRANE_SWATCH, PLASTER_SWATCH, COPPER_SWATCH, CORRUGATED_SWATCH, CANDY_ROOF_SWATCH,
	FRAME_SWATCH, AWNING_SWATCH,
}};

constexpr std::span<const uint32_t> SwatchTints(Swatch swatch)
{
	return SWATCH_TINTS[to_underlying(swatch)];
}

constexpr uint32_t SwatchTint(Swatch swatch, uint32_t bits)
{
	std::span<const uint32_t> tints = SwatchTints(swatch);
	return tints[bits % tints.size()];
}

#endif /* MINI_MAP_MATERIAL_PALETTE_H */
