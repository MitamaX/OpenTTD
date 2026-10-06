/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_painter.cpp The structure pass of the map: the buildings standing on its tiles, and the overlay layer over them. */

#include "../../stdafx.h"
#include "map_painter.h"

#include <algorithm>
#include <optional>

#include "../../map_func.h"
#include "../../rail_map.h"
#include "../../road_map.h"
#include "../../station_map.h"
#include "../../tile_map.h"
#include "../core/canvas.h"
#include "../core/tones.h"
#include "structure_forms.h"
#include "tile_shapes.h"

#include "../../safeguards.h"

MapPainter _map_painter;

/* The overlay paints the whole map as dark greyscale, then repaints the active
 * layer's structures in its accent so they carry the frame. */
void MapPainter::Paint(MiniLayer filter)
{
	this->Survey(StructureSurvey(_camera.VisibleTiles(STRUCTURE_RISE_LEVELS)));
	bool layered = filter != MiniLayer::None;
	_canvas.SetGrey(layered);
	this->pass = MiniLayer::None;
	this->PaintPass();
	_canvas.SetGrey(false);
	if (!layered) return;

	this->pass = filter;
	this->PaintPass();
}

/* Bare ground, water, trees and the transport network stand in the 3D world; only buildings are drawn here. */
static bool CarriesStructure(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_VOID:
		case MP_CLEAR:
		case MP_TREES:
		case MP_TUNNELBRIDGE:
			return false;
		case MP_WATER:
			return IsShipDepot(tile);
		case MP_RAILWAY:
			return IsRailDepot(tile);
		case MP_ROAD:
			return IsRoadDepot(tile);
		case MP_STATION:
			return !HasStationRail(tile);
		default:
			return true;
	}
}

/* Tiles go down from the farthest from the eye to the nearest, so whatever stands nearer covers what stands behind it. */
void MapPainter::Survey(const TileSpan &span)
{
	this->tiles.clear();
	for (int tx = span.tx0; tx <= span.tx1; tx++) {
		for (int ty = span.ty0; ty <= span.ty1; ty++) {
			if (CarriesStructure(TileXY(tx, ty))) this->tiles.emplace_back(tx, ty);
		}
	}

	const Vec3 &eye = _camera.Eye();
	auto distance = [&eye](const std::pair<int, int> &tile) {
		auto [x, y] = TileCentre(tile.first, tile.second);
		return (x - eye.x) * (x - eye.x) + (y - eye.y) * (y - eye.y);
	};
	std::ranges::sort(this->tiles, std::ranges::greater{}, distance);
}

void MapPainter::PaintPass()
{
	for (auto [tx, ty] : this->tiles) this->DrawVolume(TileXY(tx, ty));
}

bool MapPainter::Shows(MiniLayer layer) const
{
	return this->pass == MiniLayer::None || this->pass == layer;
}

static MiniLayer LayerOf(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_RAILWAY: return MiniLayer::Rail;
		case MP_ROAD: return MiniLayer::Road;
		case MP_STATION: return IsAnyRoadStop(tile) ? MiniLayer::Road : MiniLayer::None;
		default: return MiniLayer::None;
	}
}

VolumeStyle MapPainter::VolumeStyleOf() const
{
	if (this->pass == MiniLayer::None) return VolumeStyle{};
	return VolumeStyle{this->pass == MiniLayer::Rail ? COL_RAIL_ACCENT : COL_ROAD_ACCENT};
}

/* A building is drawn piece by piece, each tile's piece in that tile's turn of the far to near walk. */
void MapPainter::DrawVolume(TileIndex tile)
{
	if (!this->Shows(LayerOf(tile)) || !_volume_painter.MayShow(tile)) return;
	std::optional<BuildingForm> form = StructureForm(tile);
	if (form.has_value()) _volume_painter.Draw(*form, tile, this->VolumeStyleOf());
}
