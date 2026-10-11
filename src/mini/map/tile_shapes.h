/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tile_shapes.h Shapes laid on the world: tiles, areas, strokes and runs, and the points they are built from. */

#ifndef MINI_MAP_TILE_SHAPES_H
#define MINI_MAP_TILE_SHAPES_H

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "../../core/geometry_type.hpp"
#include "../../direction_type.h"
#include "../../tile_type.h"
#include "../../track_type.h"
#include "../core/camera.h"
#include "../core/canvas.h"
#include "../core/ground_trace.h"
#include "world_tiles.h"

inline constexpr double HALF_TILE = 0.5;

/* A tile's own surface, read for every point that belongs to the tile, so a point on its edge stays on it and not on the neighbour across. */
class TileGround final : public DrawnSurface {
public:
	TileGround(int tx, int ty);
	explicit TileGround(TileIndex tile);

	double Level(double x, double y) const override;
	bool Curves() const override { return false; }
	double Lowest() const;
	WorldPoint At(double x, double y) const { return {x, y, this->Level(x, y)}; }
	WorldPoint At(TilePoint at) const { return this->At(at.first, at.second); }

private:
	int tx;
	int ty;
	SurfaceTexel surface;
};

class DrawnTile {
public:
	DrawnTile(std::unique_ptr<DrawnSurface> surface, int tx, int ty);

	bool Is(int x, int y) const { return this->tx == x && this->ty == y; }
	WorldPoint At(double x, double y) const { return {x, y, this->surface->Level(x, y)}; }
	WorldPoint At(TilePoint at) const { return this->At(at.first, at.second); }
	int Divisions() const;

private:
	std::unique_ptr<DrawnSurface> surface;
	int tx;
	int ty;
};

class DrawnGround {
public:
	explicit DrawnGround(const TileSpan &tiles);

	bool Holds(int tx, int ty) const;
	DrawnTile Tile(int tx, int ty) const { return DrawnTile(this->span->Surface(tx, ty), tx, ty); }

private:
	TileSpan tiles;
	std::unique_ptr<GroundSpan> span;
};

/* The upright face where a tile's own edge stands above its neighbour's, its corners going round counterclockwise as seen from outside. */
struct StepFace {
	MapVector outward;
	std::array<WorldPoint, 4> ring;
};

bool OnMap(int tx, int ty);
/* The map's tiles within so many tiles of an area. */
Rect TilesNear(const Rect &area, int reach);
/* The area grown just enough to take in a tile. */
Rect TakingIn(const Rect &area, int tx, int ty);
MapVector Outward(DiagDirection side);

template <typename Test>
bool AnySideTile(int tx, int ty, Test test)
{
	for (DiagDirection side = DIAGDIR_BEGIN; side < DIAGDIR_END; side++) {
		MapVector out = Outward(side);
		if (test(tx + static_cast<int>(out.x), ty + static_cast<int>(out.y))) return true;
	}
	return false;
}

std::optional<StepFace> StepFaceOf(int tx, int ty, DiagDirection side);

WorldPoint Between(const WorldPoint &from, const WorldPoint &to, double share);

/* Where an arc length along a path falls: between two of its points, a share of the way from the first, carried on past the last along the last stretch. */
struct ArcPlace {
	size_t from;
	size_t to;
	double share;
};

template <typename Point, typename Flatten = std::identity>
ArcPlace PlaceAlong(std::span<const Point> path, double arc, Flatten flat = {})
{
	for (size_t point = 1; point < path.size(); point++) {
		double stretch = Length(flat(path[point]) - flat(path[point - 1]));
		if (arc <= stretch || point + 1 == path.size()) return {point - 1, point, stretch > 0.0 ? arc / stretch : 0.0};
		arc -= stretch;
	}
	return {0, 0, 0.0};
}

TilePoint TileCentre(int tx, int ty);
TilePoint Shifted(TilePoint from, DiagDirection side, double tiles);

void FillTile(int tx, int ty, uint32_t c, uint alpha = Canvas::OPAQUE_ALPHA);
/* Tile by tile so the fill follows the terrain, over the tiles that can show. */
void FillArea(int tx0, int ty0, int tx1, int ty1, uint32_t c, uint alpha = Canvas::OPAQUE_ALPHA);
void FrameArea(int tx0, int ty0, int tx1, int ty1, int width, uint32_t c, uint alpha = Canvas::OPAQUE_ALPHA);

/* Points along the ground on the straight way between two map points, close enough together to follow its rises and dips. */
std::vector<WorldPoint> GroundPath(TilePoint from, TilePoint to);

/* A band of a tile's ground between two of its points, kept a pixel across like FillAxisLine. */
void FillGroundStroke(const DrawnTile &tile, TilePoint from, TilePoint to, double half_width, uint32_t c, uint alpha = Canvas::OPAQUE_ALPHA);
void DrawTrackPiece(Track track, int tx, int ty, double half_width, uint32_t c, uint alpha = Canvas::OPAQUE_ALPHA);
void DrawAxisBand(Axis axis, int tx, int ty, double half_width, uint32_t c, uint alpha = Canvas::OPAQUE_ALPHA);
void DrawFacing(int tx, int ty, DiagDirection direction, double half_width, uint32_t c);

/* Along runs from where the trackdir enters its piece to where it leaves; lateral offsets point to the right of travel. */
WorldPoint TrackdirGroundPoint(int tx, int ty, Trackdir td, double along, double lateral);
Point ScreenOfTileCentre(int tx, int ty);

#endif /* MINI_MAP_TILE_SHAPES_H */
