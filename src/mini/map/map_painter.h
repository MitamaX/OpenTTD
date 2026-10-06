/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_painter.h The structure pass of the map: the buildings standing on its tiles, and the overlay layer over them. */

#ifndef MINI_MAP_MAP_PAINTER_H
#define MINI_MAP_MAP_PAINTER_H

#include <utility>
#include <vector>

#include "../../tile_type.h"
#include "../core/camera.h"
#include "map_overlay.h"
#include "volume_painter.h"

class MapPainter {
public:
	void Paint(MiniLayer filter);

private:
	void Survey(const TileSpan &span);
	void PaintPass();
	bool Shows(MiniLayer layer) const;
	VolumeStyle VolumeStyleOf() const;
	void DrawVolume(TileIndex tile);

	MiniLayer pass = MiniLayer::None;
	std::vector<std::pair<int, int>> tiles;
};

extern MapPainter _map_painter;

#endif /* MINI_MAP_MAP_PAINTER_H */
