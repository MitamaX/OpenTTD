/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rect_spans.h Areas of tiles taken in and joined as one. */

#ifndef MINI_WORLD_RECT_SPANS_H
#define MINI_WORLD_RECT_SPANS_H

#include <cstdint>

#include "../../core/geometry_type.hpp"
#include "../map/tile_shapes.h"

inline constexpr int64_t MOST_JOINED_SPREAD = 2;

inline int64_t AreaOf(const Rect &span)
{
	return static_cast<int64_t>(span.Width()) * span.Height();
}

inline Rect Enclosing(const Rect &a, const Rect &b)
{
	return TakingIn(TakingIn(a, b.left, b.top), b.right, b.bottom);
}

#endif /* MINI_WORLD_RECT_SPANS_H */
