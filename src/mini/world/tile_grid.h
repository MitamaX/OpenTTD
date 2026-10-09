/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tile_grid.h A tile laid as a grid of quads, its points asked for one by one. */

#ifndef MINI_WORLD_TILE_GRID_H
#define MINI_WORLD_TILE_GRID_H

#include <cstdint>
#include <vector>

template <typename Mesh, typename Vertex>
void LayTileGrid(Mesh &mesh, int tx, int ty, int divisions, Vertex vertex)
{
	int side = divisions + 1;
	std::vector<uint32_t> grid(static_cast<size_t>(side) * side);
	for (int j = 0; j < side; j++) {
		for (int i = 0; i < side; i++) grid[j * side + i] = vertex(i, j, tx + static_cast<double>(i) / divisions, ty + static_cast<double>(j) / divisions);
	}
	for (int j = 0; j < divisions; j++) {
		for (int i = 0; i < divisions; i++) mesh.Quad(grid[j * side + i], grid[j * side + i + 1], grid[(j + 1) * side + i + 1], grid[(j + 1) * side + i]);
	}
}

#endif /* MINI_WORLD_TILE_GRID_H */
