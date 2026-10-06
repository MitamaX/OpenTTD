/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_painter.h The structure pass of the map: the steps between tiles, what stands on the ground, the bridges above it, and the overlay layer. */

#ifndef MINI_MAP_MAP_PAINTER_H
#define MINI_MAP_MAP_PAINTER_H

#include <utility>
#include <vector>

#include "../../direction_type.h"
#include "../../tile_type.h"
#include "../core/camera.h"
#include "map_overlay.h"
#include "tile_shapes.h"
#include "volume_painter.h"
#include "zoom_detail.h"

struct StructureTones {
	uint32_t fill;
	uint32_t edge;
};

class MapPainter {
public:
	void PaintGround(int ppt, MiniLayer filter);
	void PaintRaised();

private:
	void Survey(const TileSpan &span);
	bool ShowsStep(int tx, int ty) const;
	bool HidesPortal(TileIndex tile) const;
	void EachPass(void (MapPainter::*paint)());
	void PaintGroundPass();
	void PaintRaisedPass();
	bool Shows(MiniLayer layer) const;
	bool Accented() const;
	uint32_t Accent() const;
	StructureTones Tones(const StructureTones &natural) const;
	VolumeStyle VolumeStyleOf() const;

	void DrawGround(TileIndex tile, int tx, int ty);
	void DrawStructure(TileIndex tile, int tx, int ty);
	void DrawStation(TileIndex tile, int tx, int ty);
	void DrawRailStop(TileIndex tile, int tx, int ty);
	void DrawPlatforms(const AxisRun &run, double inner, const StructureTones &natural);
	void DrawSignals(TileIndex tile, int tx, int ty);
	void DrawOneWay(TileIndex tile, int tx, int ty);

	void DrawSteps(TileIndex tile, int tx, int ty);
	void DrawVolume(TileIndex tile);
	void DrawRaised(TileIndex tile, int tx, int ty);
	void DrawSpan(TileIndex head, const AxisRun &run);
	void DrawSlab(const AxisRun &run);

	ZoomDetail detail{};
	int ppt = 0;
	MiniLayer filter = MiniLayer::None;
	MiniLayer pass = MiniLayer::None;
	DiagDirections facing{};
	std::vector<std::pair<int, int>> tiles;
};

extern MapPainter _map_painter;

#endif /* MINI_MAP_MAP_PAINTER_H */
