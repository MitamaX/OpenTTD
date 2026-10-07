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
#include <span>

#include "../../core/geometry_type.hpp"
#include "../core/camera.h"
#include "../map/world_tiles.h"
#include "scene_view.h"

class ChunkGrid {
public:
	explicit ChunkGrid(int chunk_tiles) : chunk_tiles(chunk_tiles) {}

	void Lay(Dimension map);
	void Clear() { this->Lay({}); }

	Dimension Map() const { return this->map; }
	size_t Count() const { return static_cast<size_t>(this->columns) * this->rows; }
	TileSpan TilesOf(size_t index) const;
	size_t IndexOf(int tx, int ty) const { return static_cast<size_t>(ty / this->chunk_tiles) * this->columns + tx / this->chunk_tiles; }

	/* Every block holding a tile of the areas or one within the margin of them. */
	template <class Visit>
	void ForEachTouched(std::span<const Rect> areas, int margin, Visit visit) const
	{
		if (this->Count() == 0) return;
		for (const Rect &area : areas) {
			int x0 = std::max(area.left - margin, 0) / this->chunk_tiles;
			int y0 = std::max(area.top - margin, 0) / this->chunk_tiles;
			int x1 = std::min((area.right + margin) / this->chunk_tiles, this->columns - 1);
			int y1 = std::min((area.bottom + margin) / this->chunk_tiles, this->rows - 1);
			for (int y = y0; y <= y1; y++) {
				for (int x = x0; x <= x1; x++) visit(static_cast<size_t>(y) * this->columns + x);
			}
		}
	}

	/* Every block within reach of the eye whose box, grown by a margin of tiles and standing up to the top, the frustum meets and in which a tile still spans the fewest pixels;
	 * the visit says whether to go on. */
	template <class Visit>
	void ForEachSeen(const SceneView &view, const Frustum &frustum, double fewest_pixels, int margin, double top, Visit visit) const
	{
		if (this->Count() == 0) return;
		double reach = view.focal / fewest_pixels + margin;
		auto chunk_at = [&](double at, int chunks) { return std::clamp(static_cast<int>(at) / this->chunk_tiles, 0, chunks - 1); };
		int x0 = chunk_at(view.eye.x - reach, this->columns);
		int x1 = chunk_at(view.eye.x + reach, this->columns);
		int y0 = chunk_at(view.eye.y - reach, this->rows);
		int y1 = chunk_at(view.eye.y + reach, this->rows);
		for (int y = y0; y <= y1; y++) {
			for (int x = x0; x <= x1; x++) {
				size_t index = static_cast<size_t>(y) * this->columns + x;
				TileSpan tiles = this->TilesOf(index);
				Vec3 low = {static_cast<double>(tiles.tx0 - margin), static_cast<double>(tiles.ty0 - margin), 0.0};
				Vec3 high = {static_cast<double>(tiles.tx1 + 1 + margin), static_cast<double>(tiles.ty1 + 1 + margin), top};
				if (!BoxMeets(frustum, low, high) || view.NearestTilePixels(low, high) < fewest_pixels) continue;
				if (!visit(index)) return;
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
