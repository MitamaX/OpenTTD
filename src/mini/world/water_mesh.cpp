/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file water_mesh.cpp The water's surface as triangles: level sheets over open water and shores, sloping ones down streams, and the sea beyond the edge. */

#include "../../stdafx.h"
#include "water_mesh.h"

#include "../../map_func.h"
#include "../map/world_tiles.h"
#include "sea_frame.h"
#include "seabed.h"

#include "../../safeguards.h"

static uint32_t Add(WaterMesh &mesh, double x, double y, double level)
{
	return mesh.Add({static_cast<float>(x), static_cast<float>(y), static_cast<float>(level)});
}

static void AddSheet(WaterMesh &mesh, double x0, double y0, double x1, double y1, double level)
{
	mesh.Quad(Add(mesh, x0, y0, level), Add(mesh, x1, y0, level), Add(mesh, x1, y1, level), Add(mesh, x0, y1, level));
}

/* Water running down a slope lies on the slope's own plane. */
static void AddIncline(WaterMesh &mesh, int tx, int ty)
{
	SurfaceTexel surface = _world_tiles.SurfaceAt(TileXY(tx, ty));
	mesh.Quad(Add(mesh, tx, ty, surface.north), Add(mesh, tx + 1, ty, surface.west), Add(mesh, tx + 1, ty + 1, surface.south), Add(mesh, tx, ty + 1, surface.east));
}

/* A river's bare banks carry its sheet on over their low ground, so the water's curved outline may reach onto them instead of stopping short along the river's tile edge. */
static bool CarriesSheet(int tx, int ty)
{
	switch (WaterFormOf(tx, ty)) {
		case WaterForm::Open:
		case WaterForm::Shore: return true;
		case WaterForm::Dry: return IsRiverBank(tx, ty);
		default: return false;
	}
}

/* Every tile lays its own sheet, so neighbours at one level share each point along their sides and leave no crack between them. */
WaterMesh BuildWaterSurface(const TileSpan &tiles)
{
	WaterMesh mesh;
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			if (CarriesSheet(tx, ty)) {
				AddSheet(mesh, tx, ty, tx + 1, ty + 1, SurfaceLevelOf(tx, ty));
			} else if (WaterFormOf(tx, ty) == WaterForm::Incline) {
				AddIncline(mesh, tx, ty);
			}
		}
	}
	return mesh;
}

WaterMesh BuildOuterWater(Dimension map, double reach)
{
	SeaFrame frame(map, 0.0, reach);
	WaterMesh mesh;
	for (const MapVector &point : frame.points) Add(mesh, point.x, point.y, 0.0);
	for (const auto &[a, b, c] : frame.triangles) mesh.Triangle(a, b, c);
	return mesh;
}
