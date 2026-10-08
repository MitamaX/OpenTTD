/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file track_models.cpp Railway track as 3D pieces: ballast, sleepers and rails, the guideways of monorail and maglev, rails set into a road, and catenary. */

#include "../../stdafx.h"
#include "track_models.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include "../../map_func.h"
#include "../../track_func.h"
#include "../core/seed.h"
#include "../core/tones.h"
#include "../map/tile_shapes.h"
#include "../map/way_course.h"

#include "../../safeguards.h"

static constexpr double ROWS_PER_TILE = 4.0;
static constexpr double MAGLEV_HOVER = 0.024;
static constexpr double LIFT_STEP = 0.0012;
static constexpr double BALLAST_VARIETY = 0.08;
static constexpr double TIMBER_VARIETY = 0.18;
static constexpr uint32_t TRACK_SALT = 0x7AC4B0A2U;

static constexpr uint32_t BALLAST = COL_BALLAST;
static constexpr uint32_t DISTANT_BALLAST = Mix(COL_BALLAST, COL_RAIL, 96);
static constexpr uint32_t TIMBER = 0x5E4B3B;
static constexpr uint32_t RAIL_HEAD = 0xC9CDD1;
static constexpr uint32_t RAIL_WEB = 0x6F6055;
static constexpr uint32_t GUIDEWAY = 0xA9A69E;
static constexpr uint32_t GUIDE_STRIP = 0x7D838A;
static constexpr uint32_t MAST = 0x8A9096;
static constexpr uint32_t WIRE = 0x3B3632;
static constexpr double STEEL_GLOSS = 0.85;
static constexpr double STRIP_GLOSS = 0.6;
static constexpr double MAST_GLOSS = 0.4;

static constexpr double GAUGE_HALF = 0.105;
static constexpr double SLEEPERS_PER_TILE = 7.0;
static constexpr Vec3 SLEEPER_LOW = {-0.02, -0.17, 0.026};
static constexpr Vec3 SLEEPER_HIGH = {0.02, 0.17, 0.047};
static constexpr double STRIP_GAP_HALF = 0.1;

static constexpr double WIRE_HEIGHT = 0.36;
static constexpr double MAST_LATERAL = -0.31;
static constexpr double MAST_HALF = 0.016;
static constexpr double MAST_HEIGHT = 0.46;
static constexpr double ARM_REACH = 0.05;
static constexpr double ARM_LOW = 0.41;
static constexpr double ARM_HIGH = 0.432;
static constexpr double MESSENGER_SAG = 0.025;
static constexpr double MESSENGER_HALF = 0.0035;
static constexpr double MESSENGER_STEPS_PER_TILE = 6.0;
static constexpr double DROPPER_HALF = 0.002;
static constexpr std::array<double, 3> DROPPER_SHARES = {0.25, 0.5, 0.75};
static constexpr double MAST_SPACING = 1.6;

static constexpr std::array<SectionPoint, 4> BALLAST_SECTION = {{
	{-BALLAST_HALF, WAY_FOOT, BALLAST}, {-0.19, 0.034, BALLAST}, {0.19, 0.034, BALLAST}, {BALLAST_HALF, WAY_FOOT, BALLAST},
}};
static constexpr std::array<SectionPoint, 4> DISTANT_BALLAST_SECTION = {{
	{-BALLAST_HALF, WAY_FOOT, DISTANT_BALLAST}, {-0.19, 0.034, DISTANT_BALLAST}, {0.19, 0.034, DISTANT_BALLAST}, {BALLAST_HALF, WAY_FOOT, DISTANT_BALLAST},
}};
static constexpr std::array<SectionPoint, 4> RAIL_SECTION = {{
	{-0.011, 0.046, RAIL_WEB}, {-0.011, RAIL_TOP, RAIL_HEAD, STEEL_GLOSS}, {0.011, RAIL_TOP, RAIL_WEB}, {0.011, 0.046, RAIL_WEB},
}};
static constexpr std::array<SectionPoint, 4> SET_RAIL_SECTION = {{
	{-0.011, 0.006, RAIL_WEB}, {-0.011, 0.017, RAIL_HEAD, STEEL_GLOSS}, {0.011, 0.017, RAIL_WEB}, {0.011, 0.006, RAIL_WEB},
}};
static constexpr std::array<SectionPoint, 6> MONORAIL_SECTION = {{
	{-0.075, WAY_FOOT, GUIDEWAY}, {-0.075, 0.088, GUIDEWAY}, {-0.06, 0.104, GUIDEWAY}, {0.06, 0.104, GUIDEWAY}, {0.075, 0.088, GUIDEWAY}, {0.075, WAY_FOOT, GUIDEWAY},
}};
static constexpr std::array<SectionPoint, 8> MAGLEV_SECTION = {{
	{-0.22, WAY_FOOT, GUIDEWAY}, {-0.22, 0.066, GUIDEWAY}, {-0.185, 0.066, GUIDEWAY}, {-0.185, 0.036, GUIDEWAY},
	{0.185, 0.036, GUIDEWAY}, {0.185, 0.066, GUIDEWAY}, {0.22, 0.066, GUIDEWAY}, {0.22, WAY_FOOT, GUIDEWAY},
}};
static constexpr std::array<SectionPoint, 4> STRIP_SECTION = {{
	{-0.03, 0.036, GUIDE_STRIP}, {-0.03, 0.046, GUIDE_STRIP, STRIP_GLOSS}, {0.03, 0.046, GUIDE_STRIP}, {0.03, 0.036, GUIDE_STRIP},
}};
static constexpr std::array<SectionPoint, 4> WIRE_SECTION = {{
	{-0.006, WIRE_HEIGHT - 0.006, WIRE}, {-0.006, WIRE_HEIGHT, WIRE}, {0.006, WIRE_HEIGHT, WIRE}, {0.006, WIRE_HEIGHT - 0.006, WIRE},
}};
static constexpr std::array<SectionPoint, 4> ARM_SECTION = {{
	{-0.008, ARM_LOW, MAST, MAST_GLOSS}, {-0.008, ARM_HIGH, MAST, MAST_GLOSS}, {0.008, ARM_HIGH, MAST, MAST_GLOSS}, {0.008, ARM_LOW, MAST, MAST_GLOSS},
}};

/* A piece on the ground is laid along the line it is drawn on, at its course's level where it eases, meeting the pieces either side without a seam;
 * on a bridge it runs plainly from edge to edge. */
class TrackPiece {
public:
	TrackPiece(const TrackSite &site, Track track) : site(site), track(track), drawn(Drawn(site, track)) {}

	const WayLine &Line() const { return this->drawn.line; }

	/* Pieces crossing on one tile, and the same piece on the tiles either side, stand a hair apart so their faces never fight; eased pieces never overlap. */
	double Lift() const
	{
		if (this->drawn.course.has_value()) return 0.0;
		int parity = (this->site.tx + this->site.ty) & 1;
		return LIFT_STEP * (static_cast<int>(this->track) + static_cast<int>(TRACK_END) * parity);
	}

	ModelMesh Body(Section section, bool capped) const
	{
		ModelMesh body = Laid(this->Line(), section, this->Rows());
		if (capped && this->drawn.open_first) body.Append(EndCap(this->Line(), section, false));
		if (capped && this->drawn.open_last) body.Append(EndCap(this->Line(), section, true));
		return body;
	}

	ModelMesh Pair(Section section, double gap_half) const
	{
		ModelMesh pair;
		for (double side : {-1.0, 1.0}) {
			std::array<SectionPoint, 4> shifted;
			for (size_t point = 0; point < shifted.size(); point++) {
				shifted[point] = section[point];
				shifted[point].lateral += side * gap_half;
			}
			pair.Append(Laid(this->Line(), shifted, this->Rows()));
		}
		return pair;
	}

	/* Sleepers lie on equal slots along the piece, so the same pitch runs on across every seam. */
	ModelMesh Sleepers() const
	{
		const WayLine &line = this->Line();
		ModelMesh sleepers;
		int count = std::max(1, static_cast<int>(std::lround(line.Span() * SLEEPERS_PER_TILE)));
		SeedDice dice(Hash32(TRACK_SALT + static_cast<uint32_t>(TileXY(this->site.tx, this->site.ty).base() * TRACK_END + this->track)));
		for (int slot = 0; slot < count; slot++) {
			double share = (slot + 0.5) / count;
			ModelMesh sleeper = Block(line.At(share, 0.0), line.Along(share), SLEEPER_LOW, SLEEPER_HIGH);
			sleepers.Append(sleeper.Paint(TIMBER).Vary(TIMBER_VARIETY, dice.Next()));
		}
		return sleepers;
	}

	std::optional<std::pair<double, double>> Distances() const
	{
		if (!this->drawn.course.has_value()) return std::nullopt;
		return this->drawn.course->Distances();
	}

	/* An eased piece is laid at its course's level; any other on what it stands on. */
	Footing FootingOn(const Footing &ground) const
	{
		if (!this->drawn.course.has_value()) return ground;
		return [course = *this->drawn.course](double x, double y) { return course.Level(course.ShareAt({x, y})); };
	}

private:
	static DrawnTrack Drawn(const TrackSite &site, Track track)
	{
		if (site.grounded) return DrawnTrack::Of(site.tx, site.ty, track);
		MapVector origin = {static_cast<double>(site.tx), static_cast<double>(site.ty)};
		auto [from, to] = TRACK_ENDS[track];
		MapVector along = Unit(to - from);
		return {site.tx, site.ty, origin + from, origin + to, Bend(origin + from, origin + to, along, along), std::nullopt, false, false};
	}

	int Rows() const { return std::max(1, static_cast<int>(std::lround(this->Line().Span() * ROWS_PER_TILE))); }

	const TrackSite &site;
	Track track;
	DrawnTrack drawn;
};

static ModelMesh RailPiece(const TrackPiece &piece, WayDetail detail, uint32_t seed)
{
	if (detail == WayDetail::Simple) return piece.Body(DISTANT_BALLAST_SECTION, true);
	ModelMesh mesh = piece.Body(BALLAST_SECTION, true);
	mesh.Vary(BALLAST_VARIETY, seed);
	mesh.Append(piece.Sleepers());
	return mesh.Append(piece.Pair(RAIL_SECTION, GAUGE_HALF));
}

static ModelMesh MaglevPiece(const TrackPiece &piece, WayDetail detail)
{
	ModelMesh mesh = piece.Body(MAGLEV_SECTION, true);
	if (detail == WayDetail::Full) mesh.Append(piece.Pair(STRIP_SECTION, STRIP_GAP_HALF));
	return mesh;
}

/* What a train rides on: the rail heads, the monorail beam's crown, or a hover over the maglev's guide strips. */
double RideHeight(RailLook look)
{
	switch (look) {
		case RailLook::Monorail: return MONORAIL_SECTION[2].height;
		case RailLook::Maglev: return STRIP_SECTION[1].height + MAGLEV_HOVER;
		default: return RAIL_TOP;
	}
}

static ModelMesh PieceModel(const TrackSite &site, const TrackPiece &piece, uint32_t seed)
{
	switch (site.look) {
		case RailLook::Monorail: return piece.Body(MONORAIL_SECTION, true);
		case RailLook::Maglev: return MaglevPiece(piece, site.detail);
		default: return RailPiece(piece, site.detail, seed);
	}
}

static void Lay(ModelMesh &mesh, ModelMesh piece, double lift, const Footing &footing)
{
	piece.Move({0.0, 0.0, lift});
	mesh.Append(Drape(piece, footing));
}

void LayTrack(ModelMesh &mesh, const TrackSite &site, const Footing &footing)
{
	for (Track track : SetTrackBitIterator(site.bits)) {
		TrackPiece piece(site, track);
		uint32_t seed = Hash32(TRACK_SALT ^ static_cast<uint32_t>(site.tx * TRACK_END + track) ^ Hash32(site.ty));
		Lay(mesh, PieceModel(site, piece, seed), piece.Lift(), piece.FootingOn(footing));
	}
}

void LayCrossingRails(ModelMesh &mesh, const TrackSite &site, const Footing &footing)
{
	if (site.detail == WayDetail::Simple) return;
	for (Track track : SetTrackBitIterator(site.bits)) {
		TrackPiece piece(TrackSite{site.tx, site.ty, site.bits, site.look, site.wired, false, site.detail}, track);
		Lay(mesh, piece.Pair(SET_RAIL_SECTION, GAUGE_HALF), 0.0, footing);
	}
}

/* Where along a run a piece's wires hang, and how far apart its masts stand: a piece on an eased run reckons from the run's first end,
 * so masts stand evenly all along the run; any other reckons from its own start, one span to the piece. */
struct WireRun {
	double from;
	double to;
	double spacing;

	/* How far through the span between two masts a share of the piece lies. */
	double SpanShare(double share) const
	{
		double spans = std::lerp(this->from, this->to, share) / this->spacing;
		return spans - std::floor(spans);
	}

	/* The shares of the piece passing a given share of every span, its first end counted and its last left to the piece beyond. */
	std::vector<double> SharesAt(double span_share) const
	{
		std::vector<double> shares;
		double low = std::min(this->from, this->to);
		double high = std::max(this->from, this->to);
		for (double span = std::ceil(low / this->spacing - span_share); (span + span_share) * this->spacing < high; span++) {
			shares.push_back(((span + span_share) * this->spacing - this->from) / (this->to - this->from));
		}
		return shares;
	}
};

static WireRun WiresOf(const TrackPiece &piece)
{
	if (const auto &distances = piece.Distances(); distances.has_value()) return {distances->first, distances->second, MAST_SPACING};
	double span = piece.Line().Span();
	return {0.0, span, span};
}

/* The messenger wire sags from mast to mast, holding the contact wire level on droppers. */
static ModelMesh Messenger(const WayLine &line, const WireRun &wires)
{
	auto at = [&](double share) {
		MapVector point = line.At(share, 0.0);
		double span = wires.SpanShare(share);
		return Vec3{point.x, point.y, ARM_LOW - 4.0 * span * (1.0 - span) * MESSENGER_SAG};
	};
	std::vector<double> shares = wires.SharesAt(0.0);
	int steps = std::max(2, static_cast<int>(std::lround(line.Span() * MESSENGER_STEPS_PER_TILE)));
	for (int step = 0; step <= steps; step++) shares.push_back(static_cast<double>(step) / steps);
	std::ranges::sort(shares);

	ModelMesh messenger;
	for (size_t index = 1; index < shares.size(); index++) messenger.Append(Strut(at(shares[index - 1]), at(shares[index]), MESSENGER_HALF));
	for (double span_share : DROPPER_SHARES) {
		for (double share : wires.SharesAt(span_share)) {
			Vec3 top = at(share);
			messenger.Append(Strut({top.x, top.y, WIRE_HEIGHT}, top, DROPPER_HALF));
		}
	}
	return messenger.Paint(WIRE);
}

/* A mast beside the track with its arm reaching over to hold the wires. */
static ModelMesh Mast(const WayLine &line, double share)
{
	MapVector foot = line.At(share, MAST_LATERAL);
	ModelMesh mast = Block(foot, line.Along(share), {-MAST_HALF, -MAST_HALF, -0.02}, {MAST_HALF, MAST_HALF, MAST_HEIGHT}).Paint(MAST).Gloss(MAST_GLOSS);
	return mast.Append(Laid(Stretch{foot, line.At(share, ARM_REACH)}, ARM_SECTION, 1));
}

/* Masts stand evenly along an eased run; on any other tile one stands at the start of its first piece. */
void LayCatenary(ModelMesh &mesh, const TrackSite &site, const Footing &footing)
{
	if (!site.wired || site.bits == TRACK_BIT_NONE || site.detail == WayDetail::Simple) return;
	bool first = true;
	for (Track track : SetTrackBitIterator(site.bits)) {
		TrackPiece piece(site, track);
		const WayLine &line = piece.Line();
		WireRun wires = WiresOf(piece);
		ModelMesh parts = piece.Body(WIRE_SECTION, false);
		parts.Append(Messenger(line, wires));
		if (piece.Distances().has_value() || first) {
			for (double share : wires.SharesAt(0.0)) parts.Append(Mast(line, share));
		}
		first = false;
		Lay(mesh, std::move(parts), piece.Lift(), piece.FootingOn(footing));
	}
}
