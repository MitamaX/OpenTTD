/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_painter.h The tile pass of the top-down map: terrain, infrastructure and the overlay layer. */

#ifndef MINI_MAP_MAP_PAINTER_H
#define MINI_MAP_MAP_PAINTER_H

#include <vector>

#include "../../direction_type.h"
#include "../../mini_atlas.h"
#include "../../tile_type.h"
#include "map_overlay.h"
#include "zoom_detail.h"

class MapPainter {
public:
	void Paint(int ppt, MiniLayer filter);

private:
	struct TileSpan {
		int tx0;
		int ty0;
		int tx1;
		int ty1;
	};

	static TileSpan VisibleTiles();
	void PaintRow(const TileSpan &span, int ty, int ppt, bool layered);
	void PaintGrid(const TileSpan &span, int ppt);
	void PaintTrees(int ppt);
	void PaintLayer(int ppt, MiniLayer filter);

	bool TileRunColour(TileIndex tile, int tx, int ty, int ppt, uint32_t &c, bool &tree_dot, MiniSprite &art);
	void DrawTile(TileIndex tile, int tx, int ty, int ppt);
	void DrawTileLayer(TileIndex tile, int tx, int ty, int ppt, MiniLayer layer);
	void DrawSignals(TileIndex tile, int x0, int y0, int x1, int y1, int ppt);
	void DrawOneWay(TileIndex tile, int x0, int y0, int x1, int y1, int ppt);
	void DrawBlock(MiniSprite s, int x0, int y0, int x1, int y1, int ppt, uint32_t fill, uint32_t border);
	void DrawDepot(int x0, int y0, int x1, int y1, int ppt, DiagDirection exit);

	ZoomDetail detail{};
	std::vector<std::pair<int, int>> tree_dots;
	std::vector<std::pair<int, int>> layer_tiles;
};

extern MapPainter _map_painter;

#endif /* MINI_MAP_MAP_PAINTER_H */
