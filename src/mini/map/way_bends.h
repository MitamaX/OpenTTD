/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file way_bends.h How far the rails' eased lines slide along each tile's sides, one texel per tile, so the far network's lines follow the same curves. */

#ifndef MINI_MAP_WAY_BENDS_H
#define MINI_MAP_WAY_BENDS_H

#include <deque>
#include <optional>
#include <vector>

#include "../../core/geometry_type.hpp"

struct WorldChanges;

/* A slide is stored as a byte about the middle of its range, this many steps to a tile. */
inline constexpr double BEND_STEPS_PER_TILE = 254.0;

/* How far the eased line slides along a tile's west side and along its north side. */
struct BendTexel {
	uint8_t west;
	uint8_t north;
	uint8_t spare_blue;
	uint8_t spare_alpha;
};

class WayBends {
public:
	/* The tiles whose sides a change of the ground's shape or the ways may ease differently are read again by the refreshes to come. */
	void Sync(const WorldChanges &changes);
	/* Reads a band of the tiles due at a time, so it runs only while the game's state holds still. */
	void Refresh();
	/* The rows read again since the last time they were taken. */
	std::optional<Rect> TakeUpdate();
	const BendTexel *Texels() const { return this->texels.data(); }

private:
	Rect Whole() const;

	std::vector<BendTexel> texels;
	Dimension size{};
	std::deque<Rect> due;
	std::optional<Rect> update;
};

extern WayBends _way_bends;

#endif /* MINI_MAP_WAY_BENDS_H */
