/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tile_shapes.cpp Track, road and band strokes across one tile's square. */

#include "../../stdafx.h"
#include "tile_shapes.h"

#include "../../track_func.h"
#include "../core/canvas.h"

#include "../../safeguards.h"

void DrawTrackPiece(Track t, int x0, int y0, int x1, int y1, int width, uint32_t c)
{
	int cx = (x0 + x1) / 2;
	int cy = (y0 + y1) / 2;
	switch (t) {
		case TRACK_X: _canvas.FillRect(cx - width / 2, y0, cx - width / 2 + width - 1, y1, c); break;
		case TRACK_Y: _canvas.FillRect(x0, cy - width / 2, x1, cy - width / 2 + width - 1, c); break;
		case TRACK_UPPER: _canvas.ThickLine(x0, cy, cx, y0, width, c); break;
		case TRACK_LOWER: _canvas.ThickLine(cx, y1, x1, cy, width, c); break;
		case TRACK_LEFT: _canvas.ThickLine(x0, cy, cx, y1, width, c); break;
		case TRACK_RIGHT: _canvas.ThickLine(cx, y0, x1, cy, width, c); break;
		default: break;
	}
}

void DrawTrackBitsPx(TrackBits bits, int x0, int y0, int x1, int y1, int width, uint32_t c)
{
	for (Track t : {TRACK_X, TRACK_Y, TRACK_UPPER, TRACK_LOWER, TRACK_LEFT, TRACK_RIGHT}) {
		if (bits & TrackToTrackBits(t)) DrawTrackPiece(t, x0, y0, x1, y1, width, c);
	}
}

void DrawRoadBitsPx(RoadBits bits, int x0, int y0, int x1, int y1, int width, uint32_t c)
{
	int cx = (x0 + x1) / 2;
	int cy = (y0 + y1) / 2;
	int lo = width / 2;
	if (bits & ROAD_NW) _canvas.FillRect(x0, cy - lo, cx, cy - lo + width - 1, c);
	if (bits & ROAD_SE) _canvas.FillRect(cx, cy - lo, x1, cy - lo + width - 1, c);
	if (bits & ROAD_NE) _canvas.FillRect(cx - lo, y0, cx - lo + width - 1, cy, c);
	if (bits & ROAD_SW) _canvas.FillRect(cx - lo, cy, cx - lo + width - 1, y1, c);
}

void DrawAxisBand(Axis axis, int x0, int y0, int x1, int y1, int width, uint32_t c)
{
	int cx = (x0 + x1) / 2;
	int cy = (y0 + y1) / 2;
	int lo = width / 2;
	if (axis == AXIS_X) {
		_canvas.FillRect(cx - lo, y0, cx - lo + width - 1, y1, c);
	} else {
		_canvas.FillRect(x0, cy - lo, x1, cy - lo + width - 1, c);
	}
}
