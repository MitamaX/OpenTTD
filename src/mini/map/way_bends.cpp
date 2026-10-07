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
#include "tile_shapes.h"
#include "way_course.h"
#include "world_tiles.h"

#include "../../safeguards.h"

static constexpr uint TILES_PER_REFRESH = 8192;
/* A tile's texel reads the eases at its west and north sides, the first of them on the side of the tile before it. */
static constexpr int BEND_REACH = WAY_EASE_REACH + 1;
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

void WayBends::Sync(const WorldChanges &changes)
{
	if (changes.whole || _world_tiles.Size() != this->size) {
		this->size = _world_tiles.Size();
		this->texels.assign(static_cast<size_t>(this->size.width) * this->size.height, STRAIGHT);
		this->due.assign(1, this->Whole());
		return;
	}
	for (const Rect &relief : changes.Of(ChangeKind::Relief)) this->due.push_back(TilesNear(relief, BEND_REACH));
}

void WayBends::Refresh()
{
	uint tiles_left = TILES_PER_REFRESH;
	while (!this->due.empty() && tiles_left > 0) {
		Rect &area = this->due.front();
		int ty = area.top++;
		for (int tx = area.left; tx <= area.right; tx++) this->texels[static_cast<size_t>(ty) * this->size.width + tx] = BendOf(tx, ty);
		tiles_left -= std::min<uint>(tiles_left, area.Width());

		Rect row{area.left, ty, area.right, ty};
		if (this->update.has_value()) {
			row = {std::min(row.left, this->update->left), std::min(row.top, this->update->top), std::max(row.right, this->update->right), std::max(row.bottom, this->update->bottom)};
		}
		this->update = row;
		if (area.top > area.bottom) this->due.pop_front();
	}
}

Rect WayBends::Whole() const
{
	return {0, 0, static_cast<int>(this->size.width) - 1, static_cast<int>(this->size.height) - 1};
}

std::optional<Rect> WayBends::TakeUpdate()
{
	return std::exchange(this->update, std::nullopt);
}
