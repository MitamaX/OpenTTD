/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file water_mesh.cpp The water's surface as triangles: level sheets over open water and shores, sloping ones down streams, and the sea beyond the edge. */

#include "../../stdafx.h"
#include "water_mesh.h"

#include "sea_frame.h"
#include "seabed.h"
#include "tile_grid.h"
#include "water_sheet.h"

#include "../../safeguards.h"

static uint32_t Add(WaterMesh &mesh, double x, double y, double level)
{
	return mesh.Add({static_cast<float>(x), static_cast<float>(y), static_cast<float>(level)});
}

static void AddSheet(WaterMesh &mesh, int tx, int ty)
{
	WaterSheet sheet(tx, ty);
	LayTileGrid(mesh, tx, ty, sheet.Divisions(), [&](int, int, double x, double y) { return Add(mesh, x, y, sheet.Level(x, y)); });
}

/* A river's bare banks carry its sheet on over their low ground, so the water's curved outline may reach onto them instead of stopping short along the river's tile edge. */
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

WaterMesh BuildOuterWater(Dimension map, double reach)
{
	SeaFrame frame(map, 0.0, reach);
	WaterMesh mesh;
	for (const MapVector &point : frame.points) Add(mesh, point.x, point.y, 0.0);
	for (const auto &[a, b, c] : frame.triangles) mesh.Triangle(a, b, c);
	return mesh;
}
