/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file blueprint.cpp The ghost of what the tool in hand would build, drawn over the map. */

#include "../../stdafx.h"
#include "blueprint.h"

#include <optional>

#include "../../gfx_func.h"
#include "../../map_func.h"
#include "../../mini_ui.h"
#include "../../newgrf_airport.h"
#include "../../settings_type.h"
#include "../../slope_func.h"
#include "../../station_type.h"
#include "../../tile_map.h"
#include "../../waypoint_func.h"
#include "../core/camera.h"
#include "../core/tones.h"
#include "../input/pointer_router.h"
#include "../map/tile_shapes.h"
#include "build_tool.h"
#include "clear_filter.h"
#include "tile_pick.h"
#include "tool_choices.h"
#include "tool_estimate.h"
#include "tool_sites.h"

#include "../../safeguards.h"

static constexpr uint FILL_ALPHA = 90;
static constexpr uint CUE_ALPHA = 70;
static constexpr uint FOOTPRINT_ALPHA = 60;
static constexpr uint BORE_ALPHA = 55;
static constexpr uint RAMP_ALPHA = 110;
static constexpr uint HOLE_ALPHA = 150;
static constexpr uint SITE_ALPHA = 35;
static constexpr uint SITE_EDGE_ALPHA = 130;
static constexpr uint CATCHMENT_ALPHA = 26;

static double HalfTiles(int pixels)
{
	return pixels / (2.0 * _camera.Ppt());
}

static double TrackHalfWidth(int ppt)
{
	return HalfTiles(std::max(2, ppt / 5));
}

static double BandHalfWidth(int ppt)
{
	return HalfTiles(std::max(2, ppt / 3));
}

static double SpanHalfWidth(int ppt)
{
	return HalfTiles(std::max(2, ppt / 2));
}

static double PlatformRailHalfWidth(int ppt)
{
	return HalfTiles(std::max(1, ppt / 6));
}

static int EdgeWidth(int ppt)
{
	return std::max(1, ppt / 8);
}

static int FineEdgeWidth(int ppt)
{
	return std::max(1, ppt / 10);
}

static uint32_t DragColour()
{
	return _tool.Removing() ? COL_BP_RM : COL_BP;
}

static void PaintFootprint(TileIndex tile, int w, int h, uint32_t c)
{
	int tx = TileX(tile);
	int ty = TileY(tile);
	FillArea(tx, ty, tx + w - 1, ty + h - 1, c, FOOTPRINT_ALPHA);
}

static void PaintRail(const RailPlan &plan, int ppt)
{
	uint32_t c = DragColour();
	for (size_t i = 0; i < plan.pieces.size(); i++) {
		auto [tile, track] = plan.pieces[i];
		DrawTrackPiece(track, TileX(tile), TileY(tile), TrackHalfWidth(ppt), _estimate.PlanColour(c, i));
	}
}

static void PaintRoad(const LinePlan &plan, int ppt)
{
	uint32_t c = DragColour();
	for (size_t i = 0; i < plan.tiles.size(); i++) {
		TileIndex tile = plan.tiles[i];
		DrawAxisBand(plan.axis, TileX(tile), TileY(tile), BandHalfWidth(ppt), _estimate.PlanColour(c, i));
	}
}

static void PaintBridge(const LinePlan &plan, int ppt)
{
	uint32_t c = plan.BridgeLength() > 0 ? _estimate.PlanColour(COL_BP) : COL_BP_RM;
	for (size_t i = 0; i < plan.tiles.size(); i++) {
		int tx = TileX(plan.tiles[i]);
		int ty = TileY(plan.tiles[i]);
		if (i == 0 || i + 1 == plan.tiles.size()) {
			FillTile(tx, ty, c, RAMP_ALPHA);
		} else {
			DrawAxisBand(plan.axis, tx, ty, SpanHalfWidth(ppt), c);
		}
	}
}

static void PaintSignals(const SignalPlan &plan, int ppt)
{
	uint32_t c = DragColour();
	for (size_t i = 0; i < plan.tiles.size(); i++) {
		int tx = TileX(plan.tiles[i]);
		int ty = TileY(plan.tiles[i]);
		uint32_t tc = _estimate.PlanColour(c, i);
		FillTile(tx, ty, tc, CUE_ALPHA);
		DrawTrackPiece(plan.track, tx, ty, TrackHalfWidth(ppt), tc);
	}
}

static AreaPlan VisiblePart(const AreaPlan &area)
{
	TileSpan seen = _camera.VisibleTiles();
	return {true, std::max(area.x0, seen.tx0), std::max(area.y0, seen.ty0), std::min(area.x1, seen.tx1), std::min(area.y1, seen.ty1)};
}

static void PaintArea(MiniTool kind, const AreaPlan &area, int ppt)
{
	if (!area.valid) return;

	uint32_t c = _estimate.PlanColour((_tool.Removing() || kind == MiniTool::Demolish) ? COL_BP_RM : COL_BP);
	/* A filter leaves most of the area standing, so the fill goes on the tiles
	 * that come off and the frame keeps showing how far the drag reaches. The
	 * area can be the whole map, so only what is on screen is walked. */
	bool filtered = _tool.FiltersClear();
	VisiblePart(area).ForEach([&](int tx, int ty) {
		if (!filtered || _clear_filter.Matches(TileXY(tx, ty))) FillTile(tx, ty, c, FILL_ALPHA);
	});
	FrameArea(area.x0, area.y0, area.x1, area.y1, EdgeWidth(ppt), c);

	/* A refused patch is painted over the area, so the run shows its holes
	 * before the drag is let go. */
	if (_estimate.FitsPerTile()) {
		area.ForEach([&](int tx, int ty) {
			if (_estimate.Fits(area.Index(tx, ty))) return;
			/* Outside the filter is not a hole; nothing was going to come
			 * off there in the first place. */
			if (filtered && !_clear_filter.Matches(TileXY(tx, ty))) return;
			FillTile(tx, ty, COL_BP_NO, HOLE_ALPHA);
		});
	}

	if (kind != MiniTool::Station || _tool.Removing()) return;
	Axis axis = _choices.StationAxis();
	area.ForEach([&](int tx, int ty) { DrawAxisBand(axis, tx, ty, PlatformRailHalfWidth(ppt), c); });
}

static void PaintDrag(MiniTool kind, int ppt)
{
	const ToolPlans &plans = _tool.Plans();
	if (kind == MiniTool::Rail) {
		PaintRail(plans.rail, ppt);
	} else if (kind == MiniTool::Road) {
		PaintRoad(plans.line, ppt);
	} else if (IsBridgeTool(kind)) {
		PaintBridge(plans.line, ppt);
	} else if (kind == MiniTool::Signal) {
		PaintSignals(plans.signal, ppt);
	} else if (IsRectTool(kind)) {
		PaintArea(kind, plans.area, ppt);
	}
}

static void PaintSites(int ppt)
{
	TileIndex hover = SiteTileAt(CursorPoint());
	int b = FineEdgeWidth(ppt);
	for (TileIndex tile : _sites.Tiles()) {
		if (tile == hover) continue;
		int tx = TileX(tile);
		int ty = TileY(tile);
		FillTile(tx, ty, COL_BP, SITE_ALPHA);
		FrameArea(tx, ty, tx, ty, b, COL_BP, SITE_EDGE_ALPHA);
	}
}

static void PaintSlopeFacing(TileIndex tile, int ppt, uint32_t c)
{
	DiagDirection d = GetInclinedSlopeDirection(GetTileSlope(tile));
	if (d != INVALID_DIAGDIR) DrawFacing(TileX(tile), TileY(tile), d, TrackHalfWidth(ppt), c);
}

/* The bore runs straight, so the box between the two mouths is the tunnel;
 * the far mouth is picked out because that is the tile the player cannot see
 * from here. */
static void PaintBore(TileIndex mouth, int ppt, uint32_t c)
{
	TileIndex end = _estimate.TunnelEnd();
	if (end == INVALID_TILE) return;

	int tx = TileX(mouth), ty = TileY(mouth);
	int ex = TileX(end), ey = TileY(end);
	FillArea(std::min(tx, ex), std::min(ty, ey), std::max(tx, ex), std::max(ty, ey), c, BORE_ALPHA);
	FillTile(ex, ey, c, FILL_ALPHA);
	FrameArea(ex, ey, ex, ey, EdgeWidth(ppt), c);
}

static void PaintClick(MiniTool kind, int ppt)
{
	uint32_t c = _estimate.PlanColour(_ctrl_pressed ? COL_BP_RM : COL_BP);
	TilePoint at = CursorPoint();
	TileIndex tile = SiteTileAt(at);
	int tx = TileX(tile);
	int ty = TileY(tile);
	FillTile(tx, ty, c, FILL_ALPHA);
	if (_ctrl_pressed) return;

	switch (kind) {
		case MiniTool::Signal: {
			Track track = SignalTrackAt(tile, at);
			if (track != INVALID_TRACK) DrawTrackPiece(track, tx, ty, TrackHalfWidth(ppt), c);
			break;
		}

		case MiniTool::BusStop:
		case MiniTool::TruckStop:
			if (_choices.StopThrough()) {
				DrawAxisBand(DiagDirToAxis(_choices.StopFacing()), tx, ty, BandHalfWidth(ppt), c);
			} else {
				DrawFacing(tx, ty, _choices.StopFacing(), BandHalfWidth(ppt), c);
			}
			break;

		case MiniTool::RailWaypoint:
		case MiniTool::RoadWaypoint: {
			Axis axis = kind == MiniTool::RailWaypoint ? GetAxisForNewRailWaypoint(tile) : GetAxisForNewRoadWaypoint(tile);
			if (IsValidAxis(axis)) DrawAxisBand(axis, tx, ty, BandHalfWidth(ppt), c);
			break;
		}

		case MiniTool::RailTunnel:
		case MiniTool::RoadTunnel:
			PaintSlopeFacing(tile, ppt, c);
			PaintBore(tile, ppt, c);
			break;

		case MiniTool::Dock:
		case MiniTool::Lock:
			PaintSlopeFacing(tile, ppt, c);
			break;

		/* The depot spans two tiles along its axis; show the real footprint. */
		case MiniTool::ShipDepot: {
			Axis a = DiagDirToAxis(_choices.PointFacing());
			PaintFootprint(tile, a == AXIS_X ? 2 : 1, a == AXIS_Y ? 2 : 1, c);
			break;
		}

		case MiniTool::Airport: {
			const AirportSpec *as = AirportSpec::Get(_choices.Airport());
			if (as->IsAvailable()) PaintFootprint(tile, as->size_x, as->size_y, c);
			break;
		}

		case MiniTool::Headquarters:
			PaintFootprint(tile, 2, 2, c);
			break;

		case MiniTool::TrainDepot:
		case MiniTool::RoadDepot:
			DrawFacing(tx, ty, _choices.PointFacing(), TrackHalfWidth(ppt), c);
			break;

		default:
			break;
	}
}

static void PaintHover(MiniTool kind)
{
	std::optional<TileIndex> tile = TileUnder(CursorPoint());
	if (!tile.has_value()) return;

	uint32_t c = kind == MiniTool::Demolish ? COL_BP_RM : COL_BP;
	/* The filter would take nothing here, so the cue stays but drops the
	 * removal red. */
	if (_tool.FiltersClear() && !_clear_filter.Matches(*tile)) c = COL_BP_NO;
	FillTile(TileX(*tile), TileY(*tile), c, CUE_ALPHA);
}

static uint CatchmentRadius(MiniTool kind)
{
	switch (kind) {
		case MiniTool::Station:
		case MiniTool::BusStop:
		case MiniTool::TruckStop:
		case MiniTool::Dock:
		case MiniTool::Airport:
			break;
		default:
			return CA_NONE;
	}
	if (!_settings_game.station.modified_catchment) return CA_UNMODIFIED;
	switch (kind) {
		case MiniTool::Station: return CA_TRAIN;
		case MiniTool::BusStop: return CA_BUS;
		case MiniTool::TruckStop: return CA_TRUCK;
		case MiniTool::Dock: return CA_DOCK;
		default: {
			const AirportSpec *as = AirportSpec::Get(_choices.Airport());
			return as->IsAvailable() ? as->catchment : CA_NONE;
		}
	}
}

static std::optional<AreaPlan> CatchmentCore(MiniTool kind)
{
	if (kind == MiniTool::Station && _tool.Dragging()) return _tool.Plans().area;

	std::optional<TileIndex> tile = TileUnder(CursorPoint());
	if (!tile.has_value()) return std::nullopt;

	int tx = TileX(*tile);
	int ty = TileY(*tile);
	if (kind != MiniTool::Airport) return AreaPlan{true, tx, ty, tx, ty};

	const AirportSpec *as = AirportSpec::Get(_choices.Airport());
	if (!as->IsAvailable()) return std::nullopt;
	return AreaPlan{true, tx, ty, tx + as->size_x - 1, ty + as->size_y - 1};
}

/* Placing a station blind is the one thing the mini UI cannot afford, so any
 * tool that creates catchment paints the square it will serve and picks out
 * the houses and industries inside it. */
static void PaintCatchment(MiniTool kind, int ppt)
{
	int r = static_cast<int>(CatchmentRadius(kind));
	if (r == 0) return;
	std::optional<AreaPlan> core = CatchmentCore(kind);
	if (!core.has_value()) return;

	AreaPlan served{
		true,
		std::max(0, core->x0 - r),
		std::max(0, core->y0 - r),
		std::min<int>(Map::SizeX() - 1, core->x1 + r),
		std::min<int>(Map::SizeY() - 1, core->y1 + r),
	};
	FillArea(served.x0, served.y0, served.x1, served.y1, MINI_CH_ACCENT, CATCHMENT_ALPHA);
	FrameArea(served.x0, served.y0, served.x1, served.y1, FineEdgeWidth(ppt), MINI_CH_ACCENT);

	served.ForEach([](int tx, int ty) {
		TileType tt = GetTileType(TileXY(tx, ty));
		if (tt == MP_HOUSE || tt == MP_INDUSTRY) FillTile(tx, ty, MINI_CH_ACCENT, FILL_ALPHA);
	});
}

/* The blueprint follows the pointer only while the pointer is on the map. */
void PaintBlueprint(int ppt)
{
	if (!_pointer.OnMap()) return;

	MiniTool kind = _tool.Kind();
	PaintCatchment(kind, ppt);
	if (_tool.Dragging()) {
		PaintDrag(kind, ppt);
	} else if (IsPointTool(kind)) {
		PaintSites(ppt);
		PaintClick(kind, ppt);
	} else if (kind != MiniTool::None) {
		PaintHover(kind);
	}
}
