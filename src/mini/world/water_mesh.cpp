/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file water_mesh.cpp The water's surface as triangles: level sheets over open water and shores, sloping ones down streams, and the sea beyond the edge. */

#include "../../stdafx.h"
#include "water_mesh.h"

#include <optional>

#include "../../map_func.h"
#include "../map/world_tiles.h"
#include "seabed.h"

#include "../../safeguards.h"

/* A run of tiles along map X whose water stands at one level. */
struct LevelRun {
	int x0;
	int x1;
	double level;
};

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

WaterMesh BuildWaterSurface(const TileSpan &tiles)
{
	WaterMesh mesh;
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		std::optional<LevelRun> run;
		auto finish = [&]() {
			if (run.has_value()) AddSheet(mesh, run->x0, ty, run->x1 + 1, ty + 1, run->level);
			run.reset();
		};
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) {
			if (!CarriesSheet(tx, ty)) {
				finish();
				if (WaterFormOf(tx, ty) == WaterForm::Incline) AddIncline(mesh, tx, ty);
				continue;
			}
			double level = SurfaceLevelOf(tx, ty);
			if (run.has_value() && run->level == level) {
				run->x1 = tx;
				continue;
			}
			finish();
			run = LevelRun{tx, tx, level};
		}
		finish();
	}
	return mesh;
}

/* Four sheets around the map at sea level, meeting its edge where the border tiles lie. */
WaterMesh BuildOuterWater(Dimension map, double reach)
{
	double width = map.width;
	double height = map.height;
	WaterMesh mesh;
	AddSheet(mesh, -reach, -reach, width + reach, 0.0, 0.0);
	AddSheet(mesh, -reach, height, width + reach, height + reach, 0.0);
	AddSheet(mesh, -reach, 0.0, 0.0, height, 0.0);
	AddSheet(mesh, width, 0.0, width + reach, height, 0.0);
	return mesh;
}
