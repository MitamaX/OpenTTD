/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file zoom_detail.h What the map shows at each zoom tier. */

#ifndef MINI_MAP_ZOOM_DETAIL_H
#define MINI_MAP_ZOOM_DETAIL_H

/* Below this many pixels a tile the map is a terrain overview; nearer it shows infrastructure. */
inline constexpr int INFRASTRUCTURE_PPT = 8;

/* A detail repeating along a tile fades in while one repeat grows across these many pixels. */
inline constexpr double UNRESOLVED_REPEAT_PIXELS = 1.5;
inline constexpr double RESOLVED_REPEAT_PIXELS = 4.0;

#endif /* MINI_MAP_ZOOM_DETAIL_H */
