/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file bridge_models.h Bridges in 3D: decks, ramps and abutments, piers down to the ground or the bed of the water, and the girders, trusses, cables and arches of each kind. */

#ifndef MINI_WORLD_BRIDGE_MODELS_H
#define MINI_WORLD_BRIDGE_MODELS_H

#include "../../bridge_type.h"
#include "../../direction_type.h"
#include "../../tile_type.h"
#include "../../transport_type.h"
#include "way_shapes.h"

/* A bridge between its two heads, the northern one at the lower map coordinate, and the level its deck carries its way at. */
struct BridgeSite {
	TileIndex north;
	TileIndex south;
	Axis axis;
	int deck;
	BridgeType type;
	TransportType transport;

	static BridgeSite OfHead(TileIndex head);
	static BridgeSite Above(TileIndex tile);
};

/* What a bridge's way is laid on: the deck over its middle tiles, and on a head the ramp climbing from the ground at its outer edge to the deck. */
Footing DeckFooting(const BridgeSite &bridge);
Footing RampFooting(const BridgeSite &bridge, TileIndex head);

void LayBridgeSpan(ModelMesh &mesh, const BridgeSite &bridge, int tx, int ty);
void LayBridgeRamp(ModelMesh &mesh, const BridgeSite &bridge, TileIndex head);

#endif /* MINI_WORLD_BRIDGE_MODELS_H */
