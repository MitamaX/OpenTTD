/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tool_sites.cpp Spots near the cursor where the tool in hand would take. */

#include "../../stdafx.h"
#include "tool_sites.h"

#include "../../company_base.h"
#include "../../company_func.h"
#include "../../gfx_func.h"
#include "../../map_func.h"
#include "../../slope_func.h"
#include "../../tile_map.h"
#include "build_tool.h"
#include "command_probe.h"
#include "tile_pick.h"
#include "tool_choices.h"
#include "tool_commit.h"

#include "../../safeguards.h"

static constexpr int MINI_SITE_RADIUS = 10;
/* Open water passes the shape test everywhere, so the sweep stops asking once
 * it has offered this many spots. */
static constexpr size_t MINI_SITE_MAX = 200;

ToolSites _sites;

static bool NeedsSite(MiniTool kind)
{
	switch (kind) {
		case MiniTool::RailTunnel:
		case MiniTool::RoadTunnel:
		case MiniTool::Dock:
		case MiniTool::Lock:
		case MiniTool::Buoy:
		case MiniTool::ShipDepot:
			return true;
		default:
			return false;
	}
}

/* A shape test comes first so the command is only asked where the ground could
 * plausibly carry the build; the sweep would be far too many commands
 * otherwise. */
static bool SiteShapeFits(MiniTool kind, TileIndex tile)
{
	switch (kind) {
		case MiniTool::RailTunnel:
		case MiniTool::RoadTunnel:
		case MiniTool::Dock:
		case MiniTool::Lock:
			return GetInclinedSlopeDirection(GetTileSlope(tile)) != INVALID_DIAGDIR;
		case MiniTool::Buoy:
		case MiniTool::ShipDepot:
			return IsTileType(tile, MP_WATER);
		default:
			return false;
	}
}

void ToolSites::Update()
{
	MiniTool kind = _tool.Kind();
	if (!NeedsSite(kind) || !_cursor.in_window || _ctrl_pressed || !Company::IsValidID(_local_company)) {
		this->tiles.clear();
		this->key = 0;
		return;
	}

	TileIndex centre = SiteTileAt(CursorPoint());
	uint64_t key = ProbeKey().Add(static_cast<uint64_t>(kind)).Add(centre.base()).Add(_choices.Key()).Value();
	if (key == this->key) return;
	this->key = key;
	this->Sweep(kind, centre);
}

void ToolSites::Sweep(MiniTool kind, TileIndex centre)
{
	this->tiles.clear();

	int cx = TileX(centre);
	int cy = TileY(centre);
	int x_lo = std::max(1, cx - MINI_SITE_RADIUS);
	int x_hi = std::min<int>(Map::SizeX() - 2, cx + MINI_SITE_RADIUS);
	int y_lo = std::max(1, cy - MINI_SITE_RADIUS);
	int y_hi = std::min<int>(Map::SizeY() - 2, cy + MINI_SITE_RADIUS);

	CommandProbe probe;
	for (int tx = x_lo; tx <= x_hi && this->tiles.size() < MINI_SITE_MAX; tx++) {
		for (int ty = y_lo; ty <= y_hi && this->tiles.size() < MINI_SITE_MAX; ty++) {
			TileIndex tile = TileXY(tx, ty);
			if (!SiteShapeFits(kind, tile)) continue;
			/* The tool's own placement runs on the candidate, so the sweep
			 * follows whatever the tool would actually put down. */
			CommitClick(kind, tile, false);
			if (probe.TakeVerdict()) this->tiles.push_back(tile);
		}
	}
}
