/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file water_mesh.cpp The water's surface as triangles: level sheets over open water and shores, sloping ones down streams, and the sea beyond the edge; and the spray off its falls. */

#include "../../stdafx.h"
#include "water_mesh.h"

#include "../../map_func.h"
#include "../map/tile_shapes.h"
#include "sea_frame.h"
#include "seabed.h"
#include "tile_grid.h"
#include "water_sheet.h"

#include "../../safeguards.h"

static constexpr double SPRAY_RADIUS_PER_LEVEL = 0.11;
static constexpr int MOST_SPRAY_LEVELS = 3;
static constexpr double SPRAY_DRIFT_PACE = 0.15;

static uint32_t Add(WaterMesh &mesh, double x, double y, double level)
{
	return mesh.Add({static_cast<float>(x), static_cast<float>(y), static_cast<float>(level)});
}

static void AddSheet(WaterMesh &mesh, int tx, int ty)
{
	WaterSheet sheet(tx, ty);
	LayTileGrid(mesh, tx, ty, sheet.Divisions(), [&](int, int, double x, double y) { return Add(mesh, x, y, sheet.Level(x, y)); });
}

/* A river's bare banks carry its sheet on over their ground, so the water's curved outline may reach onto them instead of stopping short along the river's tile edge. */
static bool CarriesSheet(int tx, int ty)
{
	return WaterFormOf(tx, ty) != WaterForm::Dry || IsRiverBank(tx, ty);
}

/* Every tile lays its own sheet, so neighbours at one level share each point along their sides and leave no crack between them. */
WaterMesh BuildWaterSurface(const TileSpan &tiles)
{
	WaterMesh mesh;
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			if (CarriesSheet(tx, ty)) AddSheet(mesh, tx, ty);
		}
	}
	return mesh;
}

static bool FootOfFall(int tx, int ty)
{
	if (!IsRapids(tx, ty)) return false;
	TileIndexDiffC fall = FallStep(tx, ty);
	int below_x = tx + fall.x;
	int below_y = ty + fall.y;
	return WaterFormOf(below_x, below_y) != WaterForm::Dry && !IsRapids(below_x, below_y);
}

static int LevelsFallenTo(int tx, int ty, TileIndexDiffC fall)
{
	int levels = 1;
	while (levels < MOST_SPRAY_LEVELS && IsRapids(tx - fall.x * levels, ty - fall.y * levels)) levels++;
	return levels;
}

std::vector<SmokeVent> BuildFallSprays(const TileSpan &tiles)
{
	std::vector<SmokeVent> sprays;
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			if (!FootOfFall(tx, ty)) continue;
			MapVector down = Downhill(tx, ty);
			TileIndexDiffC fall = FallStep(tx, ty);
			WorldPoint foot = {tx + HALF_TILE * (1 + fall.x), ty + HALF_TILE * (1 + fall.y), SurfaceLevelOf(tx, ty)};
			Vec3 drift = Vec3{down.x, down.y, 0.0} * -SPRAY_DRIFT_PACE;
			sprays.push_back({RenderPoint(foot), SPRAY_RADIUS_PER_LEVEL * LevelsFallenTo(tx, ty, fall), drift, TileXY(tx, ty).base(), true});
		}
	}
	return sprays;
}

WaterMesh BuildOuterWater(Dimension map, double reach)
{
	SeaFrame frame(map, 0.0, reach);
	WaterMesh mesh;
	for (const MapVector &point : frame.points) Add(mesh, point.x, point.y, 0.0);
	for (const auto &[a, b, c] : frame.triangles) mesh.Triangle(a, b, c);
	return mesh;
}
