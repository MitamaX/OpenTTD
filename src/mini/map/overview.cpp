/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file overview.cpp A picture of the whole map, one pixel per sampled tile, with vehicles and cargo flow drawn over it. */

#include "../../stdafx.h"
#include "overview.h"

#include "../../company_base.h"
#include "../../company_func.h"
#include "../../industry.h"
#include "../../industrytype.h"
#include "../../linkgraph/linkgraph.h"
#include "../../map_func.h"
#include "../../mini_ui.h"
#include "../../settings_type.h"
#include "../../station_base.h"
#include "../../station_map.h"
#include "../../tile_map.h"
#include "../../tree_map.h"
#include "../../vehicle_base.h"
#include "../core/tones.h"
#include "ground.h"

#include "../../safeguards.h"

static constexpr int VEHICLE_DOT_RADIUS = 1;
static constexpr int FLOW_THICKNESS = 1;
static constexpr int FLOW_EXTRA_AT_FULL_LOAD = 2;

static uint32_t Faded(uint32_t c)
{
	return Mix(c, MINI_CH_PANEL, 168);
}

static uint32_t StationColour(TileIndex tile)
{
	switch (GetStationType(tile)) {
		case StationType::Rail:
		case StationType::RailWaypoint: return COL_ST_RAIL;
		case StationType::Airport: return COL_ST_AIR;
		case StationType::Truck:
		case StationType::Bus:
		case StationType::RoadWaypoint: return COL_ST_ROAD;
		case StationType::Dock: return COL_ST_DOCK;
		case StationType::Buoy: return COL_ST_BUOY;
		default: return COL_OBJ;
	}
}

static uint32_t BaseColour(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_VOID: return COL_VOID;
		case MP_WATER: return COL_WATER;
		case MP_TREES: return COL_TREE;
		case MP_HOUSE: return COL_HOUSE;
		case MP_INDUSTRY: return COL_IND;
		case MP_RAILWAY: return COL_RAIL;
		case MP_ROAD: return COL_ROAD;
		case MP_STATION: return StationColour(tile);
		case MP_TUNNELBRIDGE: return COL_BRIDGE;
		case MP_OBJECT: return COL_OBJ;
		default: return GroundColour(tile, TileHeight(tile));
	}
}

static uint32_t OwnerColour(Owner o)
{
	if (Company::IsValidID(o)) return _company_rgb[_company_colours[o]];
	if (o == OWNER_TOWN) return COL_ROAD;
	return COL_OBJ;
}

static uint32_t TileColour(TileIndex tile, OverviewMode mode)
{
	TileType tt = GetTileType(tile);
	if (tt == MP_VOID) return COL_VOID;

	switch (mode) {
		case OverviewMode::Industries:
			if (tt == MP_INDUSTRY) {
				const Industry *ind = Industry::GetByTile(tile);
				return PaletteRgb(GetIndustrySpec(ind->type)->map_colour);
			}
			return Faded(BaseColour(tile));

		case OverviewMode::Routes:
			switch (tt) {
				case MP_RAILWAY: return COL_PAPER;
				case MP_ROAD: return COL_CATENARY;
				case MP_STATION: return StationColour(tile);
				case MP_TUNNELBRIDGE: return COL_BRIDGE;
				default: return Faded(BaseColour(tile));
			}

		/* Which ground a tile carries decides what can be built on it, and in
		 * the tropical climate the zone decides it outright, so the zone wins
		 * over the ground colour there. */
		case OverviewMode::Vegetation:
			switch (tt) {
				case MP_WATER: return COL_WATER;
				case MP_TREES: return Mix(COL_TREE, COL_PAPER, 60 - std::min(GetTreeCount(tile), 4U) * 15);
				case MP_CLEAR:
					if (_settings_game.game_creation.landscape == LandscapeType::Tropic) {
						switch (GetTropicZone(tile)) {
							case TROPICZONE_DESERT: return COL_DESERT;
							case TROPICZONE_RAINFOREST: return Mix(COL_TREE, COL_PAPER, 40);
							default: break;
						}
					}
					return GroundColour(tile, TileHeight(tile));
				default: return Faded(BaseColour(tile));
			}

		case OverviewMode::Owner:
			switch (tt) {
				case MP_HOUSE: return COL_HOUSE;
				case MP_INDUSTRY: return COL_IND;
				case MP_CLEAR:
				case MP_TREES: return Faded(BaseColour(tile));
				case MP_WATER: {
					Owner o = GetTileOwner(tile);
					return Company::IsValidID(o) ? _company_rgb[_company_colours[o]] : COL_WATER;
				}
				default: return OwnerColour(GetTileOwner(tile));
			}

		case OverviewMode::Vehicles:
		case OverviewMode::Flow:
			return Faded(BaseColour(tile));

		default:
			return BaseColour(tile);
	}
}

static uint32_t CompanyColour(Owner owner)
{
	return Company::IsValidID(owner) ? _company_rgb[_company_colours[owner]] : COL_PAPER;
}

void Overview::Paint(int width, int height, OverviewMode mode)
{
	this->width = width;
	this->height = height;
	this->PaintTiles(mode);
	if (mode == OverviewMode::Vehicles) this->PaintVehicles();
	if (mode == OverviewMode::Flow) this->PaintFlow();
}

/* The map is drawn turned so its north corner points up-left: pixel columns follow the tile y axis, pixel rows the tile x axis. */
Point Overview::PixelOf(double tile_x, double tile_y) const
{
	return {static_cast<int>(tile_y * this->width / Map::SizeY()), static_cast<int>(tile_x * this->height / Map::SizeX())};
}

TileIndex Overview::TileAt(int x, int y) const
{
	int tile_x = Clamp(static_cast<int>(static_cast<int64_t>(y) * Map::SizeX() / std::max(1, this->height)), 0, static_cast<int>(Map::SizeX()) - 1);
	int tile_y = Clamp(static_cast<int>(static_cast<int64_t>(x) * Map::SizeY() / std::max(1, this->width)), 0, static_cast<int>(Map::SizeY()) - 1);
	return TileXY(tile_x, tile_y);
}

void Overview::PaintTiles(OverviewMode mode)
{
	this->pixels.assign(static_cast<size_t>(this->width) * this->height, COL_VOID);
	for (int y = 0; y < this->height; y++) {
		uint32_t *row = this->pixels.data() + static_cast<size_t>(y) * this->width;
		for (int x = 0; x < this->width; x++) row[x] = TileColour(this->TileAt(x, y), mode);
	}
}

void Overview::PaintVehicles()
{
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (!v->IsPrimaryVehicle() || v->vehstatus.Test(VehState::Hidden)) continue;
		this->Dot(this->PixelOf(v->x_pos / static_cast<double>(TILE_SIZE), v->y_pos / static_cast<double>(TILE_SIZE)), VEHICLE_DOT_RADIUS, CompanyColour(v->owner));
	}
}

/* One line per link the company's stations carry, thickened by how much of the link is in use, so a saturated leg reads at a glance. */
void Overview::PaintFlow()
{
	for (const LinkGraph *lg : LinkGraph::Iterate()) {
		if (!IsValidCargoType(lg->Cargo())) continue;
		uint32_t colour = CargoRgb(lg->Cargo());
		for (NodeID i = 0; i < lg->Size(); i++) {
			const LinkGraph::BaseNode &node = (*lg)[i];
			if (!Station::IsValidID(node.station) || Station::Get(node.station)->owner != _local_company || node.xy == INVALID_TILE) continue;
			for (const LinkGraph::BaseEdge &edge : node.edges) {
				if (edge.capacity == 0 || edge.dest_node >= lg->Size()) continue;
				const LinkGraph::BaseNode &dest = (*lg)[edge.dest_node];
				if (dest.xy == INVALID_TILE) continue;
				int load = FLOW_EXTRA_AT_FULL_LOAD * std::min(edge.usage, edge.capacity) / edge.capacity;
				this->Line(this->PixelOf(TileX(node.xy), TileY(node.xy)), this->PixelOf(TileX(dest.xy), TileY(dest.xy)), FLOW_THICKNESS + load, colour);
			}
		}
	}
}

void Overview::Dot(Point at, int radius, uint32_t colour)
{
	for (int y = std::max(0, at.y - radius); y <= std::min(this->height - 1, at.y + radius); y++) {
		for (int x = std::max(0, at.x - radius); x <= std::min(this->width - 1, at.x + radius); x++) {
			this->pixels[static_cast<size_t>(y) * this->width + x] = colour;
		}
	}
}

void Overview::Line(Point from, Point to, int thickness, uint32_t colour)
{
	int steps = std::max({std::abs(to.x - from.x), std::abs(to.y - from.y), 1});
	for (int i = 0; i <= steps; i++) {
		Point at{from.x + (to.x - from.x) * i / steps, from.y + (to.y - from.y) * i / steps};
		this->Dot(at, thickness / 2, colour);
	}
}
