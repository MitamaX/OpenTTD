/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sea_frame.h The flat open sea around the map out to the horizon, laid so the meshes inside meet it without cracks. */

#ifndef MINI_WORLD_SEA_FRAME_H
#define MINI_WORLD_SEA_FRAME_H

#include <array>
#include <vector>

#include "../../core/geometry_type.hpp"
#include "../core/camera.h"

/* A flat frame from a margin about the map out to a reach beyond it. Its inner rim has a point at every whole tile along the map's sides and at the margin's corners,
 * so whatever meets it there shares its every point and leaves no crack for the sky to show through. */
struct SeaFrame {
	std::vector<MapVector> points;
	std::vector<std::array<uint32_t, 3>> triangles;

	SeaFrame(Dimension map, double margin, double reach);
};

#endif /* MINI_WORLD_SEA_FRAME_H */
