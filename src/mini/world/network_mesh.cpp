/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file network_mesh.cpp Everything the ways on a block of tiles lay down: a mesh for each overlay layer, and the signals standing beside the track. */

#include "../../stdafx.h"
#include "network_mesh.h"

#include <cmath>
#include <optional>

#include "../../bridge_map.h"
#include "../../elrail_func.h"
#include "../../map_func.h"
#include "../../rail_map.h"
#include "../../road_map.h"
#include "../../settings_type.h"
#include "../../station_map.h"
#include "../../tile_map.h"
#include "../../track_func.h"
#include "../../tunnelbridge_map.h"
#include "../map/tile_shapes.h"
#include "../map/way_profile.h"
#include "../map/world_tiles.h"
#include "bridge_models.h"
#include "road_models.h"
#include "station_models.h"
#include "track_models.h"
#include "tunnel_models.h"

#include "../../safeguards.h"

/* The game stands a signal about a quarter tile in from the edge its trackdir enters by, clear of the ballast. */
static constexpr double SIGNAL_ALONG = 0.25;
static constexpr double SIGNAL_LATERAL = 0.34;
static constexpr uint8_t SIGNAL_SIDE_LEFT = 0;
static constexpr uint8_t SIGNAL_SIDE_RIGHT = 2;
static constexpr uint8_t ROAD_SIDE_LEFT = 0;

/* Signals stand right of the track, or left where the game is set to or where traffic keeps left. */
static double SignalSide()
{
	switch (_settings_game.construction.train_signal_side) {
		case SIGNAL_SIDE_LEFT: return -1.0;
		case SIGNAL_SIDE_RIGHT: return 1.0;
		default: return _settings_game.vehicle.road_side == ROAD_SIDE_LEFT ? -1.0 : 1.0;
	}
}

/* A country road eases along its course; any other keeps to its tile's ground. */
static Footing RoadFooting(int tx, int ty)
{
	std::optional<WayCourse> course = WayCourse::OfRoad(tx, ty);
	if (!course.has_value()) return GroundFooting(tx, ty);
	return [course = *course](double x, double y) { return course.Level(course.ShareAt({x, y})); };
}

static bool Wired(TileIndex tile)
{
	return HasRailCatenaryDrawn(GetRailType(tile));
}

/* One block's builder: it walks the block's tiles and lays each one's ways into the layer it belongs to. */
class NetworkBuilder {
public:
	explicit NetworkBuilder(WayDetail detail) : detail(detail) {}

	void Build(int tx, int ty)
	{
		TileIndex tile = TileXY(tx, ty);
		if (IsBridgeAbove(tile)) this->BridgeSpan(BridgeSite::Above(tile), tx, ty);
		switch (GetTileType(tile)) {
			case MP_RAILWAY: this->Railway(tile, tx, ty); break;
			case MP_ROAD: this->Road(tile, tx, ty); break;
			case MP_STATION: this->Station(tile, tx, ty); break;
			case MP_TUNNELBRIDGE: this->TunnelOrBridge(tile, tx, ty); break;
			default: break;
		}
	}

	NetworkMeshes Finish() { return std::move(this->meshes); }

private:
	/* The world's texel names the track pieces a tile lays, a crossing's and a station's among them. */
	TrackSite TrackOf(TileIndex tile, int tx, int ty) const
	{
		NetworkTexel texel = _world_tiles.NetworkAt(tile);
		return {tx, ty, static_cast<TrackBits>(texel.track), static_cast<RailLook>(texel.style & NETWORK_RAIL_LOOK_MASK), Wired(tile), true, this->detail};
	}

	void LayTrackOn(const TrackSite &site, const Footing &footing)
	{
		ModelMesh &rail = this->meshes.Layer(MiniLayer::Rail);
		LayTrack(rail, site, footing);
		LayCatenary(rail, site, footing);
	}

	RoadSite RoadOf(TileIndex tile, int tx, int ty, bool crossing) const
	{
		NetworkTexel texel = _world_tiles.NetworkAt(tile);
		DisallowedRoadDirections one_way = static_cast<DisallowedRoadDirections>(texel.style >> NETWORK_ONE_WAY_SHIFT & DRD_BOTH);
		bool kerbed = (texel.style & NETWORK_KERB_BIT) != 0;
		return {tx, ty, static_cast<RoadBits>(texel.road), static_cast<RoadBits>(texel.tram), kerbed, crossing, one_way, this->detail};
	}

	void Railway(TileIndex tile, int tx, int ty)
	{
		this->LayTrackOn(this->TrackOf(tile, tx, ty), GroundFooting(tx, ty));
		if (IsPlainRailTile(tile) && HasSignals(tile)) this->Signals(tile, tx, ty);
	}

	void Road(TileIndex tile, int tx, int ty)
	{
		Footing footing = RoadFooting(tx, ty);
		bool crossing = IsLevelCrossingTile(tile);
		LayRoad(this->meshes.Layer(MiniLayer::Road), this->RoadOf(tile, tx, ty, crossing), footing);
		if (!crossing) return;
		TrackSite site = this->TrackOf(tile, tx, ty);
		LayCrossingRails(this->meshes.Layer(MiniLayer::Rail), site, footing);
		LayCatenary(this->meshes.Layer(MiniLayer::Rail), site, footing);
	}

	void Station(TileIndex tile, int tx, int ty)
	{
		if (HasStationRail(tile)) {
			this->LayTrackOn(this->TrackOf(tile, tx, ty), GroundFooting(tx, ty));
			LayRailStop(this->meshes.Layer(MiniLayer::Rail), tile, this->detail);
		} else if (IsAnyRoadStop(tile)) {
			ModelMesh &road = this->meshes.Layer(MiniLayer::Road);
			RoadSite site = this->RoadOf(tile, tx, ty, false);
			site.kerbed = IsDriveThroughStopTile(tile);
			LayRoad(road, site, GroundFooting(tx, ty));
			if (IsDriveThroughStopTile(tile) && IsBusStop(tile)) LayBusShelter(road, tile, this->detail);
		}
	}

	void TunnelOrBridge(TileIndex tile, int tx, int ty)
	{
		if (IsBridge(tile)) {
			BridgeSite bridge = BridgeSite::OfHead(tile);
			LayBridgeRamp(this->Layer(bridge.transport), bridge, tile);
			this->BridgeWay(bridge, tx, ty, RampFooting(bridge, tile));
			return;
		}
		TransportType transport = GetTunnelBridgeTransportType(tile);
		if (transport == TRANSPORT_RAIL) {
			this->LayTrackOn(this->TrackOf(tile, tx, ty), GroundFooting(tx, ty));
		} else {
			LayRoad(this->meshes.Layer(MiniLayer::Road), this->RoadOf(tile, tx, ty, false), GroundFooting(tx, ty));
		}
		LayTunnelPortal(this->Layer(transport), tile);
	}

	ModelMesh &Layer(TransportType transport)
	{
		switch (transport) {
			case TRANSPORT_RAIL: return this->meshes.Layer(MiniLayer::Rail);
			case TRANSPORT_ROAD: return this->meshes.Layer(MiniLayer::Road);
			default: return this->meshes.Layer(MiniLayer::None);
		}
	}

	void BridgeSpan(const BridgeSite &bridge, int tx, int ty)
	{
		LayBridgeSpan(this->Layer(bridge.transport), bridge, tx, ty);
		this->BridgeWay(bridge, tx, ty, DeckFooting(bridge));
	}

	/* A bridge carries its head's way straight along its axis, from edge to edge of every tile. */
	void BridgeWay(const BridgeSite &bridge, int tx, int ty, const Footing &footing)
	{
		TileIndex head = bridge.north;
		if (bridge.transport == TRANSPORT_RAIL) {
			TrackSite site = {tx, ty, AxisToTrackBits(bridge.axis), RailLookOf(GetRailType(head)), Wired(head), false, this->detail};
			this->LayTrackOn(site, footing);
		} else if (bridge.transport == TRANSPORT_ROAD) {
			RoadBits bits = AxisToRoadBits(bridge.axis);
			RoadBits road = HasTileRoadType(head, RTT_ROAD) ? bits : ROAD_NONE;
			RoadBits tram = HasTileRoadType(head, RTT_TRAM) ? bits : ROAD_NONE;
			LayRoad(this->meshes.Layer(MiniLayer::Road), {tx, ty, road, tram, false, false, DRD_NONE, this->detail}, footing);
		}
	}

	void Signals(TileIndex tile, int tx, int ty)
	{
		double lateral = SignalSide() * SIGNAL_LATERAL;
		double rise = LevelRise();
		for (Track track : SetTrackBitIterator(GetTrackBits(tile))) {
			for (Trackdir trackdir : {TrackToTrackdir(track), ReverseTrackdir(TrackToTrackdir(track))}) {
				if (!HasSignalOnTrackdir(tile, trackdir)) continue;
				WorldPoint foot = TrackdirGroundPoint(tx, ty, trackdir, SIGNAL_ALONG, lateral);
				WorldPoint entry = TrackdirGroundPoint(tx, ty, trackdir, 0.0, 0.0);
				WorldPoint exit = TrackdirGroundPoint(tx, ty, trackdir, 1.0, 0.0);
				double facing = std::atan2(entry.y - exit.y, entry.x - exit.x);
				this->meshes.signals.push_back({{foot.x, foot.y, WayLevel(foot.x, foot.y) * rise}, facing, tile, trackdir});
			}
		}
	}

	WayDetail detail;
	NetworkMeshes meshes;
};

NetworkMeshes BuildNetwork(const TileSpan &tiles, WayDetail detail)
{
	NetworkBuilder builder(detail);
	for (int ty = tiles.ty0; ty <= tiles.ty1; ty++) {
		for (int tx = tiles.tx0; tx <= tiles.tx1; tx++) builder.Build(tx, ty);
	}
	return builder.Finish();
}
