/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_painter.cpp The structure pass of the map: the steps between tiles, what stands on the ground, the bridges above it, and the overlay layer. */

#include "../../stdafx.h"
#include "map_painter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#include "../../bridge_map.h"
#include "../../direction_func.h"
#include "../../elrail_func.h"
#include "../../map_func.h"
#include "../../rail_map.h"
#include "../../road_map.h"
#include "../../settings_type.h"
#include "../../station_map.h"
#include "../../tile_map.h"
#include "../../track_func.h"
#include "../../tunnelbridge_map.h"
#include "../../water_map.h"
#include "../core/canvas.h"
#include "../core/tones.h"
#include "network_style.h"
#include "structure_forms.h"
#include "volume_painter.h"
#include "world_tiles.h"

#include "../../safeguards.h"

/* A deck stands at most one level above the highest ground, and a wire stands above its deck. */
static constexpr double DECK_LEVELS = 1.0;
static constexpr double CATENARY_LEVELS = CATENARY_RISE / LEVEL_TILES;
static constexpr double RAISED_LEVELS = DECK_LEVELS + CATENARY_LEVELS;
static constexpr std::array<double, 2> BOTH_SIDES = {-1.0, 1.0};
static constexpr double EDGE_HALF = 0.025;

/* The game stands a signal about a quarter tile in from the edge its trackdir enters by. */
static constexpr double SIGNAL_ALONG = 0.25;
static constexpr double SIGNAL_CLEARANCE = 0.08;
static constexpr double SIGNAL_OFFSET = RAIL_BED_HALF + SIGNAL_CLEARANCE;
static constexpr uint8_t SIGNAL_SIDE_LEFT = 0;
static constexpr uint8_t SIGNAL_SIDE_RIGHT = 2;
static constexpr uint8_t ROAD_SIDE_LEFT = 0;
static constexpr double LEFT_OF_TRAVEL = -1.0;
static constexpr double RIGHT_OF_TRAVEL = 1.0;

static constexpr int SIGNAL_RADIUS_SHARE = 10;
static constexpr int MIN_SIGNAL_RADIUS = 1;
static constexpr double ARROW_HALF_LENGTH = 0.125;
static constexpr double ARROW_HALF_BASE = 0.125;
static constexpr double NO_ENTRY_HALF_LENGTH = 0.25;
static constexpr double NO_ENTRY_HALF_WIDTH = 1.0 / 12.0;

static constexpr StructureTones DECK_TONES = {COL_BRIDGE, Darken(COL_BRIDGE)};
static constexpr StructureTones RAIL_STOP_TONES = {COL_ST_RAIL, COL_ST_RAIL_B};
static constexpr StructureTones ROAD_STOP_TONES = {COL_ST_ROAD, COL_ST_ROAD_B};

/* A rail or road surface as the ground shader draws it at the current zoom, so spans and platforms meet it edge to edge. */
struct Way {
	double half;
	uint32_t tone;
	double rail_half;
	bool wired;
};

MapPainter _map_painter;

static int SignalRadius(int ppt)
{
	return std::max(MIN_SIGNAL_RADIUS, ppt / SIGNAL_RADIUS_SHARE);
}

/* How far a detail repeating this often per tile has faded in, exactly as the ground shader resolves it. */
static double Resolved(double frequency)
{
	return SmoothStep(UNRESOLVED_REPEAT_PIXELS, RESOLVED_REPEAT_PIXELS, _camera.Ppt() / frequency);
}

static double CatenaryShown()
{
	return SmoothStep(CATENARY_FAR_PPT, CATENARY_NEAR_PPT, _camera.Ppt());
}

static Way RailWay(RailType railtype)
{
	RailLook look = RailLookOf(railtype);
	const RailLookWidths &widths = RAIL_LOOK_WIDTHS[to_underlying(look)];
	double detail = Resolved(SLEEPERS_PER_TILE);
	uint32_t surface = look == RailLook::Rail ? COL_BALLAST : COL_CONCRETE;
	return {
		.half = std::lerp(DISTANT_RAIL_HALF, widths.body, detail),
		.tone = Mix(COL_RAIL, surface, FadedAlpha(detail)),
		.rail_half = widths.strip,
		.wired = HasRailCatenaryDrawn(railtype),
	};
}

static Way RoadWay(TileIndex tile)
{
	bool asphalt = HasTileRoadType(tile, RTT_ROAD);
	uint32_t surface = asphalt ? COL_ASPHALT : COL_CONCRETE;
	return {
		.half = asphalt ? ROAD_HALF : TRAM_BED_HALF,
		.tone = Mix(COL_ROAD, surface, FadedAlpha(Resolved(MARKING_FREQUENCY))),
		.rail_half = HasTileRoadType(tile, RTT_TRAM) ? RAIL_HALF : NO_RAILS,
		.wired = false,
	};
}

static void DrawRails(const AxisRun &run, double rail_half)
{
	uint alpha = FadedAlpha(Resolved(RAIL_FREQUENCY));
	for (double side : BOTH_SIDES) FillAxisLine(run, side * RAIL_GAUGE_HALF, rail_half, COL_STEEL, alpha);
}

static void DrawCatenary(const AxisRun &run)
{
	FillAxisLine(run.Lifted(CATENARY_LEVELS), 0.0, WIRE_HALF, COL_WIRE, FadedAlpha(CatenaryShown()));
}

/* The ground pass surveys the tiles both passes walk, far enough out for the tallest and the widest building. */
void MapPainter::PaintGround(int ppt, MiniLayer filter)
{
	this->detail = ZoomDetail::For(ppt);
	this->ppt = ppt;
	this->filter = filter;
	this->Survey(StructureSurvey(_camera.VisibleTiles(std::max(RAISED_LEVELS, STRUCTURE_RISE_LEVELS))));
	this->EachPass(&MapPainter::PaintGroundPass);
}

void MapPainter::PaintRaised()
{
	this->EachPass(&MapPainter::PaintRaisedPass);
}

/* The overlay paints the whole map as dark greyscale, then repaints the active
 * layer's structures in its accent so they carry the frame. */
void MapPainter::EachPass(void (MapPainter::*paint)())
{
	bool layered = this->filter != MiniLayer::None;
	_canvas.SetGrey(layered);
	this->pass = MiniLayer::None;
	(this->*paint)();
	_canvas.SetGrey(false);
	if (!layered) return;

	this->pass = this->filter;
	(this->*paint)();
}

/* Bare ground, open water, trees, rails and roads belong to the ground shader; only what stands on them is drawn here. */
static bool CarriesStructure(TileIndex tile)
{
	if (IsBridgeAbove(tile)) return true;
	switch (GetTileType(tile)) {
		case MP_VOID:
		case MP_CLEAR:
		case MP_TREES:
			return false;
		case MP_WATER:
			return IsShipDepot(tile);
		default:
			return true;
	}
}

/* The coordinate i steps into lo..hi from the end farther from the viewer. */
static int FarToNear(int lo, int hi, double toward, int i)
{
	return toward < 0.0 ? hi - i : lo + i;
}

/* A step shows only on a side whose outward face turns toward the viewer. */
static DiagDirections FacingSides(MapVector toward)
{
	DiagDirections facing{};
	for (DiagDirection side = DIAGDIR_BEGIN; side < DIAGDIR_END; side++) {
		MapVector outward = Outward(side);
		if (outward.x * toward.x + outward.y * toward.y > 0.0) facing.Set(side);
	}
	return facing;
}

/* Walking each axis from its far end leaves out of depth order only tiles a tile's width apart across the screen, which cannot hide each other. */
void MapPainter::Survey(const TileSpan &span)
{
	this->tiles.clear();
	MapVector toward = _camera.Toward();
	this->facing = FacingSides(toward);
	for (int i = 0; i <= span.tx1 - span.tx0; i++) {
		int tx = FarToNear(span.tx0, span.tx1, toward.x, i);
		for (int j = 0; j <= span.ty1 - span.ty0; j++) {
			int ty = FarToNear(span.ty0, span.ty1, toward.y, j);
			if (CarriesStructure(TileXY(tx, ty)) || this->ShowsStep(tx, ty)) this->tiles.emplace_back(tx, ty);
		}
	}
}

bool MapPainter::ShowsStep(int tx, int ty) const
{
	for (DiagDirection side : this->facing) {
		if (StepFaceOf(tx, ty, side).has_value()) return true;
	}
	return false;
}

/* A portal lies on the step the hill shows over its tunnel's cut, so it shows only where that step turns toward the viewer. */
bool MapPainter::HidesPortal(TileIndex tile) const
{
	return IsTunnelTile(tile) && !this->facing.Test(ReverseDiagDir(GetTunnelBridgeDirection(tile)));
}

void MapPainter::PaintGroundPass()
{
	for (auto [tx, ty] : this->tiles) this->DrawGround(TileXY(tx, ty), tx, ty);
}

/* Steps, buildings and raised spans go down tile by tile from far to near, so nothing beside a pier covers its deck. */
void MapPainter::PaintRaisedPass()
{
	for (auto [tx, ty] : this->tiles) {
		TileIndex tile = TileXY(tx, ty);
		this->DrawSteps(tile, tx, ty);
		this->DrawVolume(tile);
		this->DrawRaised(tile, tx, ty);
	}
}

bool MapPainter::Shows(MiniLayer layer) const
{
	return this->pass == MiniLayer::None || this->pass == layer;
}

bool MapPainter::Accented() const
{
	return this->pass != MiniLayer::None;
}

uint32_t MapPainter::Accent() const
{
	return this->pass == MiniLayer::Rail ? COL_RAIL_ACCENT : COL_ROAD_ACCENT;
}

StructureTones MapPainter::Tones(const StructureTones &natural) const
{
	if (!this->Accented()) return natural;
	return {Darken(this->Accent()), this->Accent()};
}

static MiniLayer LayerOf(TransportType transport)
{
	switch (transport) {
		case TRANSPORT_RAIL: return MiniLayer::Rail;
		case TRANSPORT_ROAD: return MiniLayer::Road;
		default: return MiniLayer::None;
	}
}

static MiniLayer GroundLayer(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_RAILWAY: return MiniLayer::Rail;
		case MP_ROAD: return MiniLayer::Road;
		case MP_STATION:
			if (HasStationRail(tile)) return MiniLayer::Rail;
			return IsAnyRoadStop(tile) ? MiniLayer::Road : MiniLayer::None;
		case MP_TUNNELBRIDGE: return LayerOf(GetTunnelBridgeTransportType(tile));
		default: return MiniLayer::None;
	}
}

void MapPainter::DrawGround(TileIndex tile, int tx, int ty)
{
	if (!this->Shows(GroundLayer(tile))) return;
	if (OnScreen(ScreenQuadOf(TileQuad(tx, ty)))) this->DrawStructure(tile, tx, ty);
}

/* Buildings, depots and tunnel portals stand as volumes in the raised pass; the ground pass keeps what lies flat. */
void MapPainter::DrawStructure(TileIndex tile, int tx, int ty)
{
	switch (GetTileType(tile)) {
		case MP_RAILWAY:
			if (HasSignals(tile)) this->DrawSignals(tile, tx, ty);
			break;

		case MP_ROAD:
			if (IsNormalRoad(tile)) this->DrawOneWay(tile, tx, ty);
			break;

		case MP_STATION:
			this->DrawStation(tile, tx, ty);
			break;

		default:
			break;
	}
}

void MapPainter::DrawStation(TileIndex tile, int tx, int ty)
{
	if (HasStationRail(tile)) {
		this->DrawRailStop(tile, tx, ty);
	} else if (IsDriveThroughStopTile(tile)) {
		this->DrawPlatforms({tx, ty, GetDriveThroughStopAxis(tile)}, RoadWay(tile).half, ROAD_STOP_TONES);
	}
}

/* The ground shader leaves a station's wire to this pass, so the platforms never cover it. */
void MapPainter::DrawRailStop(TileIndex tile, int tx, int ty)
{
	AxisRun run{tx, ty, GetRailStationAxis(tile)};
	Way way = RailWay(GetRailType(tile));
	this->DrawPlatforms(run, way.half, RAIL_STOP_TONES);
	if (way.wired && !this->Accented()) DrawCatenary(run);
}

void MapPainter::DrawPlatforms(const AxisRun &run, double inner, const StructureTones &natural)
{
	StructureTones tones = this->Tones(natural);
	for (double side : BOTH_SIDES) {
		FillAxisStrip(run, side * inner, side * HALF_TILE, tones.fill);
		if (this->detail.block_borders) FillAxisLine(run, side * (inner + EDGE_HALF), EDGE_HALF, tones.edge);
	}
}

static double SignalSide()
{
	switch (_settings_game.construction.train_signal_side) {
		case SIGNAL_SIDE_LEFT: return LEFT_OF_TRAVEL;
		case SIGNAL_SIDE_RIGHT: return RIGHT_OF_TRAVEL;
		default: return _settings_game.vehicle.road_side == ROAD_SIDE_LEFT ? LEFT_OF_TRAVEL : RIGHT_OF_TRAVEL;
	}
}

void MapPainter::DrawSignals(TileIndex tile, int tx, int ty)
{
	if (!this->detail.signals) return;
	int r = SignalRadius(this->ppt);
	double offset = SignalSide() * SIGNAL_OFFSET;
	for (Track t : SetTrackBitIterator(GetTrackBits(tile))) {
		for (Trackdir td : {TrackToTrackdir(t), ReverseTrackdir(TrackToTrackdir(t))}) {
			if (!HasSignalOnTrackdir(tile, td)) continue;
			auto [px, py] = _camera.ScreenOf(TrackdirGroundPoint(tx, ty, td, SIGNAL_ALONG, offset));
			_canvas.FillCircle(px, py, r + 1, COL_INK);
			_canvas.FillCircle(px, py, r, GetSignalStateByTrackdir(tile, td) == SIGNAL_STATE_GREEN ? COL_GO : COL_STOP);
		}
	}
}

/* An arrowhead lying on the ground at the tile centre, pointing out of the heading side. */
static void FillArrow(int tx, int ty, DiagDirection heading, uint32_t c)
{
	TileGround ground(tx, ty);
	TilePoint centre = TileCentre(tx, ty);
	TilePoint tail = Shifted(centre, heading, -ARROW_HALF_LENGTH);
	DiagDirection side = ChangeDiagDir(heading, DIAGDIRDIFF_90RIGHT);
	_canvas.FillWorldQuad({
		ground.At(Shifted(centre, heading, ARROW_HALF_LENGTH)),
		ground.At(Shifted(tail, side, ARROW_HALF_BASE)),
		ground.At(tail),
		ground.At(Shifted(tail, side, -ARROW_HALF_BASE)),
	}, c);
}

/* A bar across the road, closed to traffic both ways. */
static void FillNoEntry(int tx, int ty, Axis road, uint32_t c)
{
	TilePoint centre = TileCentre(tx, ty);
	DiagDirection across = AxisToDiagDir(OtherAxis(road));
	FillGroundStroke(TileGround(tx, ty), Shifted(centre, across, -NO_ENTRY_HALF_LENGTH), Shifted(centre, across, NO_ENTRY_HALF_LENGTH), NO_ENTRY_HALF_WIDTH, c);
}

void MapPainter::DrawOneWay(TileIndex tile, int tx, int ty)
{
	if (!this->detail.oneway) return;
	DisallowedRoadDirections drd = GetDisallowedRoadDirections(tile);
	if (drd == DRD_NONE) return;

	RoadBits rb = GetRoadBits(tile, RTT_ROAD);
	bool axis_x = (rb & ROAD_X) == ROAD_X;
	bool axis_y = (rb & ROAD_Y) == ROAD_Y;
	if (axis_x == axis_y) return;

	Axis road = axis_x ? AXIS_X : AXIS_Y;
	if (drd == DRD_BOTH) {
		FillNoEntry(tx, ty, road, COL_STOP);
		return;
	}

	/* Northbound traffic heads toward smaller map coordinates. */
	DiagDirection southward = AxisToDiagDir(road);
	FillArrow(tx, ty, drd == DRD_SOUTHBOUND ? ReverseDiagDir(southward) : southward, COL_PAPER);
}

static AxisRun DeckRun(TileIndex head, int tx, int ty)
{
	RunEnd deck = GetBridgeHeight(head);
	return {tx, ty, DiagDirToAxis(GetTunnelBridgeDirection(head)), deck, deck};
}

VolumeStyle MapPainter::VolumeStyleOf() const
{
	return this->Accented() ? VolumeStyle{this->Accent()} : VolumeStyle{};
}

/* A step belongs to the higher tile and goes before what stands on it; it is bare ground, so the overlay greys it with the terrain. */
void MapPainter::DrawSteps(TileIndex tile, int tx, int ty)
{
	if (this->Accented()) return;
	for (DiagDirection side : this->facing) {
		std::optional<StepFace> step = StepFaceOf(tx, ty, side);
		if (step.has_value()) _volume_painter.DrawStep(tile, *step);
	}
}

/* A building is drawn piece by piece, each tile's piece in that tile's turn of the far to near walk. */
void MapPainter::DrawVolume(TileIndex tile)
{
	if (!this->Shows(GroundLayer(tile)) || this->HidesPortal(tile) || !_volume_painter.MayShow(tile)) return;
	std::optional<BuildingForm> form = StructureForm(tile);
	if (form.has_value()) _volume_painter.Draw(*form, tile, this->VolumeStyleOf());
}

/* A bridge passes over a tile at its own deck height, not at the ground below. */
void MapPainter::DrawRaised(TileIndex tile, int tx, int ty)
{
	if (IsBridgeTile(tile)) this->DrawSpan(tile, RampRun(tile));
	if (!IsBridgeAbove(tile)) return;

	TileIndex head = GetSouthernBridgeEnd(tile);
	this->DrawSpan(head, DeckRun(head, tx, ty));
}

void MapPainter::DrawSpan(TileIndex head, const AxisRun &run)
{
	MiniLayer layer = LayerOf(GetTunnelBridgeTransportType(head));
	if (!this->Shows(layer)) return;

	this->DrawSlab(run);
	if (layer == MiniLayer::None) return;

	Way way = layer == MiniLayer::Rail ? RailWay(GetRailType(head)) : RoadWay(head);
	FillAxisBand(run, way.half, this->Accented() ? this->Accent() : way.tone);
	if (this->Accented()) return;
	if (way.rail_half > NO_RAILS) DrawRails(run, way.rail_half);
	if (way.wired) DrawCatenary(run);
}

void MapPainter::DrawSlab(const AxisRun &run)
{
	StructureTones tones = this->Tones(DECK_TONES);
	FillAxisBand(run, DECK_HALF, tones.fill);
	if (!this->detail.block_borders) return;
	for (double side : BOTH_SIDES) FillAxisLine(run, side * (DECK_HALF - EDGE_HALF), EDGE_HALF, tones.edge);
}
