/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file road_models.cpp Roads as 3D surfaces: asphalt with rounded junctions and bends, kerbs and pavements in town, lane markings, tram tracks and one way arrows. */

#include "../../stdafx.h"
#include "road_models.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <optional>

#include "../core/seed.h"
#include "../core/tones.h"
#include "../map/car_look.h"
#include "../map/network_style.h"
#include "street_furniture.h"

#include "../../safeguards.h"

static constexpr int ARMS = 4;
static constexpr double EDGE = 0.5;
static constexpr double ON_EDGE = 1.0e-6;
static constexpr double FILLET = 0.2;
static constexpr int FILLET_STEPS = 4;
static constexpr int BEND_STEPS = 10;
static constexpr int END_STEPS = 4;
static constexpr double KERB_FOOT = ASPHALT_TOP - 0.002;

static constexpr double MARK_TOP = ASPHALT_TOP + 0.0015;
static constexpr double MARK_HALF = 0.011;
static constexpr double EDGE_INSET = 0.035;
static constexpr int DASHES = 2;
static constexpr double DASH_SHARE = 0.48;
static constexpr double ARROW_TAIL = 0.17;
static constexpr double ARROW_NECK = 0.02;
static constexpr double ARROW_TIP = 0.17;
static constexpr double ARROW_SHAFT_HALF = 0.024;
static constexpr double ARROW_HEAD_HALF = 0.075;
static constexpr double BAR_HALF_LENGTH = 0.03;
static constexpr double BAR_HALF_WIDTH = 0.2;

static constexpr double ZEBRA_NEAR = 0.34;
static constexpr double ZEBRA_FAR = 0.44;
static constexpr double ZEBRA_STRIPE = 0.04;
static constexpr double ZEBRA_PITCH = 0.075;
static constexpr double ZEBRA_MARGIN = 0.03;
static constexpr double LAMP_ALONG = -0.35;
static constexpr double LAMP_LATERAL = 0.43;
static constexpr double AMENITY_LATERAL = 0.465;
static constexpr double TREE_SHARE = 0.3;
static constexpr double BENCH_SHARE = 0.18;
static constexpr double BIN_SHARE = 0.16;
static constexpr uint32_t AMENITY_SALT = 0x3A11E7U;
static constexpr double KERBSIDE_SHARE = 0.4;
static constexpr double KERBSIDE_REACH = 0.3;
static constexpr double KERBSIDE_LATERAL = ROAD_HALF;
static constexpr double KERB_PERCH = 0.012;
static constexpr uint32_t KERBSIDE_SALT = 0x6A7C11E5U;
static constexpr double BARRIER_ALONG = 0.4;
static constexpr double BARRIER_LATERAL = ROAD_HALF + 0.06;

static constexpr double TRAM_GAUGE_HALF = 0.105;
static constexpr double TRAM_RAIL_HALF = 0.011;
static constexpr double TRAM_RAIL_TOP = ASPHALT_TOP + 0.0025;
static constexpr double TRAM_BED_TOP = 0.009;

static constexpr uint32_t ASPHALT = COL_ASPHALT;
static constexpr uint32_t SHOULDER = 0x3D3F43;
static constexpr uint32_t PAVEMENT = 0xB7B2A8;
static constexpr uint32_t KERB = 0xCDC9C0;
static constexpr uint32_t MARKING = 0xE8E6DE;
static constexpr uint32_t NO_ENTRY = 0xC8463C;
static constexpr uint32_t TRAM_BED = 0x9C998F;
static constexpr uint32_t TRAM_RAIL = 0xBFC3C7;
static constexpr double ASPHALT_GLOSS = 0.08;
static constexpr double STEEL_GLOSS = 0.85;
static constexpr double PAVEMENT_VARIETY = 0.06;
static constexpr uint32_t PAVEMENT_SALT = 0x9A7E3E17U;

/* The way each of the game's road bits leads out of the tile, in bit order, so each arm lies a quarter turn from the next. */
static constexpr std::array<MapVector, ARMS> ARM_WAYS = {{{0.0, -1.0}, {1.0, 0.0}, {0.0, 1.0}, {-1.0, 0.0}}};
static constexpr MapVector MIDDLE = {0.5, 0.5};

static bool Has(RoadBits bits, int arm)
{
	return (bits & (1 << arm)) != 0;
}

static int Next(int arm)
{
	return (arm + 1) % ARMS;
}

static void Extend(std::vector<MapVector> &line, const std::vector<MapVector> &more)
{
	line.insert(line.end(), more.begin(), more.end());
}

/* A bend is a quarter of a ring about the tile corner its two arms share, so it keeps the road's width all the way round. */
static std::vector<MapVector> BendOutline(int arm)
{
	MapVector out = ARM_WAYS[arm];
	MapVector on = ARM_WAYS[Next(arm)];
	MapVector corner = (out + on) * EDGE;
	std::vector<MapVector> outline = Arc(corner, EDGE + ROAD_HALF, on * -1.0, out * -1.0, BEND_STEPS);
	Extend(outline, Arc(corner, EDGE - ROAD_HALF, out * -1.0, on * -1.0, BEND_STEPS / 2));
	return outline;
}

/* Between two arms the edge is filleted; where neither side has an arm it rounds off, so a dead end closes in a half circle. */
static void AddCorner(std::vector<MapVector> &outline, RoadBits bits, int arm)
{
	MapVector out = ARM_WAYS[arm];
	MapVector on = ARM_WAYS[Next(arm)];
	MapVector corner = (out + on) * ROAD_HALF;
	bool here = Has(bits, arm);
	bool there = Has(bits, Next(arm));
	if (here && there) {
		Extend(outline, Arc(corner + (out + on) * FILLET, FILLET, on * -1.0, out * -1.0, FILLET_STEPS));
	} else if (!here && !there) {
		Extend(outline, Arc(corner - (out + on) * ROAD_HALF, ROAD_HALF, out, on, END_STEPS));
	} else {
		outline.push_back(corner);
	}
}

static std::optional<int> BendArm(RoadBits bits)
{
	if (std::popcount(static_cast<uint>(bits)) != 2) return std::nullopt;
	for (int arm = 0; arm < ARMS; arm++) {
		if (Has(bits, arm) && Has(bits, Next(arm))) return arm;
	}
	return std::nullopt;
}

/* The asphalt's edge going round the tile's middle arm by arm, every point of it in sight of the middle. */
static std::vector<MapVector> Outline(RoadBits bits)
{
	if (std::optional<int> bend = BendArm(bits); bend.has_value()) return BendOutline(*bend);
	std::vector<MapVector> outline;
	for (int arm = 0; arm < ARMS; arm++) {
		MapVector out = ARM_WAYS[arm];
		MapVector on = ARM_WAYS[Next(arm)];
		if (Has(bits, arm)) {
			outline.push_back(out * EDGE - on * ROAD_HALF);
			outline.push_back(out * EDGE + on * ROAD_HALF);
		}
		AddCorner(outline, bits, arm);
	}
	return outline;
}

static bool OnTileEdge(const MapVector &a, const MapVector &b)
{
	auto edge = [](double u, double v) { return std::abs(std::abs(u) - EDGE) < ON_EDGE && std::abs(u - v) < ON_EDGE; };
	return edge(a.x, b.x) || edge(a.y, b.y);
}

/* The stretches of the asphalt's edge that border the tile's ground rather than run out over its edge, each turned to keep the asphalt on its right. */
static std::vector<std::vector<MapVector>> Borders(const std::vector<MapVector> &outline)
{
	size_t count = outline.size();
	size_t start = 0;
	while (start < count && !OnTileEdge(outline[start], outline[(start + 1) % count])) start++;

	std::vector<std::vector<MapVector>> borders;
	std::vector<MapVector> border;
	for (size_t step = 1; step <= count; step++) {
		const MapVector &from = outline[(start + step) % count];
		const MapVector &to = outline[(start + step + 1) % count];
		if (OnTileEdge(from, to)) {
			if (border.size() > 1) borders.push_back(std::move(border));
			border.clear();
			continue;
		}
		if (border.empty()) border.push_back(from);
		border.push_back(to);
	}
	if (border.size() > 1) borders.push_back(std::move(border));

	for (std::vector<MapVector> &line : borders) {
		MapVector middle = (line[0] + line[1]) * 0.5;
		if (Dot(RightOf(line[1] - line[0]), middle * -1.0) < 0.0) std::ranges::reverse(line);
	}
	return borders;
}

/* A road site's own builder: every shape is made about the tile's middle, then moved onto the tile and laid on its footing. */
class RoadBuilder {
public:
	RoadBuilder(const RoadSite &site, ModelMesh &mesh, const Footing &footing) :
		site(site), mesh(mesh), footing(footing), origin(MapVector{static_cast<double>(site.tx), static_cast<double>(site.ty)} + MIDDLE)
	{
	}

	/* The parts are laid on the footing together, as many share the spots where they meet. */
	void Build()
	{
		if (this->site.road != ROAD_NONE) this->Asphalt();
		if (this->site.tram != ROAD_NONE) this->Tram();
		this->mesh.Append(Drape(this->parts, this->footing));
	}

private:
	void Lay(ModelMesh part)
	{
		this->parts.Append(part.Move({this->origin.x, this->origin.y, 0.0}));
	}

	void Lay(ModelMesh part, uint32_t tone, double gloss = 0.0)
	{
		this->Lay(std::move(part.Paint(tone).Gloss(gloss)));
	}

	/* The direction a plain road runs in, toward higher map coordinates. */
	static MapVector Along(RoadBits bits)
	{
		return bits == ROAD_X ? MapVector{1.0, 0.0} : MapVector{0.0, 1.0};
	}

	static bool Plain(RoadBits bits)
	{
		return bits == ROAD_X || bits == ROAD_Y;
	}

	bool Full() const { return this->site.detail == WayDetail::Full; }

	void Asphalt()
	{
		std::vector<MapVector> outline = Outline(this->site.road);
		std::vector<std::vector<MapVector>> borders = Borders(outline);
		this->Lay(Plate(outline, {0.0, 0.0}, ASPHALT_TOP), ASPHALT, ASPHALT_GLOSS);
		if (this->site.kerbed) {
			this->Pavement(outline);
			for (const auto &border : borders) this->Lay(Wall(border, KERB_FOOT, PAVEMENT_TOP), KERB);
		} else {
			for (auto border : borders) {
				std::ranges::reverse(border);
				this->Lay(Wall(border, WAY_FOOT, ASPHALT_TOP), SHOULDER);
			}
		}
		if (!this->Full()) return;
		if (!this->site.kerbed && !this->site.crossing) {
			for (const auto &border : borders) this->Lay(Ribbon(Offset(border, EDGE_INSET), MARK_HALF, MARK_TOP), MARKING);
		}
		this->CentreLine();
		this->OneWay();
		this->Crosswalks();
		this->StreetLight();
		this->KerbsideParking();
		this->Barriers();
	}

	/* Where town streets meet, each arm is crossed by a zebra just short of the junction. */
	void Crosswalks()
	{
		RoadBits bits = this->site.road;
		if (!this->site.kerbed || std::popcount(static_cast<uint>(bits)) < 3) return;
		for (int arm = 0; arm < ARMS; arm++) {
			if (!Has(bits, arm)) continue;
			MapVector out = ARM_WAYS[arm];
			MapVector across = RightOf(out);
			for (double lateral = -ROAD_HALF + ZEBRA_MARGIN; lateral + ZEBRA_STRIPE <= ROAD_HALF - ZEBRA_MARGIN; lateral += ZEBRA_PITCH) {
				std::array<MapVector, 4> stripe = {out * ZEBRA_NEAR + across * lateral, out * ZEBRA_FAR + across * lateral, out * ZEBRA_FAR + across * (lateral + ZEBRA_STRIPE), out * ZEBRA_NEAR + across * (lateral + ZEBRA_STRIPE)};
				this->Lay(Plate(stripe, out * ((ZEBRA_NEAR + ZEBRA_FAR) * 0.5) + across * (lateral + ZEBRA_STRIPE * 0.5), MARK_TOP), MARKING);
			}
		}
	}

	/* A plain town street is lit from its pavement, the lamps standing on alternate sides from tile to tile, with room across from each for an amenity. */
	void StreetLight()
	{
		RoadBits bits = this->site.road;
		if (!this->site.kerbed || !Plain(bits)) return;
		double side = ((this->site.tx + this->site.ty) & 1) == 0 ? 1.0 : -1.0;
		MapVector along = Along(bits);
		MapVector across = RightOf(along) * side;
		this->Lay(LampPost(along * LAMP_ALONG + across * LAMP_LATERAL, across * -1.0, PAVEMENT_TOP));
		this->Amenity(along * -LAMP_ALONG + across * -AMENITY_LATERAL, across);
	}

	/* Across from the lamp the pavement may hold a street tree, a bench facing the road or a litter bin, out by the edge where walkers pass in front. */
	void Amenity(const MapVector &at, const MapVector &toward_road)
	{
		uint32_t seed = Hash32(AMENITY_SALT + static_cast<uint32_t>(this->site.tx * 4099 + this->site.ty));
		double pick = SeedShare(seed, 0, SeedDice::SHARE_BITS);
		if (pick < TREE_SHARE) {
			this->Lay(StreetTree(at, PAVEMENT_TOP, seed));
		} else if (pick < TREE_SHARE + BENCH_SHARE) {
			this->Lay(Bench(at, toward_road * -1.0, PAVEMENT_TOP));
		} else if (pick < TREE_SHARE + BENCH_SHARE + BIN_SHARE) {
			this->Lay(LitterBin(at, PAVEMENT_TOP));
		}
	}

	/* Along a plain town street a car may stand at either kerb, half up on the pavement, clear of the lane traffic drives in and of the walkers on the pavement, nose toward the traffic on its side. */
	void KerbsideParking()
	{
		if (!this->site.kerbside || !this->site.kerbed || !Plain(this->site.road)) return;
		SeedDice dice(Hash32(KERBSIDE_SALT + static_cast<uint32_t>(this->site.tx * 4099 + this->site.ty)));
		MapVector along = Along(this->site.road);
		for (double side : {-1.0, 1.0}) {
			if (dice.Share() >= KERBSIDE_SHARE) continue;
			MapVector at = along * dice.Between(-KERBSIDE_REACH, KERBSIDE_REACH) + RightOf(along) * (side * KERBSIDE_LATERAL);
			this->Lay(ParkedCar(at, along * -side, PAVEMENT_TOP - KERB_PERCH, CAR_TINTS[dice.Below(static_cast<uint32_t>(CAR_TINTS.size()))]));
		}
	}

	/* A barrier stands at each approach to a level crossing, on the right of the traffic it stops. */
	void Barriers()
	{
		if (!this->site.crossing || !Plain(this->site.road)) return;
		MapVector along = Along(this->site.road);
		for (double end : {-1.0, 1.0}) {
			MapVector facing = along * end;
			this->Lay(CrossingBarrier(facing * BARRIER_ALONG + RightOf(facing) * -BARRIER_LATERAL, facing, 0.0));
		}
	}

	/* Each stretch between two rays from the middle runs from the asphalt's edge out to the tile's, taking in the tile corner it passes. */
	void Pavement(const std::vector<MapVector> &outline)
	{
		auto rim = [](const MapVector &at) { return at * (EDGE / std::max(std::abs(at.x), std::abs(at.y))); };
		auto across_x = [](const MapVector &at) { return std::abs(std::abs(at.x) - EDGE) < ON_EDGE; };
		ModelMesh pavement;
		auto point = [&pavement](const MapVector &at) { return pavement.Point({at.x, at.y, PAVEMENT_TOP}); };
		for (size_t index = 0; index < outline.size(); index++) {
			const MapVector &inner = outline[index];
			const MapVector &next = outline[(index + 1) % outline.size()];
			MapVector outer = rim(inner);
			MapVector outer_next = rim(next);
			uint32_t first = point(inner);
			uint32_t rim_first = point(outer);
			uint32_t rim_last = point(outer_next);
			if (across_x(outer) != across_x(outer_next)) {
				uint32_t corner = point(across_x(outer) ? MapVector{outer.x, outer_next.y} : MapVector{outer_next.x, outer.y});
				pavement.Triangle(first, rim_first, corner);
				pavement.Triangle(first, corner, rim_last);
			} else {
				pavement.Triangle(first, rim_first, rim_last);
			}
			pavement.Triangle(first, rim_last, point(next));
		}
		uint32_t seed = Hash32(PAVEMENT_SALT + static_cast<uint32_t>(this->site.tx * 4099 + this->site.ty));
		this->Lay(std::move(pavement.Vary(PAVEMENT_VARIETY, seed)), PAVEMENT);
	}

	/* Dashes run down the middle of a plain road or round a bend, in step with the tiles either side. */
	void CentreLine()
	{
		RoadBits bits = this->site.road;
		if (this->site.crossing || this->site.tram != ROAD_NONE || this->site.one_way != DRD_NONE) return;
		if (Plain(bits)) {
			MapVector along = Along(bits);
			for (int dash = 0; dash < DASHES; dash++) {
				double middle = (dash + 0.5) / DASHES - EDGE;
				std::array<MapVector, 2> line = {along * (middle - DASH_SHARE * EDGE / DASHES), along * (middle + DASH_SHARE * EDGE / DASHES)};
				this->Lay(Ribbon(line, MARK_HALF, MARK_TOP), MARKING);
			}
			return;
		}
		std::optional<int> bend = BendArm(bits);
		if (!bend.has_value()) return;
		MapVector out = ARM_WAYS[*bend];
		MapVector on = ARM_WAYS[Next(*bend)];
		std::vector<MapVector> arc = Arc((out + on) * EDGE, EDGE, on * -1.0, out * -1.0, BEND_STEPS);
		for (int dash = 0; dash < DASHES; dash++) {
			size_t first = static_cast<size_t>(std::lround(BEND_STEPS * (dash + 0.5 - DASH_SHARE * 0.5) / DASHES));
			size_t last = static_cast<size_t>(std::lround(BEND_STEPS * (dash + 0.5 + DASH_SHARE * 0.5) / DASHES));
			this->Lay(Ribbon(std::span<const MapVector>(arc).subspan(first, last - first + 1), MARK_HALF, MARK_TOP), MARKING);
		}
	}

	/* The game's northbound traffic heads toward lower map coordinates. */
	void OneWay()
	{
		RoadBits bits = this->site.road;
		if (this->site.one_way == DRD_NONE || !Plain(bits)) return;
		MapVector south = Along(bits);
		if (this->site.one_way == DRD_BOTH) {
			MapVector across = RightOf(south);
			std::array<MapVector, 4> bar = {south * -BAR_HALF_LENGTH - across * BAR_HALF_WIDTH, south * BAR_HALF_LENGTH - across * BAR_HALF_WIDTH, south * BAR_HALF_LENGTH + across * BAR_HALF_WIDTH, south * -BAR_HALF_LENGTH + across * BAR_HALF_WIDTH};
			this->Lay(Plate(bar, {0.0, 0.0}, MARK_TOP), NO_ENTRY);
			return;
		}
		MapVector heading = this->site.one_way == DRD_SOUTHBOUND ? south * -1.0 : south;
		MapVector side = RightOf(heading);
		std::array<MapVector, 4> shaft = {heading * -ARROW_TAIL - side * ARROW_SHAFT_HALF, heading * ARROW_NECK - side * ARROW_SHAFT_HALF, heading * ARROW_NECK + side * ARROW_SHAFT_HALF, heading * -ARROW_TAIL + side * ARROW_SHAFT_HALF};
		std::array<MapVector, 3> head = {heading * ARROW_NECK - side * ARROW_HEAD_HALF, heading * ARROW_TIP, heading * ARROW_NECK + side * ARROW_HEAD_HALF};
		this->Lay(Plate(shaft, heading * ((ARROW_NECK - ARROW_TAIL) * 0.5), MARK_TOP), MARKING);
		this->Lay(Plate(head, heading * ((2.0 * ARROW_NECK + ARROW_TIP) / 3.0), MARK_TOP), MARKING);
	}

	/* A tram runs between every pair of its ends, round the shared corner between neighbouring ones, and up to the middle from a lone end. */
	std::vector<std::vector<MapVector>> TramRoutes(double offset) const
	{
		RoadBits bits = this->site.tram;
		bool lone = std::popcount(static_cast<uint>(bits)) == 1;
		std::vector<std::vector<MapVector>> routes;
		for (int from = 0; from < ARMS; from++) {
			for (int to = from; to < ARMS; to++) {
				if (!Has(bits, from) || !Has(bits, to) || (from == to && !lone)) continue;
				MapVector a = ARM_WAYS[from];
				MapVector b = ARM_WAYS[to];
				if (from == to) {
					std::array<MapVector, 2> line = {{{0.0, 0.0}, a * EDGE}};
					routes.push_back(Offset(line, offset));
				} else if (to - from == 2) {
					std::array<MapVector, 2> line = {a * EDGE, b * EDGE};
					routes.push_back(Offset(line, offset));
				} else {
					bool wraps = to - from == 3;
					MapVector out = wraps ? b : a;
					MapVector on = wraps ? a : b;
					routes.push_back(Arc((out + on) * EDGE, EDGE + offset, on * -1.0, out * -1.0, BEND_STEPS));
				}
			}
		}
		return routes;
	}

	void Tram()
	{
		if (this->site.road == ROAD_NONE) {
			for (const auto &route : this->TramRoutes(0.0)) this->Lay(Ribbon(route, TRAM_BED_HALF, TRAM_BED_TOP), TRAM_BED);
		}
		if (!this->Full()) return;
		for (double side : {-TRAM_GAUGE_HALF, TRAM_GAUGE_HALF}) {
			for (const auto &route : this->TramRoutes(side)) this->Lay(Ribbon(route, TRAM_RAIL_HALF, TRAM_RAIL_TOP), TRAM_RAIL, STEEL_GLOSS);
		}
	}

	const RoadSite &site;
	ModelMesh &mesh;
	const Footing &footing;
	MapVector origin;
	ModelMesh parts;
};

void LayRoad(ModelMesh &mesh, const RoadSite &site, const Footing &footing)
{
	RoadBuilder(site, mesh, footing).Build();
}
