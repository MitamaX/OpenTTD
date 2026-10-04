/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_painter.cpp The tile pass of the top-down map: terrain, infrastructure and the overlay layer. */

#include "../../stdafx.h"
#include "map_painter.h"

#include "../../bridge_map.h"
#include "../../map_func.h"
#include "../../rail_map.h"
#include "../../road_map.h"
#include "../../station_map.h"
#include "../../tile_map.h"
#include "../../tunnelbridge_map.h"
#include "../../water_map.h"
#include "../../depot_map.h"
#include "../../elrail_func.h"
#include "../../gfx_func.h"
#include "../core/camera.h"
#include "../core/canvas.h"
#include "../core/tones.h"
#include "../core/tuning.h"
#include "ground.h"
#include "tile_shapes.h"

#include "../../safeguards.h"

MapPainter _map_painter;

void MapPainter::Paint(int ppt, MiniLayer filter)
{
	this->detail = ZoomDetail::For(ppt);
	TileSpan span = VisibleTiles();
	bool layered = filter != MiniLayer::None;

	this->tree_dots.clear();
	this->layer_tiles.clear();
	_canvas.SetGrey(layered);
	_canvas.FillRect(0, 0, _camera.Width() - 1, _camera.Height() - 1, COL_VOID);
	for (int ty = span.ty0; ty <= span.ty1; ty++) this->PaintRow(span, ty, ppt, layered);
	this->PaintGrid(span, ppt);
	this->PaintTrees(ppt);
	_canvas.SetGrey(false);
	if (layered) this->PaintLayer(ppt, filter);
}

MapPainter::TileSpan MapPainter::VisibleTiles()
{
	return {
		std::max(0, (int)std::floor(_camera.MapXAt(0))),
		std::max(0, (int)std::floor(_camera.MapYAt(0))),
		std::min<int>(Map::SizeX() - 1, (int)std::floor(_camera.MapXAt(_camera.Height() - 1))),
		std::min<int>(Map::SizeY() - 1, (int)std::floor(_camera.MapYAt(_camera.Width() - 1))),
	};
}

void MapPainter::PaintRow(const TileSpan &span, int ty, int ppt, bool layered)
{
	int run_start = -1;
	uint32_t run_c = 0;
	MiniSprite run_art = MiniSprite::End;
	auto flush = [&](int tx_end) {
		if (run_start < 0) return;
		auto [x0, y0, x1, y1] = _camera.AreaRect(run_start, ty, tx_end - 1, ty);
		if (run_art == MiniSprite::End || !MiniAtlasTileRun(run_art, x0, y0, x1, y1, tx_end - run_start, _canvas.Tone(run_c))) {
			_canvas.FillRect(x0, y0, x1, y1, run_c);
		}
		run_start = -1;
	};
	for (int tx = span.tx0; tx <= span.tx1; tx++) {
		TileIndex tile = TileXY(tx, ty);
		uint32_t c;
		bool tree_dot;
		MiniSprite art;
		if (this->TileRunColour(tile, tx, ty, ppt, c, tree_dot, art)) {
			if (run_start >= 0 && (c != run_c || art != run_art)) flush(tx);
			if (run_start < 0) {
				run_start = tx;
				run_c = c;
				run_art = art;
			}
			if (tree_dot) this->tree_dots.emplace_back(tx, ty);
		} else {
			flush(tx);
			this->DrawTile(tile, tx, ty, ppt);
			if (layered) this->layer_tiles.emplace_back(tx, ty);
		}
	}
	flush(span.tx1 + 1);
}

/* Deliberate tile grid at build zooms: merged runs are seamless, so tile
 * boundaries return as their own faint overlay instead of draw artefacts. */
void MapPainter::PaintGrid(const TileSpan &span, int ppt)
{
	if (ppt < INFRASTRUCTURE_PPT || _tuning.grid_alpha <= 0) return;

	int gx0 = std::max(0, _camera.ScreenX(span.ty0));
	int gx1 = std::min(_camera.Width() - 1, _camera.ScreenX(span.ty1 + 1) - 1);
	int gy0 = std::max(0, _camera.ScreenY(span.tx0));
	int gy1 = std::min(_camera.Height() - 1, _camera.ScreenY(span.tx1 + 1) - 1);
	for (int ty = span.ty0; ty <= span.ty1 + 1; ty++) {
		int x = _camera.ScreenX(ty);
		if (x >= 0 && x < _camera.Width()) _canvas.BlendRect(x, gy0, x, gy1, COL_SHADOW, _tuning.grid_alpha);
	}
	for (int tx = span.tx0; tx <= span.tx1 + 1; tx++) {
		int y = _camera.ScreenY(tx);
		if (y >= 0 && y < _camera.Height()) _canvas.BlendRect(gx0, y, gx1, y, COL_SHADOW, _tuning.grid_alpha);
	}
}

void MapPainter::PaintTrees(int ppt)
{
	int tree_r = std::max(1, ppt / 8);
	for (auto [tx, ty] : this->tree_dots) {
		_canvas.FillShapeRot(MiniSprite::Tree, (_camera.ScreenX(ty) + _camera.ScreenX(ty + 1) - 1) / 2, (_camera.ScreenY(tx) + _camera.ScreenY(tx + 1) - 1) / 2, tree_r, 0, COL_TREE);
	}
}

/* Runs only merge bare ground and water, so every tile that can carry
 * layer content already went through DrawTile and sits in layer_tiles. */
void MapPainter::PaintLayer(int ppt, MiniLayer filter)
{
	for (auto [tx, ty] : this->layer_tiles) this->DrawTileLayer(TileXY(tx, ty), tx, ty, ppt, filter);
}

void MapPainter::DrawSignals(TileIndex tile, int x0, int y0, int x1, int y1, int ppt)
{
	if (!this->detail.signals) return;
	int r = std::max(1, ppt / 10);
	int cx = (x0 + x1) / 2;
	int cy = (y0 + y1) / 2;
	int off = (int)((x1 - x0 + 1) * 0.36);
	for (Track t : {TRACK_X, TRACK_Y, TRACK_UPPER, TRACK_LOWER, TRACK_LEFT, TRACK_RIGHT}) {
		if (!HasSignalOnTrack(tile, t)) continue;
		for (Trackdir td : {TrackToTrackdir(t), ReverseTrackdir(TrackToTrackdir(t))}) {
			if (!HasSignalOnTrackdir(tile, td)) continue;
			DiagDirection d = TrackdirToExitdir(td);
			int px = cx + _diag_dx[d] * off;
			int py = cy + _diag_dy[d] * off;
			uint32_t c = GetSignalStateByTrackdir(tile, td) == SIGNAL_STATE_GREEN ? COL_GO : COL_STOP;
			_canvas.FillCircle(px, py, r + 1, COL_INK);
			_canvas.FillCircle(px, py, r, c);
		}
	}
}

void MapPainter::DrawOneWay(TileIndex tile, int x0, int y0, int x1, int y1, int ppt)
{
	if (!this->detail.oneway) return;
	DisallowedRoadDirections drd = GetDisallowedRoadDirections(tile);
	if (drd == DRD_NONE) return;

	int cx = (x0 + x1) / 2;
	int cy = (y0 + y1) / 2;
	int s = std::max(2, ppt / 4);

	if (drd == DRD_BOTH) {
		_canvas.FillRect(cx - s, cy - s / 3, cx + s, cy + s / 3, COL_STOP);
		return;
	}

	RoadBits rb = GetRoadBits(tile, RTT_ROAD);
	bool axis_x = (rb & ROAD_X) == ROAD_X;
	bool axis_y = (rb & ROAD_Y) == ROAD_Y;
	if (axis_x == axis_y) return;

	/* Northbound traffic heads toward smaller map coordinates. */
	int dir = drd == DRD_SOUTHBOUND ? -1 : 1;
	for (int i = 0; i <= s; i++) {
		int w = (s - i) / 2;
		if (axis_x) {
			int py = cy + dir * (i - s / 2);
			_canvas.FillRect(cx - w, py, cx + w, py, COL_PAPER);
		} else {
			int px = cx + dir * (i - s / 2);
			_canvas.FillRect(px, cy - w, px, cy + w, COL_PAPER);
		}
	}
}

void MapPainter::DrawBlock(MiniSprite s, int x0, int y0, int x1, int y1, int ppt, uint32_t fill, uint32_t border)
{
	if (ppt >= INFRASTRUCTURE_PPT && MiniAtlasHasArt(s) && MiniAtlasQuad(s, x0, y0, x1, y1, _canvas.Tone(fill))) return;
	if (!this->detail.block_borders) {
		_canvas.FillRect(x0, y0, x1, y1, fill);
		return;
	}
	int inset = std::max(1, ppt / 10);
	int b = std::max(1, ppt / 10);
	_canvas.FillRect(x0 + inset, y0 + inset, x1 - inset, y1 - inset, border);
	_canvas.FillRect(x0 + inset + b, y0 + inset + b, x1 - inset - b, y1 - inset - b, fill);
}

/* Dark block with a bright tick pointing out of the exit side. Depot art is
 * authored exit-up and rotates to the real exit instead of the tick. */
void MapPainter::DrawDepot(int x0, int y0, int x1, int y1, int ppt, DiagDirection exit)
{
	if (ppt >= INFRASTRUCTURE_PPT && MiniAtlasHasArt(MiniSprite::Depot)) {
		if (MiniAtlasQuadRot(MiniSprite::Depot, (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0 + 1) / 2, exit * 90, _canvas.Tone(COL_DEPOT))) return;
	}
	DrawBlock(MiniSprite::Depot, x0, y0, x1, y1, ppt, COL_DEPOT, COL_INK);
	if (!this->detail.block_borders) return;
	int cx = (x0 + x1) / 2;
	int cy = (y0 + y1) / 2;
	int w = std::max(2, ppt / 5);
	_canvas.ThickLine(cx, cy, cx + _diag_dx[exit] * (ppt / 2), cy + _diag_dy[exit] * (ppt / 2), w, COL_PAPER);
}

static void DrawWater(int x0, int y0, int x1, int y1, int ppt)
{
	if (ppt >= INFRASTRUCTURE_PPT && MiniAtlasHasArt(MiniSprite::Water) && MiniAtlasQuad(MiniSprite::Water, x0, y0, x1, y1, _canvas.Tone(COL_WATER))) return;
	_canvas.FillRect(x0, y0, x1, y1, COL_WATER);
}

void MapPainter::DrawTile(TileIndex tile, int tx, int ty, int ppt)
{
	auto [x0, y0, x1, y1] = _camera.TileRect(tx, ty);
	if (x1 < 0 || y1 < 0 || x0 >= _camera.Width() || y0 >= _camera.Height()) return;

	int rail_w = std::max(1, ppt / 6);
	int road_w = std::max(2, ppt / 3);
	int cat_w = rail_w >= 2 ? std::max(1, rail_w / 3) : 0;

	bool water_tile = false;

	switch (GetTileType(tile)) {
		case MP_VOID:
			_canvas.FillRect(x0, y0, x1, y1, COL_VOID);
			return;

		case MP_WATER:
			DrawWater(x0, y0, x1, y1, ppt);
			water_tile = true;
			if (IsShipDepot(tile)) DrawDepot(x0, y0, x1, y1, ppt, GetShipDepotDirection(tile));
			break;

		case MP_CLEAR:
			DrawGround(tile, x0, y0, x1, y1, ppt);
			break;

		case MP_TREES: {
			DrawGround(tile, x0, y0, x1, y1, ppt);
			if (this->detail.tree_dots) {
				int cx = (x0 + x1) / 2;
				int cy = (y0 + y1) / 2;
				int r = std::max(1, ppt / 8);
				_canvas.FillShapeRot(MiniSprite::Tree, cx, cy, r, 0, COL_TREE);
			}
			break;
		}

		case MP_RAILWAY:
			DrawGround(tile, x0, y0, x1, y1, ppt);
			if (IsRailDepot(tile)) {
				DrawDepot(x0, y0, x1, y1, ppt, GetRailDepotDirection(tile));
			} else {
				TrackBits bits = GetTrackBits(tile);
				DrawTrackBitsPx(bits, x0, y0, x1, y1, rail_w, COL_RAIL);
				if (cat_w > 0 && HasRailCatenary(GetRailType(tile))) {
					DrawTrackBitsPx(bits, x0, y0, x1, y1, cat_w, COL_CATENARY);
				}
				if (HasSignals(tile)) DrawSignals(tile, x0, y0, x1, y1, ppt);
			}
			break;

		case MP_ROAD:
			DrawGround(tile, x0, y0, x1, y1, ppt);
			if (IsLevelCrossing(tile)) {
				DrawAxisBand(GetCrossingRoadAxis(tile), x0, y0, x1, y1, road_w, COL_ROAD);
				DrawTrackBitsPx(GetCrossingRailBits(tile), x0, y0, x1, y1, rail_w, COL_RAIL);
			} else if (IsRoadDepot(tile)) {
				DrawDepot(x0, y0, x1, y1, ppt, GetRoadDepotDirection(tile));
			} else {
				RoadBits bits = GetAnyRoadBits(tile, RTT_ROAD, true) | GetAnyRoadBits(tile, RTT_TRAM, true);
				DrawRoadBitsPx(bits, x0, y0, x1, y1, road_w, COL_ROAD);
				if (IsNormalRoad(tile)) DrawOneWay(tile, x0, y0, x1, y1, ppt);
			}
			break;

		case MP_HOUSE:
			DrawGround(tile, x0, y0, x1, y1, ppt);
			DrawBlock(MiniSprite::House, x0, y0, x1, y1, ppt, COL_HOUSE, COL_HOUSE_B);
			break;

		case MP_INDUSTRY:
			DrawGround(tile, x0, y0, x1, y1, ppt);
			DrawBlock(MiniSprite::Industry, x0, y0, x1, y1, ppt, COL_IND, COL_IND_B);
			break;

		case MP_STATION: {
			uint32_t fill, border;
			bool on_water = false;
			switch (GetStationType(tile)) {
				case StationType::Rail:
				case StationType::RailWaypoint: fill = COL_ST_RAIL; border = COL_ST_RAIL_B; break;
				case StationType::Airport: fill = COL_ST_AIR; border = COL_ST_AIR_B; break;
				case StationType::Truck:
				case StationType::Bus:
				case StationType::RoadWaypoint: fill = COL_ST_ROAD; border = COL_ST_ROAD_B; break;
				case StationType::Dock: fill = COL_ST_DOCK; border = COL_ST_DOCK_B; on_water = true; break;
				case StationType::Buoy: fill = COL_ST_BUOY; border = COL_ST_BUOY_B; on_water = true; break;
				default: fill = COL_OBJ; border = COL_OBJ_B; on_water = true; break;
			}
			if (on_water) {
				DrawWater(x0, y0, x1, y1, ppt);
				water_tile = true;
			} else {
				DrawGround(tile, x0, y0, x1, y1, ppt);
			}
			DrawBlock(MiniSprite::Station, x0, y0, x1, y1, ppt, fill, border);
			if (IsDriveThroughStopTile(tile)) {
				DrawAxisBand(GetDriveThroughStopAxis(tile), x0, y0, x1, y1, road_w, COL_ROAD);
			}
			if (HasStationRail(tile)) {
				DrawAxisBand(GetRailStationAxis(tile), x0, y0, x1, y1, rail_w, COL_RAIL);
				if (cat_w > 0 && HasRailCatenary(GetRailType(tile))) {
					DrawAxisBand(GetRailStationAxis(tile), x0, y0, x1, y1, cat_w, COL_CATENARY);
				}
			}
			break;
		}

		case MP_OBJECT:
			DrawGround(tile, x0, y0, x1, y1, ppt);
			DrawBlock(MiniSprite::Object, x0, y0, x1, y1, ppt, COL_OBJ, COL_OBJ_B);
			break;

		case MP_TUNNELBRIDGE: {
			DrawGround(tile, x0, y0, x1, y1, ppt);
			Axis axis = DiagDirToAxis(GetTunnelBridgeDirection(tile));
			if (IsTunnel(tile)) {
				DrawBlock(MiniSprite::Tunnel, x0, y0, x1, y1, ppt, COL_TUNNEL, COL_RAIL);
			} else {
				DrawAxisBand(axis, x0, y0, x1, y1, road_w, COL_BRIDGE);
				if (cat_w > 0 && GetTunnelBridgeTransportType(tile) == TRANSPORT_RAIL && HasRailCatenary(GetRailType(tile))) {
					DrawAxisBand(axis, x0, y0, x1, y1, cat_w, COL_CATENARY);
				}
			}
			break;
		}

		default:
			_canvas.FillRect(x0, y0, x1, y1, COL_OBJ);
			break;
	}

	if (IsBridgeAbove(tile)) {
		DrawAxisBand(GetBridgeAxis(tile), x0, y0, x1, y1, road_w, COL_BRIDGE);
	}

	if (!water_tile) {
		int cw = std::max(1, ppt / 8);
		uint h = TileHeight(tile);
		if (tx + 1 < (int)Map::SizeX() && TileHeight(TileXY(tx + 1, ty)) != h) _canvas.BlendRect(x0, y1 - cw + 1, x1, y1, COL_SHADOW, _tuning.contour_alpha);
		if (ty + 1 < (int)Map::SizeY() && TileHeight(TileXY(tx, ty + 1)) != h) _canvas.BlendRect(x1 - cw + 1, y0, x1, y1, COL_SHADOW, _tuning.contour_alpha);
	}
}

/* Second pass for the overlay: the base map went down as dark greyscale and
 * the active layer's content is repainted in a bright accent so it carries
 * the frame. Rail reads as paper-white lines, road as catenary-yellow. */
void MapPainter::DrawTileLayer(TileIndex tile, int tx, int ty, int ppt, MiniLayer layer)
{
	auto [x0, y0, x1, y1] = _camera.TileRect(tx, ty);
	if (x1 < 0 || y1 < 0 || x0 >= _camera.Width() || y0 >= _camera.Height()) return;

	int rail_w = std::max(1, ppt / 6);
	int road_w = std::max(2, ppt / 3);
	int cat_w = rail_w >= 2 ? std::max(1, rail_w / 3) : 0;
	bool rail = layer == MiniLayer::Rail;
	uint32_t accent = rail ? COL_PAPER : COL_CATENARY;

	auto depot = [&](DiagDirection exit) {
		if (ppt >= INFRASTRUCTURE_PPT && MiniAtlasHasArt(MiniSprite::Depot)) {
			if (MiniAtlasQuadRot(MiniSprite::Depot, (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0 + 1) / 2, exit * 90, _canvas.Tone(accent))) return;
		}
		uint32_t fill = Darken(accent);
		DrawBlock(MiniSprite::Depot, x0, y0, x1, y1, ppt, fill, accent);
		if (!this->detail.block_borders) return;
		uint lum = (77 * ((fill >> 16) & 0xFFU) + 151 * ((fill >> 8) & 0xFFU) + 28 * (fill & 0xFFU)) >> 8;
		int cx = (x0 + x1) / 2;
		int cy = (y0 + y1) / 2;
		int w = std::max(2, ppt / 5);
		_canvas.ThickLine(cx, cy, cx + _diag_dx[exit] * (ppt / 2), cy + _diag_dy[exit] * (ppt / 2), w, lum >= 140 ? COL_INK : COL_PAPER);
	};

	switch (GetTileType(tile)) {
		case MP_RAILWAY:
			if (!rail) break;
			if (IsRailDepot(tile)) {
				depot(GetRailDepotDirection(tile));
			} else {
				TrackBits bits = GetTrackBits(tile);
				DrawTrackBitsPx(bits, x0, y0, x1, y1, rail_w, accent);
				if (cat_w > 0 && HasRailCatenary(GetRailType(tile))) {
					DrawTrackBitsPx(bits, x0, y0, x1, y1, cat_w, COL_CATENARY);
				}
				if (HasSignals(tile)) DrawSignals(tile, x0, y0, x1, y1, ppt);
			}
			break;

		case MP_ROAD:
			if (IsLevelCrossing(tile)) {
				if (rail) {
					DrawTrackBitsPx(GetCrossingRailBits(tile), x0, y0, x1, y1, rail_w, accent);
				} else {
					DrawAxisBand(GetCrossingRoadAxis(tile), x0, y0, x1, y1, road_w, accent);
				}
			} else if (!rail) {
				if (IsRoadDepot(tile)) {
					depot(GetRoadDepotDirection(tile));
				} else {
					RoadBits bits = GetAnyRoadBits(tile, RTT_ROAD, true) | GetAnyRoadBits(tile, RTT_TRAM, true);
					DrawRoadBitsPx(bits, x0, y0, x1, y1, road_w, accent);
					if (IsNormalRoad(tile)) DrawOneWay(tile, x0, y0, x1, y1, ppt);
				}
			}
			break;

		case MP_STATION:
			switch (GetStationType(tile)) {
				case StationType::Rail:
				case StationType::RailWaypoint:
					if (!rail) break;
					DrawBlock(MiniSprite::Station, x0, y0, x1, y1, ppt, COL_ST_RAIL, COL_ST_RAIL_B);
					DrawAxisBand(GetRailStationAxis(tile), x0, y0, x1, y1, rail_w, accent);
					if (cat_w > 0 && HasRailCatenary(GetRailType(tile))) {
						DrawAxisBand(GetRailStationAxis(tile), x0, y0, x1, y1, cat_w, COL_CATENARY);
					}
					break;
				case StationType::Truck:
				case StationType::Bus:
				case StationType::RoadWaypoint:
					if (rail) break;
					DrawBlock(MiniSprite::Station, x0, y0, x1, y1, ppt, COL_ST_ROAD, COL_ST_ROAD_B);
					if (IsDriveThroughStopTile(tile)) {
						DrawAxisBand(GetDriveThroughStopAxis(tile), x0, y0, x1, y1, road_w, accent);
					}
					break;
				default:
					break;
			}
			break;

		case MP_TUNNELBRIDGE: {
			TransportType tt = GetTunnelBridgeTransportType(tile);
			if (rail ? tt != TRANSPORT_RAIL : tt != TRANSPORT_ROAD) break;
			Axis axis = DiagDirToAxis(GetTunnelBridgeDirection(tile));
			if (IsTunnel(tile)) {
				DrawBlock(MiniSprite::Tunnel, x0, y0, x1, y1, ppt, COL_TUNNEL, accent);
			} else {
				DrawAxisBand(axis, x0, y0, x1, y1, road_w, accent);
				if (cat_w > 0 && rail && HasRailCatenary(GetRailType(tile))) {
					DrawAxisBand(axis, x0, y0, x1, y1, cat_w, COL_CATENARY);
				}
			}
			break;
		}

		default:
			break;
	}

	if (IsBridgeAbove(tile)) {
		TransportType tt = GetTunnelBridgeTransportType(GetSouthernBridgeEnd(tile));
		if (rail ? tt == TRANSPORT_RAIL : tt == TRANSPORT_ROAD) {
			DrawAxisBand(GetBridgeAxis(tile), x0, y0, x1, y1, road_w, accent);
		}
	}
}

/* A tile whose whole footprint is one solid colour can join a horizontal run
 * with equal neighbours; one rect per run keeps the command count far below
 * one per tile on open terrain and water. Tree tiles merge their ground too
 * and only defer the dot on top. Ground with art still merges: the run draws
 * as one repeat-wrapped quad instead of a rect, keyed by the art slot. */
bool MapPainter::TileRunColour(TileIndex tile, int tx, int ty, int ppt, uint32_t &c, bool &tree_dot, MiniSprite &art)
{
	tree_dot = false;
	art = MiniSprite::End;
	if (IsBridgeAbove(tile)) return false;
	switch (GetTileType(tile)) {
		case MP_VOID:
			c = COL_VOID;
			return true;

		case MP_WATER:
			if (IsShipDepot(tile)) return false;
			c = COL_WATER;
			if (ppt >= INFRASTRUCTURE_PPT && MiniAtlasHasArt(MiniSprite::Water)) art = MiniSprite::Water;
			return true;

		case MP_TREES:
			tree_dot = this->detail.tree_dots;
			[[fallthrough]];
		case MP_CLEAR: {
			auto [s, hbase] = GetTileSlopeZ(tile);
			if (s == SLOPE_FLAT) {
				c = GroundColour(tile, hbase);
				if (ppt >= INFRASTRUCTURE_PPT) {
					MiniSprite g = GroundSlot(tile);
					if (MiniAtlasHasArt(g)) art = g;
				}
			} else if (ppt < INFRASTRUCTURE_PPT) {
				c = GroundOverviewColour(tile, s, hbase);
			} else {
				return false;
			}
			uint h = TileHeight(tile);
			if (tx + 1 < (int)Map::SizeX() && TileHeight(TileXY(tx + 1, ty)) != h) return false;
			if (ty + 1 < (int)Map::SizeY() && TileHeight(TileXY(tx, ty + 1)) != h) return false;
			return true;
		}

		default:
			return false;
	}
}
