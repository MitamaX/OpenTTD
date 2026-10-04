/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tool_sites.h Spots near the cursor where the tool in hand would take. */

#ifndef MINI_TOOLS_TOOL_SITES_H
#define MINI_TOOLS_TOOL_SITES_H

#include <span>
#include <vector>

#include "../../tile_type.h"
#include "tool_kind.h"

/* A tunnel mouth, a dock, a lock and a ship depot each want one shape of
 * ground, and a blueprint that only answers for the tile under the cursor
 * leaves the player clicking around to find it. The ground near the cursor is
 * swept instead and every spot that would take the build is ghosted. */
class ToolSites {
public:
	void Update();
	std::span<const TileIndex> Tiles() const { return this->tiles; }

private:
	void Sweep(MiniTool kind, TileIndex centre);

	std::vector<TileIndex> tiles;
	uint64_t key = 0;
};

extern ToolSites _sites;

#endif /* MINI_TOOLS_TOOL_SITES_H */
