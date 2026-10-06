/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file chunk_grid.h The map cut into square blocks of tiles, numbered row by row, and which blocks a change of the world reaches. */

#ifndef MINI_WORLD_CHUNK_GRID_H
#define MINI_WORLD_CHUNK_GRID_H

#include <algorithm>

#include "../../core/geometry_type.hpp"
#include "../core/camera.h"
#include "../map/world_tiles.h"

class ChunkGrid {
public:
	explicit ChunkGrid(int chunk_tiles) : chunk_tiles(chunk_tiles) {}

	void Lay(Dimension map);
	void Clear() { this->Lay({}); }

	Dimension Map() const { return this->map; }
	size_t Count() const { return static_cast<size_t>(this->columns) * this->rows; }
	TileSpan TilesOf(size_t index) const;

	/* Every block holding a changed tile or one within the margin of it. */
	template <class Visit>
	void ForEachTouched(const WorldChanges &changes, int margin, Visit visit) const
	{
		if (this->Count() == 0) return;
		for (const Rect &area : changes.areas) {
			int x0 = std::max(area.left - margin, 0) / this->chunk_tiles;
			int y0 = std::max(area.top - margin, 0) / this->chunk_tiles;
			int x1 = std::min((area.right + margin) / this->chunk_tiles, this->columns - 1);
			int y1 = std::min((area.bottom + margin) / this->chunk_tiles, this->rows - 1);
			for (int y = y0; y <= y1; y++) {
				for (int x = x0; x <= x1; x++) visit(static_cast<size_t>(y) * this->columns + x);
			}
		}
	}

private:
	int chunk_tiles;
	Dimension map{};
	int columns = 0;
	int rows = 0;
};

#endif /* MINI_WORLD_CHUNK_GRID_H */
