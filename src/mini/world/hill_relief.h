/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file hill_relief.h The ground of the land as its meshes and everything standing on it read it. */

#ifndef MINI_WORLD_HILL_RELIEF_H
#define MINI_WORLD_HILL_RELIEF_H

#include "../core/camera.h"

class Landform {
public:
	explicit Landform(const TileSpan &tiles);

	double LevelAt(double x, double y) const;
	WorldPoint At(const MapVector &at) const { return {at.x, at.y, this->LevelAt(at.x, at.y)}; }
	/* How the ground climbs along map x and map y at a point, in levels per tile. */
	MapVector Climb(double x, double y) const;

private:
	TileSpan tiles;
};

class LaidGround {
public:
	LaidGround(const Landform &land, int tx, int ty);

	double Level(double x, double y) const;
	double Level(const MapVector &at) const { return this->Level(at.x, at.y); }
	WorldPoint At(const MapVector &at) const { return {at.x, at.y, this->Level(at)}; }

private:
	const Landform &land;
	int tx;
	int ty;
};

#endif /* MINI_WORLD_HILL_RELIEF_H */
