/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file chunk_grid.cpp The map cut into square blocks of tiles, numbered row by row, and which blocks a change of the world reaches. */

#include "../../stdafx.h"
#include "chunk_grid.h"

#include "../../core/math_func.hpp"

#include "../../safeguards.h"

void ChunkGrid::Lay(Dimension map)
{
	this->map = map;
	this->columns = static_cast<int>(CeilDiv(map.width, this->chunk_tiles));
	this->rows = static_cast<int>(CeilDiv(map.height, this->chunk_tiles));
}

TileSpan ChunkGrid::TilesOf(size_t index) const
{
	int tx = static_cast<int>(index % this->columns) * this->chunk_tiles;
	int ty = static_cast<int>(index / this->columns) * this->chunk_tiles;
	return {tx, ty, std::min(tx + this->chunk_tiles, static_cast<int>(this->map.width)) - 1, std::min(ty + this->chunk_tiles, static_cast<int>(this->map.height)) - 1};
}
