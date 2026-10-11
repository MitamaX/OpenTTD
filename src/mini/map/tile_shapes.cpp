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
#include <utility>
#include <vector>

#include "../../direction_func.h"
#include "../../map_func.h"
#include "../../tile_map.h"
#include "../../track_func.h"

#include "../../safeguards.h"

static constexpr double HALF_PIXEL = 0.5;
static constexpr double GROUND_PATH_STEP = 0.5;
static constexpr int CURVED_SHAPE_DIVISIONS = 4;
static constexpr int GROUND_PATH_REACH = 8;

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
	return this->surface.Lowest();
}

DrawnTile::DrawnTile(std::unique_ptr<DrawnSurface> surface, int tx, int ty) : surface(std::move(surface)), tx(tx), ty(ty)
{
}

int DrawnTile::Divisions() const
{
	return this->surface->Curves() ? CURVED_SHAPE_DIVISIONS : 1;
}

DrawnGround::DrawnGround(const TileSpan &tiles) : tiles(tiles), span(GroundOver(tiles))
{
}

bool DrawnGround::Holds(int tx, int ty) const
{
	return this->tiles.Holds(tx, ty);
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

Rect TilesNear(const Rect &area, int reach)
{
	Rect grown = area.Expand(reach);
	return {std::max(grown.left, 0), std::max(grown.top, 0), std::min(grown.right, static_cast<int>(Map::MaxX())), std::min(grown.bottom, static_cast<int>(Map::MaxY()))};
}

Rect TakingIn(const Rect &area, int tx, int ty)
{
	return {std::min(area.left, tx), std::min(area.top, ty), std::max(area.right, tx), std::max(area.bottom, ty)};
}

/* The same corner as the neighbour one step away names it. */
static CornerOffset SeenFrom(CornerOffset corner, TileIndexDiffC step)
{
	return {corner.x - step.x, corner.y - step.y};
}

static StepEnd StepEndAt(int tx, int ty, CornerOffset corner, const SurfaceTexel &own, const SurfaceTexel &next, TileIndexDiffC step)
{
	CornerOffset across = SeenFrom(corner, step);
	double foot = next.CornerAt(across.x, across.y);
	double top = own.CornerAt(corner.x, corner.y);
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
	return std::abs(ahead.x * aside.y - ahead.y * aside.x) / (std::hypot(ahead.x, ahead.y) * Length(along));
}

static VisibleStroke Widened(MapVector along, double half_width, uint alpha)
{
	double half = std::max(half_width, HALF_PIXEL / PixelsAcross(along));
	return {half, FadedAlpha(half_width / half, alpha)};
}

static void FillOn(const DrawnTile &tile, int tx, int ty, uint32_t c, uint alpha)
{
	int divisions = tile.Divisions();
	for (int j = 0; j < divisions; j++) {
		for (int i = 0; i < divisions; i++) {
			auto at = [&](int across, int down) { return tile.At(tx + static_cast<double>(i + across) / divisions, ty + static_cast<double>(j + down) / divisions); };
			_canvas.FillWorldQuad({at(0, 0), at(1, 0), at(1, 1), at(0, 1)}, c, alpha);
		}
	}
}

void FillTile(int tx, int ty, uint32_t c, uint alpha)
{
	FillOn(DrawnGround({tx, ty, tx, ty}).Tile(tx, ty), tx, ty, c, alpha);
}

void FillArea(int tx0, int ty0, int tx1, int ty1, uint32_t c, uint alpha)
{
	TileSpan seen = _camera.VisibleTiles();
	TileSpan shown = {std::max(tx0, seen.tx0), std::max(ty0, seen.ty0), std::min(tx1, seen.tx1), std::min(ty1, seen.ty1)};
	if (shown.tx0 > shown.tx1 || shown.ty0 > shown.ty1) return;
	DrawnGround ground(shown);
	for (int tx = shown.tx0; tx <= shown.tx1; tx++) {
		for (int ty = shown.ty0; ty <= shown.ty1; ty++) FillOn(ground.Tile(tx, ty), tx, ty, c, alpha);
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

/* Walking a side left to right as seen from outside, each tile adds its own two corners on that side, and the points between them where its ground curves. */
static void AddSideCorners(std::vector<WorldPoint> &ring, const DrawnGround &ground, int tx, int ty, DiagDirection side, int tiles)
{
	TileIndexDiffC walk = TileIndexDiffCByDiagDir(ChangeDiagDir(side, DIAGDIRDIFF_90LEFT));
	auto [first, last] = SIDE_CORNERS[side];
	for (int i = 0; i < tiles; i++) {
		int x = tx + walk.x * i;
		int y = ty + walk.y * i;
		DrawnTile tile = ground.Tile(x, y);
		int divisions = tile.Divisions();
		for (int step = 0; step <= divisions; step++) {
			double share = static_cast<double>(step) / divisions;
			AddDistinct(ring, tile.At(x + first.x + (last.x - first.x) * share, y + first.y + (last.y - first.y) * share));
		}
	}
}

/* The outline passes every tile corner along the area's edge on that tile's own ground, so it climbs every step the fill does. */
void FrameArea(int tx0, int ty0, int tx1, int ty1, int width, uint32_t c, uint alpha)
{
	int tiles_x = tx1 + 1 - tx0;
	int tiles_y = ty1 + 1 - ty0;
	DrawnGround ground({tx0, ty0, tx1, ty1});
	std::vector<WorldPoint> ring;
	AddSideCorners(ring, ground, tx0, ty0, DIAGDIR_NW, tiles_x);
	AddSideCorners(ring, ground, tx1, ty0, DIAGDIR_SW, tiles_y);
	AddSideCorners(ring, ground, tx1, ty1, DIAGDIR_SE, tiles_x);
	AddSideCorners(ring, ground, tx0, ty1, DIAGDIR_NE, tiles_y);
	if (SamePoint(ring.front(), ring.back())) ring.pop_back();
	_canvas.FrameWorldRing(ring, width, c, alpha);
}

static int ClampedTile(double at, uint last)
{
	return std::clamp(static_cast<int>(std::floor(at)), 0, static_cast<int>(last));
}

std::vector<WorldPoint> GroundPath(TilePoint from, TilePoint to)
{
	double span = std::hypot(to.first - from.first, to.second - from.second);
	int steps = std::max(1, static_cast<int>(std::ceil(span / GROUND_PATH_STEP)));
	std::vector<WorldPoint> path;
	path.reserve(steps + 1);
	std::optional<DrawnGround> ground;
	for (int step = 0; step <= steps; step++) {
		double share = static_cast<double>(step) / steps;
		double x = from.first + (to.first - from.first) * share;
		double y = from.second + (to.second - from.second) * share;
		int tx = ClampedTile(x, Map::MaxX());
		int ty = ClampedTile(y, Map::MaxY());
		if (!ground.has_value() || !ground->Holds(tx, ty)) ground.emplace(TileSpan{tx - GROUND_PATH_REACH, ty - GROUND_PATH_REACH, tx + GROUND_PATH_REACH, ty + GROUND_PATH_REACH});
		path.push_back(ground->Tile(tx, ty).At(x, y));
	}
	return path;
}

void FillGroundStroke(const DrawnTile &tile, TilePoint from, TilePoint to, double half_width, uint32_t c, uint alpha)
{
	auto [from_x, from_y] = from;
	auto [to_x, to_y] = to;
	MapVector along = {to_x - from_x, to_y - from_y};
	double length = Length(along);
	if (length == 0.0) return;

	VisibleStroke stroke = Widened(along, half_width, alpha);
	double side_x = along.y / length * stroke.half;
	double side_y = -along.x / length * stroke.half;
	int divisions = tile.Divisions();
	auto at = [&](int step, double side) {
		double share = static_cast<double>(step) / divisions;
		return tile.At(std::lerp(from_x, to_x, share) + side_x * side, std::lerp(from_y, to_y, share) + side_y * side);
	};
	for (int step = 0; step < divisions; step++) _canvas.FillWorldQuad({at(step, -1.0), at(step, 1.0), at(step + 1, 1.0), at(step + 1, -1.0)}, c, stroke.alpha);
}

void DrawTrackPiece(Track track, int tx, int ty, double half_width, uint32_t c, uint alpha)
{
	Trackdir td = TrackToTrackdir(track);
	FillGroundStroke(DrawnGround({tx, ty, tx, ty}).Tile(tx, ty), EdgeMidpoint(tx, ty, EntrySide(td)), EdgeMidpoint(tx, ty, TrackdirToExitdir(td)), half_width, c, alpha);
}

void DrawAxisBand(Axis axis, int tx, int ty, double half_width, uint32_t c, uint alpha)
{
	DrawTrackPiece(AxisToTrack(axis), tx, ty, half_width, c, alpha);
}

void DrawFacing(int tx, int ty, DiagDirection direction, double half_width, uint32_t c)
{
	FillGroundStroke(DrawnGround({tx, ty, tx, ty}).Tile(tx, ty), TileCentre(tx, ty), EdgeMidpoint(tx, ty, direction), half_width, c);
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
