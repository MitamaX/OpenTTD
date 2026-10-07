/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file way_bends.cpp How far the rails' eased lines slide along each tile's sides, one texel per tile, so the far network's lines follow the same curves. */

#include "../../stdafx.h"
#include "way_bends.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "../../map_func.h"
#include "way_course.h"
#include "world_tiles.h"

#include "../../safeguards.h"

static constexpr uint ROWS_PER_REFRESH = 32;
static constexpr uint8_t UNBENT = 128;
static constexpr BendTexel STRAIGHT = {UNBENT, UNBENT, 0, 0};

WayBends _way_bends;

static uint8_t BendByte(double slide)
{
	return static_cast<uint8_t>(std::clamp<long>(std::lround(UNBENT + slide * BEND_STEPS_PER_TILE), 0, UINT8_MAX));
}

static BendTexel BendOf(int tx, int ty)
{
	if (_world_tiles.NetworkAt(TileXY(tx, ty)).track == 0) return STRAIGHT;
	auto [west, north] = SideSlides(tx, ty);
	return {BendByte(west), BendByte(north), 0, 0};
}

void WayBends::Refresh()
{
	Dimension map = _world_tiles.Size();
	if (map != this->size) {
		this->size = map;
		this->texels.assign(static_cast<size_t>(map.width) * map.height, STRAIGHT);
		this->revision = UINT64_MAX;
	}
	if (this->revision != _world_tiles.WaysRevision()) {
		this->revision = _world_tiles.WaysRevision();
		this->next_row = 0;
	}
	if (this->next_row >= this->size.height) return;

	uint last = std::min(this->next_row + ROWS_PER_REFRESH, this->size.height) - 1;
	for (uint ty = this->next_row; ty <= last; ty++) {
		for (uint tx = 0; tx < this->size.width; tx++) this->texels[static_cast<size_t>(ty) * this->size.width + tx] = BendOf(tx, ty);
	}
	Rect rows{0, static_cast<int>(this->next_row), static_cast<int>(this->size.width) - 1, static_cast<int>(last)};
	if (this->update.has_value()) {
		rows.top = std::min(rows.top, this->update->top);
		rows.bottom = std::max(rows.bottom, this->update->bottom);
	}
	this->update = rows;
	this->next_row = last + 1;
}

std::optional<Rect> WayBends::TakeUpdate()
{
	return std::exchange(this->update, std::nullopt);
}
