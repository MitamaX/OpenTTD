/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tile_shapes.cpp Shapes laid on the world: tiles, areas, strokes and runs, and the points they are built from. */

#include "../../stdafx.h"
#include "tile_shapes.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "../../direction_func.h"
#include "../../map_func.h"
#include "../../tile_map.h"
#include "../../track_func.h"

#include "../../safeguards.h"

static constexpr double HALF_PIXEL = 0.5;

/* A stroke's half width once it keeps a pixel across, and the alpha it fades to for the width it lacks. */
struct VisibleStroke {
	double half;
	uint alpha;
};

/* A tile corner as its offset from the tile's north corner. */
struct CornerOffset {
	int x;
	int y;
};

static constexpr CornerOffset NORTH_CORNER = {0, 0};
static constexpr CornerOffset WEST_CORNER = {1, 0};
static constexpr CornerOffset EAST_CORNER = {0, 1};
static constexpr CornerOffset SOUTH_CORNER = {1, 1};

/* Each side's two corners, left to right as seen from outside the tile, in DiagDirection order. */
static constexpr std::array<std::array<CornerOffset, 2>, DIAGDIR_END> SIDE_CORNERS = {{
	{EAST_CORNER, NORTH_CORNER},
	{SOUTH_CORNER, EAST_CORNER},
	{WEST_CORNER, SOUTH_CORNER},
	{NORTH_CORNER, WEST_CORNER},
}};

/* One end of a step's run along a side: where it stands, the neighbour's edge at its foot and the tile's own edge at its top. */
struct StepEnd {
	double x;
	double y;
	double foot;
	double top;

	double Rise() const { return this->top - this->foot; }
};

TileGround::TileGround(int tx, int ty) : tx(tx), ty(ty), surface(_world_tiles.SurfaceAt(TileXY(tx, ty)))
{
}

TileGround::TileGround(TileIndex tile) : TileGround(static_cast<int>(TileX(tile)), static_cast<int>(TileY(tile)))
{
}

double TileGround::Level(double x, double y) const
{
	return FacetLevel(this->surface, x - this->tx, y - this->ty);
}

double TileGround::Lowest() const
{
	return std::min({this->surface.north, this->surface.west, this->surface.east, this->surface.south});
}

WorldPoint Between(const WorldPoint &from, const WorldPoint &to, double share)
{
	return {from.x + (to.x - from.x) * share, from.y + (to.y - from.y) * share, from.level + (to.level - from.level) * share};
}

TilePoint TileCentre(int tx, int ty)
{
	return {tx + HALF_TILE, ty + HALF_TILE};
}

TilePoint Shifted(TilePoint from, DiagDirection side, double tiles)
{
	TileIndexDiffC step = TileIndexDiffCByDiagDir(side);
	return {from.first + step.x * tiles, from.second + step.y * tiles};
}

static TilePoint EdgeMidpoint(int tx, int ty, DiagDirection side)
{
	return Shifted(TileCentre(tx, ty), side, HALF_TILE);
}

static DiagDirection EntrySide(Trackdir td)
{
	return TrackdirToExitdir(ReverseTrackdir(td));
}

MapVector Outward(DiagDirection side)
{
	TileIndexDiffC step = TileIndexDiffCByDiagDir(side);
	return {static_cast<double>(step.x), static_cast<double>(step.y)};
}

bool OnMap(int tx, int ty)
{
	return tx >= 0 && ty >= 0 && tx <= static_cast<int>(Map::MaxX()) && ty <= static_cast<int>(Map::MaxY());
}

static uint8_t CornerLevel(const SurfaceTexel &surface, CornerOffset corner)
{
	if (corner.y == 0) return corner.x == 0 ? surface.north : surface.west;
	return corner.x == 0 ? surface.east : surface.south;
}

/* The same corner as the neighbour one step away names it. */
static CornerOffset SeenFrom(CornerOffset corner, TileIndexDiffC step)
{
	return {corner.x - step.x, corner.y - step.y};
}

static StepEnd StepEndAt(int tx, int ty, CornerOffset corner, const SurfaceTexel &own, const SurfaceTexel &next, TileIndexDiffC step)
{
	double foot = CornerLevel(next, SeenFrom(corner, step));
	double top = CornerLevel(own, corner);
	return {static_cast<double>(tx + corner.x), static_cast<double>(ty + corner.y), foot, top};
}

/* Where along the run from one end to the other the two edges cross. */
static StepEnd Crossing(const StepEnd &from, const StepEnd &to)
{
	double share = from.Rise() / (from.Rise() - to.Rise());
	return {std::lerp(from.x, to.x, share), std::lerp(from.y, to.y, share), std::lerp(from.foot, to.foot, share), std::lerp(from.top, to.top, share)};
}

/* Where the two edges cross, the tile keeps the part of the run it stands higher on and the neighbour owns the rest. */
std::optional<StepFace> StepFaceOf(int tx, int ty, DiagDirection side)
{
	TileIndexDiffC step = TileIndexDiffCByDiagDir(side);
	if (!OnMap(tx + step.x, ty + step.y)) return std::nullopt;
	TileIndex own_tile = TileXY(tx, ty);
	TileIndex next_tile = TileXY(tx + step.x, ty + step.y);
	SurfaceTexel own = _world_tiles.SurfaceAt(own_tile);
	SurfaceTexel next = _world_tiles.SurfaceAt(next_tile);
	auto [from_corner, to_corner] = SIDE_CORNERS[side];
	StepEnd from = StepEndAt(tx, ty, from_corner, own, next, step);
	StepEnd to = StepEndAt(tx, ty, to_corner, own, next, step);
	if (from.Rise() <= 0.0 && to.Rise() <= 0.0) return std::nullopt;
	if (IsTileType(own_tile, MP_VOID) || IsTileType(next_tile, MP_VOID)) return std::nullopt;

	if (from.Rise() < 0.0) {
		from = Crossing(from, to);
	} else if (to.Rise() < 0.0) {
		to = Crossing(from, to);
	}
	return StepFace{Outward(side), {{
		{from.x, from.y, from.top},
		{from.x, from.y, from.foot},
		{to.x, to.y, to.foot},
		{to.x, to.y, to.top},
	}}};
}

/* Pixels one tile of sideways offset spans across a line running along this map direction. */
static double PixelsAcross(MapVector along)
{
	ExactPoint ahead = _camera.ScreenStep(along);
	ExactPoint aside = _camera.ScreenStep({along.y, -along.x});
	return std::abs(ahead.x * aside.y - ahead.y * aside.x) / (std::hypot(ahead.x, ahead.y) * std::hypot(along.x, along.y));
}

static VisibleStroke Widened(MapVector along, double half_width, uint alpha)
{
	double half = std::max(half_width, HALF_PIXEL / PixelsAcross(along));
	return {half, FadedAlpha(half_width / half, alpha)};
}

std::array<WorldPoint, 4> TileQuad(int tx, int ty)
{
	TileGround ground(tx, ty);
	return {ground.At(tx, ty), ground.At(tx + 1, ty), ground.At(tx + 1, ty + 1), ground.At(tx, ty + 1)};
}

void FillTile(int tx, int ty, uint32_t c, uint alpha)
{
	_canvas.FillWorldQuad(TileQuad(tx, ty), c, alpha);
}

void FillArea(int tx0, int ty0, int tx1, int ty1, uint32_t c, uint alpha)
{
	TileSpan seen = _camera.VisibleTiles();
	for (int tx = std::max(tx0, seen.tx0); tx <= std::min(tx1, seen.tx1); tx++) {
		for (int ty = std::max(ty0, seen.ty0); ty <= std::min(ty1, seen.ty1); ty++) FillTile(tx, ty, c, alpha);
	}
}

static bool SamePoint(const WorldPoint &a, const WorldPoint &b)
{
	return a.x == b.x && a.y == b.y && a.level == b.level;
}

static void AddDistinct(std::vector<WorldPoint> &ring, const WorldPoint &point)
{
	if (ring.empty() || !SamePoint(ring.back(), point)) ring.push_back(point);
}

/* Walking a side left to right as seen from outside, each tile adds its own two corners on that side. */
static void AddSideCorners(std::vector<WorldPoint> &ring, int tx, int ty, DiagDirection side, int tiles)
{
	TileIndexDiffC walk = TileIndexDiffCByDiagDir(ChangeDiagDir(side, DIAGDIRDIFF_90LEFT));
	for (int i = 0; i < tiles; i++) {
		int x = tx + walk.x * i;
		int y = ty + walk.y * i;
		TileGround ground(x, y);
		for (CornerOffset corner : SIDE_CORNERS[side]) AddDistinct(ring, ground.At(x + corner.x, y + corner.y));
	}
}

/* The outline passes every tile corner along the area's edge on that tile's own ground, so it climbs every step the fill does. */
void FrameArea(int tx0, int ty0, int tx1, int ty1, int width, uint32_t c, uint alpha)
{
	int tiles_x = tx1 + 1 - tx0;
	int tiles_y = ty1 + 1 - ty0;
	std::vector<WorldPoint> ring;
	AddSideCorners(ring, tx0, ty0, DIAGDIR_NW, tiles_x);
	AddSideCorners(ring, tx1, ty0, DIAGDIR_SW, tiles_y);
	AddSideCorners(ring, tx1, ty1, DIAGDIR_SE, tiles_x);
	AddSideCorners(ring, tx0, ty1, DIAGDIR_NE, tiles_y);
	if (SamePoint(ring.front(), ring.back())) ring.pop_back();
	_canvas.FrameWorldRing(ring, width, c, alpha);
}

void FillGroundStroke(const TileGround &ground, TilePoint from, TilePoint to, double half_width, uint32_t c, uint alpha)
{
	auto [from_x, from_y] = from;
	auto [to_x, to_y] = to;
	MapVector along = {to_x - from_x, to_y - from_y};
	double length = std::hypot(along.x, along.y);
	if (length == 0.0) return;

	VisibleStroke stroke = Widened(along, half_width, alpha);
	double side_x = along.y / length * stroke.half;
	double side_y = -along.x / length * stroke.half;
	_canvas.FillWorldQuad({
		ground.At(from_x - side_x, from_y - side_y),
		ground.At(from_x + side_x, from_y + side_y),
		ground.At(to_x + side_x, to_y + side_y),
		ground.At(to_x - side_x, to_y - side_y),
	}, c, stroke.alpha);
}

void DrawTrackPiece(Track track, int tx, int ty, double half_width, uint32_t c, uint alpha)
{
	Trackdir td = TrackToTrackdir(track);
	FillGroundStroke(TileGround(tx, ty), EdgeMidpoint(tx, ty, EntrySide(td)), EdgeMidpoint(tx, ty, TrackdirToExitdir(td)), half_width, c, alpha);
}

void DrawAxisBand(Axis axis, int tx, int ty, double half_width, uint32_t c, uint alpha)
{
	DrawTrackPiece(AxisToTrack(axis), tx, ty, half_width, c, alpha);
}

void DrawFacing(int tx, int ty, DiagDirection direction, double half_width, uint32_t c)
{
	FillGroundStroke(TileGround(tx, ty), TileCentre(tx, ty), EdgeMidpoint(tx, ty, direction), half_width, c);
}

WorldPoint TrackdirGroundPoint(int tx, int ty, Trackdir td, double along, double lateral)
{
	auto [entry_x, entry_y] = EdgeMidpoint(tx, ty, EntrySide(td));
	auto [exit_x, exit_y] = EdgeMidpoint(tx, ty, TrackdirToExitdir(td));
	double dx = exit_x - entry_x;
	double dy = exit_y - entry_y;
	double length = std::hypot(dx, dy);
	return TileGround(tx, ty).At(entry_x + dx * along + dy / length * lateral, entry_y + dy * along - dx / length * lateral);
}

Point ScreenOfTileCentre(int tx, int ty)
{
	auto [x, y] = TileCentre(tx, ty);
	return _camera.ScreenOfGround(x, y);
}
