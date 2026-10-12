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
	Frame, Mullion, Awning, End,
};

inline constexpr uint32_t WHITE_PAINT = 0xFFD4D2C9U;

inline constexpr uint32_t BRICK_SWATCH[] = {0xFFA8553FU, 0xFF8E4A3AU, 0xFFB86F4EU, 0xFFC49A6CU, 0xFFA0503AU};
inline constexpr uint32_t RENDER_SWATCH[] = {0xFFDCD1BAU, 0xFFDCD3C3U, 0xFFE6D2A8U, 0xFFC9C2B4U, 0xFFD9B8A0U, 0xFFB8C2B0U, 0xFFAFC0CCU};
inline constexpr uint32_t TIMBER_SWATCH[] = {0xFF8B2F25U, 0xFFC79A3AU, WHITE_PAINT, 0xFF6A4B35U, 0xFF9DB4C4U, 0xFF5E7A5AU};
inline constexpr uint32_t STONE_SWATCH[] = {0xFFB2A891U, 0xFF9F9681U, 0xFF8E897EU};
inline constexpr uint32_t CONCRETE_SWATCH[] = {0xFF9D9A92U, 0xFF878B8DU, 0xFFAAA79EU};
inline constexpr uint32_t GLASS_SWATCH[] = {0xFF6E8090U, 0xFF6C8884U, 0xFF887C6AU, 0xFF788A80U, 0xFF8E969CU};
inline constexpr uint32_t METAL_SWATCH[] = {0xFF9AA3AAU, 0xFF7E8C96U, 0xFFB0A58EU};
inline constexpr uint32_t ADOBE_SWATCH[] = {0xFFDDD9CEU, 0xFFD9C29AU, 0xFFCF9B5AU, 0xFFE3B5A4U, 0xFF7FB8B0U};
inline constexpr uint32_t CANDY_SWATCH[] = {0xFFF29BC0U, 0xFF9BE3C4U, 0xFFF5E27AU, 0xFF8EC9F2U, 0xFFC6A6EEU, 0xFFFFB37AU};
inline constexpr uint32_t CLAY_TILE_SWATCH[] = {0xFF984E38U, 0xFF884631U, 0xFF905438U, 0xFF784436U, 0xFF6C3A2EU, 0xFF8A5A40U};
inline constexpr uint32_t SLATE_SWATCH[] = {0xFF4C5058U, 0xFF5A5E64U, 0xFF45474CU, 0xFF4F4D55U, 0xFF62625FU};
inline constexpr uint32_t SHINGLE_SWATCH[] = {0xFF645042U, 0xFF4A544AU, 0xFF58585CU, 0xFF704438U};
inline constexpr uint32_t THATCH_SWATCH[] = {0xFFB59A62U, 0xFF9E8452U};
inline constexpr uint32_t SEAM_SWATCH[] = {0xFF7A2E2AU, 0xFF3E5B45U, 0xFF35383CU, 0xFF5B6E80U};
inline constexpr uint32_t GRAVEL_SWATCH[] = {0xFF8C8A84U, 0xFF77746EU, 0xFF9A968CU, 0xFFA89F8EU};
inline constexpr uint32_t MEMBRANE_SWATCH[] = {0xFF55575AU, 0xFF62656AU, 0xFF8A8D90U, 0xFFBFC0BCU};
inline constexpr uint32_t PLASTER_SWATCH[] = {0xFFD8CDB8U, 0xFFCDBF9FU};
inline constexpr uint32_t COPPER_SWATCH[] = {0xFF6E9C8AU};
inline constexpr uint32_t CORRUGATED_SWATCH[] = {0xFF8C9399U, 0xFF9A7B5CU};
inline constexpr uint32_t CANDY_ROOF_SWATCH[] = {0xFFE0464EU, 0xFF3F7FE0U, 0xFFF28C38U, 0xFF8E5BD8U, 0xFF4CC26AU, 0xFFF2C94CU};
inline constexpr uint32_t FRAME_SWATCH[] = {0xFFDBD9D1U, 0xFFE4DDCBU, 0xFF8A9096U, 0xFF4A4E54U};
inline constexpr uint32_t MULLION_SWATCH[] = {0xFF3A3E43U, 0xFF4D5258U, 0xFF4A4036U, 0xFF686E74U};
inline constexpr uint32_t AWNING_SWATCH[] = {0xFFB8463CU, 0xFF3F6E9AU, 0xFF4F8A4BU, 0xFFD9A441U};
inline constexpr uint32_t LEAF_TINT = 0xFF4E7A3AU;

inline constexpr std::array<std::span<const uint32_t>, to_underlying(Swatch::End)> SWATCH_TINTS = {{
	BRICK_SWATCH, RENDER_SWATCH, TIMBER_SWATCH, STONE_SWATCH, CONCRETE_SWATCH, GLASS_SWATCH, METAL_SWATCH, ADOBE_SWATCH, CANDY_SWATCH,
	CLAY_TILE_SWATCH, SLATE_SWATCH, SHINGLE_SWATCH, THATCH_SWATCH, SEAM_SWATCH, GRAVEL_SWATCH, MEMBRANE_SWATCH, PLASTER_SWATCH, COPPER_SWATCH, CORRUGATED_SWATCH, CANDY_ROOF_SWATCH,
	FRAME_SWATCH, MULLION_SWATCH, AWNING_SWATCH,
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
