/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file car_look.h How a parked car looks wherever it stands: in a yard, up a drive or at the kerb. */

#ifndef MINI_MAP_CAR_LOOK_H
#define MINI_MAP_CAR_LOOK_H

#include <array>
#include <cstdint>

/* A car is a painted body under a glazed cabin roofed in the same paint, in tiles. */
inline constexpr float CAR_LENGTH = 0.17f;
inline constexpr float CAR_WIDTH = 0.08f;
inline constexpr float CAR_CLEARANCE = 0.008f;
inline constexpr float CAR_BODY_HEIGHT = 0.034f;
inline constexpr float CAB_SHARE = 0.55f;
inline constexpr float CAB_INSET = 0.008f;
inline constexpr float CAB_HEIGHT = 0.03f;
inline constexpr uint32_t CAR_GLASS = 0xFF2B3640U;
inline constexpr std::array<uint32_t, 10> CAR_TINTS = {
	0xFFB9BDC2U, 0xFFE8E8E4U, 0xFF2A2C30U, 0xFFB0302AU, 0xFF2E5A9AU, 0xFF2F5A40U, 0xFFC9B48AU, 0xFF6E7378U, 0xFFD8B03AU, 0xFF7A2638U,
};

#endif /* MINI_MAP_CAR_LOOK_H */
