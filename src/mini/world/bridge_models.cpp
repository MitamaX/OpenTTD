/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file bridge_models.cpp Bridges in 3D: decks, ramps and abutments, piers down to the ground or the bed of the water, and the girders, trusses, cables and arches of each kind. */

#include "../../stdafx.h"
#include "bridge_models.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#include "../../bridge.h"
#include "../../bridge_map.h"
#include "../../direction_func.h"
#include "../../map_func.h"
#include "../../tile_map.h"
#include "../../tunnelbridge_map.h"
#include "../map/tile_shapes.h"
#include "../map/world_tiles.h"
#include "seabed.h"

#include "../../safeguards.h"

/** The build each of the game's bridge kinds is shown with. */
enum class BridgeBuild : uint8_t {
	Trestle,
	Beam,
	Girder,
	Suspension,
	Truss,
	Arch,
};

struct BridgeLook {
	BridgeBuild build;
	uint32_t frame;
	uint32_t deck;
};

static constexpr uint32_t TIMBER = 0x7A5B3E;
static constexpr uint32_t CONCRETE = 0xACA79C;
static constexpr uint32_t STONE = 0x9E978A;
static constexpr uint32_t STEEL_GREY = 0x6E7A86;
static constexpr uint32_t WATER = 0x3F7393;

/* In the order of the game's bridge table. */
static constexpr std::array<BridgeLook, MAX_BRIDGES> BRIDGE_LOOKS = {{
	{BridgeBuild::Trestle, TIMBER, 0x6A4E36},
	{BridgeBuild::Beam, 0xA8574A, CONCRETE},
	{BridgeBuild::Girder, STEEL_GREY, CONCRETE},
	{BridgeBuild::Suspension, 0xB5AFA4, CONCRETE},
	{BridgeBuild::Suspension, 0x7E8790, CONCRETE},
	{BridgeBuild::Suspension, 0xD8B13A, CONCRETE},
	{BridgeBuild::Truss, 0x6D7680, CONCRETE},
	{BridgeBuild::Truss, 0x7A5638, CONCRETE},
	{BridgeBuild::Truss, 0xA43C32, CONCRETE},
	{BridgeBuild::Girder, 0x5E6A76, CONCRETE},
	{BridgeBuild::Arch, 0xBEC3C9, CONCRETE},
	{BridgeBuild::Arch, 0xD9B43C, CONCRETE},
	{BridgeBuild::Arch, 0xCDD3DA, CONCRETE},
}};
static constexpr BridgeLook AQUEDUCT_LOOK = {BridgeBuild::Beam, STONE, STONE};

static constexpr double DECK_HALF = 0.36;
static constexpr double DECK_DEPTH = 0.07;
static constexpr double PARAPET_HEIGHT = 0.07;
static constexpr double PARAPET_WIDTH = 0.03;
static constexpr double BOX_HALF_TOP = 0.24;
static constexpr double BOX_HALF_FOOT = 0.18;
static constexpr double BOX_DEPTH = 0.2;
static constexpr double GIRDER_HEIGHT = 0.17;
static constexpr double STIFFENER_PITCH = 0.25;
static constexpr double STIFFENER_HALF = 0.008;
static constexpr double TRUSS_HEIGHT = 0.32;
static constexpr double TRUSS_LATERAL = 0.345;
static constexpr double MEMBER_HALF = 0.012;
static constexpr double THIN_HALF = 0.004;
static constexpr double TOWER_LATERAL = 0.43;
static constexpr double TOWER_HALF = 0.03;
static constexpr double TOWER_SHARE = 1.0 / 6.0;
static constexpr double TOWER_LEAST = 0.9;
static constexpr double TOWER_PER_TILE = 0.06;
static constexpr double TOWER_MOST = 1.8;
static constexpr double CABLE_LATERAL = 0.4;
static constexpr double CABLE_BELOW_TOP = 0.06;
static constexpr double CABLE_SAG = 0.1;
static constexpr double ARCH_LATERAL = 0.37;
static constexpr double ARCH_RISE_BASE = 0.2;
static constexpr double ARCH_RISE_PER_TILE = 0.08;
static constexpr double ARCH_RISE_MOST = 1.1;
static constexpr double ARCH_BRACED = 0.3;
static constexpr double HANGER_LEAST = 0.03;
static constexpr int SAMPLES_PER_TILE = 4;
static constexpr double PIER_HALF_ALONG = 0.06;
static constexpr double PIER_HALF_ACROSS = 0.22;
static constexpr double CAP_HALF_ALONG = 0.085;
static constexpr double CAP_HALF_ACROSS = 0.3;
static constexpr double CAP_DEPTH = 0.04;
static constexpr double LEG_TOP_LATERAL = 0.26;
static constexpr double LEG_FOOT_LATERAL = 0.34;
static constexpr double TRESTLE_HALF = 0.018;
static constexpr double TROUGH_HEIGHT = 0.1;
static constexpr double WATER_TOP = 0.065;
static constexpr double CAISSON_MARGIN = 0.03;
static constexpr double CAISSON_RISE = 0.05;
static constexpr double CAISSON_SINK = 0.01;
static constexpr double WATER_GLOSS = 0.9;
static constexpr double STEEL_GLOSS = 0.45;
static constexpr int EMBANKMENT_ROWS = 4;
static constexpr double EMBANKMENT_RUN_PER_RISE = 0.8;
static constexpr double EMBANKMENT_SPREAD_MOST = 0.4;
static constexpr double EMBANKMENT_CROWN_DEPTH = 0.015;
static constexpr double EMBANKMENT_VARIETY = 0.08;
static constexpr uint32_t EMBANKMENT_SEED = 0xBA4C;
static constexpr uint32_t EMBANKMENT_GRASS = 0x5A7136;
static constexpr double FACE_LEAST_AREA = 1e-6;
static constexpr uint32_t EMBANKMENT_CROWN = 0x7D7466;

BridgeSite BridgeSite::OfHead(TileIndex head)
{
	TileIndex other = GetOtherBridgeEnd(head);
	Axis axis = DiagDirToAxis(GetTunnelBridgeDirection(head));
	bool head_first = axis == AXIS_X ? TileX(head) < TileX(other) : TileY(head) < TileY(other);
	return {head_first ? head : other, head_first ? other : head, axis, GetBridgeHeight(head), GetBridgeType(head), GetTunnelBridgeTransportType(head)};
}

BridgeSite BridgeSite::Above(TileIndex tile)
{
	return OfHead(GetSouthernBridgeEnd(tile));
}

static const BridgeLook &LookOf(const BridgeSite &bridge)
{
	if (bridge.transport == TRANSPORT_WATER || bridge.type >= MAX_BRIDGES) return AQUEDUCT_LOOK;
	return BRIDGE_LOOKS[bridge.type];
}

Footing DeckFooting(const BridgeSite &bridge)
{
	return LevelFooting(bridge.deck);
}

/* The direction a head's ramp climbs in, from its outer edge onto the bridge. */
static MapVector Onto(TileIndex head)
{
	return Outward(GetTunnelBridgeDirection(head));
}

static MapVector EdgeMiddle(TileIndex head, const MapVector &side)
{
	return MapVector{TileX(head) + HALF_TILE, TileY(head) + HALF_TILE} + side * HALF_TILE;
}

Footing RampFooting(const BridgeSite &bridge, TileIndex head)
{
	MapVector onto = Onto(head);
	MapVector outer = EdgeMiddle(head, onto * -1.0);
	double foot = TileGround(head).Level(outer.x, outer.y);
	double deck = bridge.deck;
	return [onto, outer, foot, deck](double x, double y) { return std::lerp(foot, deck, std::clamp(Dot(MapVector{x, y} - outer, onto), 0.0, 1.0)); };
}

/* Places along a bridge: how far past the northern head's inner edge, in tiles, and how far right of the centre line. */
class SpanFrame {
public:
	explicit SpanFrame(const BridgeSite &bridge) :
		along(bridge.axis == AXIS_X ? MapVector{1.0, 0.0} : MapVector{0.0, 1.0}),
		start(EdgeMiddle(bridge.north, this->along)),
		length(static_cast<double>(DistanceManhattan(bridge.north, bridge.south) - 1))
	{
	}

	MapVector At(double span, double lateral) const { return this->start + this->along * span + RightOf(this->along) * lateral; }
	Vec3 Point(double span, double lateral, double height) const
	{
		MapVector at = this->At(span, lateral);
		return {at.x, at.y, height};
	}
	double Entry(int tx, int ty) const { return Dot(MapVector{static_cast<double>(tx), static_cast<double>(ty)} - this->start, this->along); }
	double Length() const { return this->length; }
	Stretch Run(double from, double to) const { return {this->At(from, 0.0), this->At(to, 0.0)}; }

private:
	MapVector along;
	MapVector start;
	double length;
};

/* A deck slab with a low wall along either edge, or a trough of water for an aqueduct. */
static ModelMesh Deck(const Stretch &run, const BridgeLook &look, bool aqueduct, bool walled)
{
	ModelMesh deck = Laid(run, BoxSection(-DECK_HALF, DECK_HALF, -DECK_DEPTH, 0.0, look.deck, look.deck), 1);
	double height = aqueduct ? TROUGH_HEIGHT : PARAPET_HEIGHT;
	if (walled || aqueduct) {
		for (double side : {-1.0, 1.0}) deck.Append(Laid(run, BoxSection(side * DECK_HALF, side * (DECK_HALF - PARAPET_WIDTH), 0.0, height, look.frame, look.frame), 1));
	}
	if (aqueduct) {
		std::array<SectionPoint, 2> water = {{{-(DECK_HALF - PARAPET_WIDTH), WATER_TOP, WATER, WATER_GLOSS}, {DECK_HALF - PARAPET_WIDTH, WATER_TOP, WATER, WATER_GLOSS}}};
		deck.Append(Laid(run, water, 1));
	}
	return deck;
}

/* One tile of a bridge over the middle, its parts placed along the whole span so cables and arches run on from tile to tile. */
class SpanBuilder {
public:
	SpanBuilder(const BridgeSite &bridge, int tx, int ty) :
		bridge(bridge), look(LookOf(bridge)), frame(bridge), tx(tx), ty(ty), first(this->frame.Entry(tx, ty))
	{
	}

	ModelMesh Build()
	{
		bool aqueduct = this->bridge.transport == TRANSPORT_WATER;
		ModelMesh mesh = Deck(this->frame.Run(this->first, this->first + 1.0), this->look, aqueduct, this->look.build != BridgeBuild::Girder);
		switch (this->look.build) {
			case BridgeBuild::Trestle: this->Trestle(mesh); break;
			case BridgeBuild::Beam: this->Beam(mesh); break;
			case BridgeBuild::Girder: this->Girder(mesh); break;
			case BridgeBuild::Suspension: this->Suspension(mesh); break;
			case BridgeBuild::Truss: this->Truss(mesh); break;
			case BridgeBuild::Arch: this->Arch(mesh); break;
		}
		return mesh;
	}

private:
	/* How far below the deck the bed lies under a place on the span, in tiles. */
	double BedDepth(double span, double lateral) const
	{
		MapVector at = this->frame.At(span, lateral);
		return (BedLevel(this->tx, this->ty, at.x, at.y) - this->bridge.deck) * LevelRise();
	}

	/* How far below the deck the water's surface lies over a place on the span, where it stands above the bed there. */
	std::optional<double> WaterDepth(double span, double lateral) const
	{
		if (WaterFormOf(this->tx, this->ty) == WaterForm::Dry) return std::nullopt;
		double water = (SurfaceLevelOf(this->tx, this->ty) - this->bridge.deck) * LevelRise();
		if (water <= this->BedDepth(span, lateral)) return std::nullopt;
		return water;
	}

	/* Supports standing in water rise from a caisson at the waterline, as what stands below it would only waver through the surface. */
	double FootDepth(ModelMesh &mesh, double span, double lateral, double half_along, double half_across) const
	{
		std::optional<double> water = this->WaterDepth(span, lateral);
		if (!water.has_value()) return this->BedDepth(span, lateral);
		MapVector at = this->frame.At(span, lateral);
		MapVector along = this->frame.At(span + 1.0, lateral) - at;
		double spread_along = half_along + CAISSON_MARGIN;
		double spread_across = half_across + CAISSON_MARGIN;
		mesh.Append(Block(at, along, {-spread_along, -spread_across, *water - CAISSON_SINK}, {spread_along, spread_across, *water + CAISSON_RISE}).Paint(CONCRETE));
		return *water + CAISSON_RISE;
	}

	/* Piers stand only on open ground or water, never on a way or a building beneath the bridge. */
	bool Clear() const
	{
		switch (GetTileType(TileXY(this->tx, this->ty))) {
			case MP_CLEAR:
			case MP_TREES:
			case MP_WATER: return true;
			default: return false;
		}
	}

	bool PierHere(int every) const
	{
		int index = static_cast<int>(std::lround(this->first));
		return this->Clear() && this->frame.Length() > 1.0 && (index + 1) % every == 0 && index + 1 < this->frame.Length();
	}

	void Pier(ModelMesh &mesh, double top) const
	{
		double middle = this->first + HALF_TILE;
		MapVector at = this->frame.At(middle, 0.0);
		MapVector along = this->frame.At(middle + 1.0, 0.0) - at;
		double foot = this->FootDepth(mesh, middle, 0.0, PIER_HALF_ALONG, PIER_HALF_ACROSS);
		mesh.Append(Block(at, along, {-PIER_HALF_ALONG, -PIER_HALF_ACROSS, foot}, {PIER_HALF_ALONG, PIER_HALF_ACROSS, top - CAP_DEPTH}).Paint(CONCRETE));
		mesh.Append(Block(at, along, {-CAP_HALF_ALONG, -CAP_HALF_ACROSS, top - CAP_DEPTH}, {CAP_HALF_ALONG, CAP_HALF_ACROSS, top}).Paint(CONCRETE));
	}

	void Member(ModelMesh &mesh, const Vec3 &from, const Vec3 &to, double half = MEMBER_HALF) const
	{
		mesh.Append(Strut(from, to, half).Paint(this->look.frame).Gloss(STEEL_GLOSS));
	}

	void Trestle(ModelMesh &mesh) const
	{
		if (!this->Clear()) return;
		double middle = this->first + HALF_TILE;
		for (double side : {-1.0, 1.0}) {
			Vec3 top = this->frame.Point(middle, side * LEG_TOP_LATERAL, -DECK_DEPTH);
			double depth = this->FootDepth(mesh, middle, side * LEG_FOOT_LATERAL, TRESTLE_HALF, TRESTLE_HALF);
			this->Member(mesh, this->frame.Point(middle, side * LEG_FOOT_LATERAL, depth), top, TRESTLE_HALF);
		}
		double low = std::max(this->BedDepth(middle, -LEG_FOOT_LATERAL), this->BedDepth(middle, LEG_FOOT_LATERAL));
		for (double side : {-1.0, 1.0}) low = std::max(low, this->WaterDepth(middle, side * LEG_FOOT_LATERAL).value_or(low));
		Vec3 left_top = this->frame.Point(middle, -LEG_TOP_LATERAL, -DECK_DEPTH);
		Vec3 right_top = this->frame.Point(middle, LEG_TOP_LATERAL, -DECK_DEPTH);
		Vec3 left_low = this->frame.Point(middle, -LEG_FOOT_LATERAL, low);
		Vec3 right_low = this->frame.Point(middle, LEG_FOOT_LATERAL, low);
		this->Member(mesh, left_top, right_low, THIN_HALF * 2.0);
		this->Member(mesh, right_top, left_low, THIN_HALF * 2.0);
	}

	void Beam(ModelMesh &mesh) const
	{
		std::array<SectionPoint, 4> box = {{
			{BOX_HALF_TOP, -DECK_DEPTH, this->look.frame}, {BOX_HALF_FOOT, -BOX_DEPTH, this->look.frame},
			{-BOX_HALF_FOOT, -BOX_DEPTH, this->look.frame}, {-BOX_HALF_TOP, -DECK_DEPTH, this->look.frame},
		}};
		mesh.Append(Laid(this->frame.Run(this->first, this->first + 1.0), box, 1));
		if (this->PierHere(2)) this->Pier(mesh, -BOX_DEPTH);
	}

	void Girder(ModelMesh &mesh) const
	{
		Stretch run = this->frame.Run(this->first, this->first + 1.0);
		for (double side : {-1.0, 1.0}) {
			mesh.Append(Laid(run, BoxSection(side * DECK_HALF, side * (DECK_HALF - PARAPET_WIDTH), 0.0, GIRDER_HEIGHT, this->look.frame, this->look.frame, STEEL_GLOSS), 1));
			for (double span = this->first + STIFFENER_PITCH * 0.5; span < this->first + 1.0; span += STIFFENER_PITCH) {
				MapVector at = this->frame.At(span, side * (DECK_HALF + STIFFENER_HALF));
				mesh.Append(Block(at, run.Along(), {-STIFFENER_HALF, -STIFFENER_HALF, -DECK_DEPTH}, {STIFFENER_HALF, STIFFENER_HALF, GIRDER_HEIGHT}).Paint(this->look.frame).Gloss(STEEL_GLOSS));
			}
		}
		if (this->PierHere(2)) this->Pier(mesh, -DECK_DEPTH);
	}

	/* Each side is a run of triangles: a vertical at every half tile, a diagonal across each half, and the top chord over them. */
	void Truss(ModelMesh &mesh) const
	{
		double last = this->first + 1.0;
		for (double side : {-1.0, 1.0}) {
			double lateral = side * TRUSS_LATERAL;
			this->Member(mesh, this->frame.Point(this->first, lateral, TRUSS_HEIGHT), this->frame.Point(last, lateral, TRUSS_HEIGHT));
			for (double span : {this->first, this->first + HALF_TILE}) {
				this->Member(mesh, this->frame.Point(span, lateral, 0.0), this->frame.Point(span, lateral, TRUSS_HEIGHT));
				bool rising = std::lround(span * 2.0) % 2 == 0;
				double low = rising ? span : span + HALF_TILE;
				double high = rising ? span + HALF_TILE : span;
				this->Member(mesh, this->frame.Point(low, lateral, 0.0), this->frame.Point(high, lateral, TRUSS_HEIGHT));
			}
		}
		for (double span : {this->first, this->first + HALF_TILE}) {
			this->Member(mesh, this->frame.Point(span, -TRUSS_LATERAL, TRUSS_HEIGHT), this->frame.Point(span, TRUSS_LATERAL, TRUSS_HEIGHT), THIN_HALF * 2.0);
		}
		if (last >= this->frame.Length()) {
			for (double side : {-1.0, 1.0}) this->Member(mesh, this->frame.Point(last, side * TRUSS_LATERAL, 0.0), this->frame.Point(last, side * TRUSS_LATERAL, TRUSS_HEIGHT));
		}
		if (this->PierHere(3)) this->Pier(mesh, -DECK_DEPTH);
	}

	/* A line over the span sampled across this tile, hung from the deck's edges every sample where it stands clear of the deck. */
	template <class Height>
	void Hung(ModelMesh &mesh, double lateral, Height height) const
	{
		for (double side : {-1.0, 1.0}) {
			for (int sample = 0; sample < SAMPLES_PER_TILE; sample++) {
				double from = this->first + static_cast<double>(sample) / SAMPLES_PER_TILE;
				double to = this->first + static_cast<double>(sample + 1) / SAMPLES_PER_TILE;
				this->Member(mesh, this->frame.Point(from, side * lateral, height(from)), this->frame.Point(to, side * lateral, height(to)));
				if (height(from) > HANGER_LEAST) this->Member(mesh, this->frame.Point(from, side * lateral, 0.0), this->frame.Point(from, side * lateral, height(from)), THIN_HALF);
			}
		}
	}

	/* Towers stand a sixth of the way in from either head, taller the longer the span; the cables sag between them and fall back to the heads beyond. */
	void Suspension(ModelMesh &mesh) const
	{
		double length = this->frame.Length();
		if (length < 2.0) {
			this->Beam(mesh);
			return;
		}
		double near = std::floor(length * TOWER_SHARE) + HALF_TILE;
		double far = length - near;
		double top = std::clamp(TOWER_LEAST + TOWER_PER_TILE * length, TOWER_LEAST, TOWER_MOST);
		double saddle = top - CABLE_BELOW_TOP;
		auto cable = [=](double span) {
			if (span <= near) return saddle * span / near;
			if (span >= far) return saddle * (length - span) / (length - far);
			double share = (span - near) / (far - near) * 2.0 - 1.0;
			return CABLE_SAG + (saddle - CABLE_SAG) * share * share;
		};
		this->Hung(mesh, CABLE_LATERAL, cable);
		double middle = this->first + HALF_TILE;
		if (std::abs(middle - near) > 0.01 && std::abs(middle - far) > 0.01) return;
		for (double side : {-1.0, 1.0}) {
			double lateral = side * TOWER_LATERAL;
			double foot = this->FootDepth(mesh, middle, lateral, TOWER_HALF, TOWER_HALF);
			this->Member(mesh, this->frame.Point(middle, lateral, foot), this->frame.Point(middle, lateral, top), TOWER_HALF);
		}
		for (double height : {-DECK_DEPTH - TOWER_HALF, saddle - TOWER_HALF, top - TOWER_HALF}) {
			this->Member(mesh, this->frame.Point(middle, -TOWER_LATERAL, height), this->frame.Point(middle, TOWER_LATERAL, height), TOWER_HALF * 0.8);
		}
	}

	/* The arch springs from the heads and rises with the span, braced across where it stands high enough. */
	void Arch(ModelMesh &mesh) const
	{
		double length = this->frame.Length();
		double rise = std::clamp(ARCH_RISE_BASE + ARCH_RISE_PER_TILE * length, ARCH_RISE_BASE, ARCH_RISE_MOST);
		auto arch = [=](double span) { return rise * 4.0 * span * (length - span) / (length * length); };
		this->Hung(mesh, ARCH_LATERAL, arch);
		if (arch(this->first) > ARCH_BRACED) {
			this->Member(mesh, this->frame.Point(this->first, -ARCH_LATERAL, arch(this->first)), this->frame.Point(this->first, ARCH_LATERAL, arch(this->first)), THIN_HALF * 2.0);
		}
	}

	const BridgeSite &bridge;
	const BridgeLook &look;
	SpanFrame frame;
	int tx;
	int ty;
	double first;
};

void LayBridgeSpan(ModelMesh &mesh, const BridgeSite &bridge, int tx, int ty)
{
	ModelMesh span = SpanBuilder(bridge, tx, ty).Build();
	mesh.Append(Drape(span, DeckFooting(bridge)));
}

/* A face of four corners going round, lit from the side it faces out to; one with no area is left out. */
static void FacingQuad(ModelMesh &mesh, const std::array<Vec3, 4> &corners, const Vec3 &outward)
{
	Vec3 across = Cross(corners[2] - corners[0], corners[3] - corners[1]);
	if (Length(across) < FACE_LEAST_AREA) return;
	Vec3 normal = Normalised(across);
	if (Dot(normal, outward) < 0.0) normal = normal * -1.0;
	std::array<uint32_t, 4> points;
	for (size_t i = 0; i < corners.size(); i++) points[i] = mesh.Point(corners[i], normal);
	mesh.Quad(points[0], points[1], points[2], points[3]);
}

/* A ramp on the ground climbs over its head on an earth embankment, its sides sloping out onto the ground beside it, and meets the span at a stone abutment
 * whose wing walls close the bank's end. Points are in render space. */
class Embankment {
public:
	Embankment(const BridgeSite &bridge, TileIndex head) : footing(RampFooting(bridge, head)), onto(Onto(head)), run({EdgeMiddle(head, onto * -1.0), EdgeMiddle(head, onto)})
	{
	}

	ModelMesh Build() const
	{
		ModelMesh sides;
		ModelMesh crown;
		for (int row = 0; row < EMBANKMENT_ROWS; row++) {
			double from = static_cast<double>(row) / EMBANKMENT_ROWS;
			double to = static_cast<double>(row + 1) / EMBANKMENT_ROWS;
			for (double side : {-1.0, 1.0}) FacingQuad(sides, {this->Top(from, side), this->Foot(from, side), this->Foot(to, side), this->Top(to, side)}, this->Outward(side));
			FacingQuad(crown, {this->Top(from, -1.0), this->Top(from, 1.0), this->Top(to, 1.0), this->Top(to, -1.0)}, {0.0, 0.0, 1.0});
		}
		ModelMesh mesh = sides.Paint(EMBANKMENT_GRASS).Vary(EMBANKMENT_VARIETY, EMBANKMENT_SEED);
		mesh.Append(crown.Paint(EMBANKMENT_CROWN));
		mesh.Append(this->End(0.0, EMBANKMENT_GRASS));
		return mesh.Append(this->End(1.0, STONE));
	}

private:
	double Depth(const MapVector &at) const { return std::max(this->footing(at.x, at.y) - GroundLevel(at.x, at.y), 0.0) * LevelRise(); }
	Vec3 Outward(double side) const
	{
		MapVector right = this->run.Right() * side;
		return {right.x, right.y, 1.0};
	}

	Vec3 Top(double share, double side) const
	{
		MapVector at = this->run.At(share, side * DECK_HALF);
		return {at.x, at.y, this->footing(at.x, at.y) * LevelRise() - EMBANKMENT_CROWN_DEPTH};
	}

	/* The bank's foot reaches out with the ground's depth below the top, then rests on the ground there. */
	Vec3 Foot(double share, double side) const
	{
		MapVector edge = this->run.At(share, side * DECK_HALF);
		double spread = std::min(this->Depth(edge) * EMBANKMENT_RUN_PER_RISE, EMBANKMENT_SPREAD_MOST);
		MapVector at = edge + this->run.Right() * (side * spread);
		return {at.x, at.y, std::min(GroundLevel(at.x, at.y) * LevelRise(), this->Top(share, side).z)};
	}

	ModelMesh End(double share, uint32_t tone) const
	{
		Vec3 facing = {this->onto.x * (share - 0.5), this->onto.y * (share - 0.5), 0.0};
		ModelMesh end;
		FacingQuad(end, {this->Foot(share, -1.0), this->Top(share, -1.0), this->Top(share, 1.0), this->Foot(share, 1.0)}, facing);
		return end.Paint(tone);
	}

	Footing footing;
	MapVector onto;
	Stretch run;
};

/* An aqueduct's ramp is its trough climbing over the head, walled down to the ground beneath it and facing the span with an abutment. */
static void LayAqueductRamp(ModelMesh &mesh, const BridgeSite &bridge, TileIndex head)
{
	MapVector onto = Onto(head);
	Stretch run = {EdgeMiddle(head, onto * -1.0), EdgeMiddle(head, onto)};
	ModelMesh ramp = Deck(run, AQUEDUCT_LOOK, true, true);

	Footing footing = RampFooting(bridge, head);
	TileGround ground(head);
	double rise = LevelRise();
	auto below = [&](const MapVector &at) { return Vec3{at.x, at.y, (ground.Level(at.x, at.y) - footing(at.x, at.y)) * rise}; };
	auto deck_foot = [](const MapVector &at) { return Vec3{at.x, at.y, -DECK_DEPTH}; };
	ModelMesh abutment;
	auto face = [&](const MapVector &from, const MapVector &to, const MapVector &outward) {
		Vec3 facing = {outward.x, outward.y, 0.0};
		uint32_t a = abutment.Point(below(from), facing);
		uint32_t b = abutment.Point(below(to), facing);
		uint32_t c = abutment.Point(deck_foot(to), facing);
		uint32_t d = abutment.Point(deck_foot(from), facing);
		abutment.Quad(a, b, c, d);
	};
	MapVector right = run.Right();
	for (double side : {-1.0, 1.0}) face(run.At(0.0, side * DECK_HALF), run.At(1.0, side * DECK_HALF), right * side);
	face(run.At(1.0, -DECK_HALF), run.At(1.0, DECK_HALF), onto);
	ramp.Append(abutment.Paint(STONE));
	mesh.Append(Drape(ramp, footing));
}

void LayBridgeRamp(ModelMesh &mesh, const BridgeSite &bridge, TileIndex head)
{
	if (bridge.transport == TRANSPORT_WATER) {
		LayAqueductRamp(mesh, bridge, head);
		return;
	}
	mesh.Append(Embankment(bridge, head).Build());
}
