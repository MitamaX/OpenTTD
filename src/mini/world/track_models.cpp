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
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "../../track_func.h"
#include "../core/tones.h"
#include "../map/tile_shapes.h"
#include "../map/track_pieces.h"
#include "../map/way_course.h"

#include "../../safeguards.h"

static constexpr double MAGLEV_HOVER = 0.024;
static constexpr double BALLAST_VARIETY = 0.08;
static constexpr double TIMBER_VARIETY = 0.18;

static constexpr uint32_t BALLAST = COL_BALLAST;
static constexpr uint32_t DISTANT_BALLAST = COL_DISTANT_BALLAST;
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

static constexpr double STRIP_GAP_HALF = 0.1;

static constexpr double MACHINE_REACH = 0.08;
static constexpr double MACHINE_OFFSET = 0.25;
static constexpr Vec3 MACHINE_LOW = {-0.05, -0.025, 0.0};
static constexpr Vec3 MACHINE_HIGH = {0.05, 0.025, 0.065};
static constexpr std::array<double, 2> ROD_REACHES = {0.05, 0.11};
static constexpr double ROD_OVERHANG = 0.02;
static constexpr double ROD_HEIGHT = 0.05;
static constexpr double ROD_HALF = 0.006;
static constexpr double FROG_HALF_LENGTH = 0.045;
static constexpr double FROG_HALF_WIDTH = 0.016;
static constexpr double FROG_PROUD = 0.001;
static constexpr double CHECK_HALF = 0.08;
static constexpr int CHECK_ROWS = 2;
static constexpr double CHECK_INSET = 0.03;
static constexpr double CHECK_LATERAL = GAUGE_HALF - CHECK_INSET;

static constexpr double WIRE_HEIGHT = 0.36;
static constexpr double MAST_OFFSET = 0.31;
static constexpr std::array<double, 2> MAST_SIDES = {-1.0, 1.0};
static constexpr double MAST_HALF = 0.016;
static constexpr double MAST_CLEARANCE = BALLAST_HALF + MAST_HALF;
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

static constexpr double CROWN_HALF = 0.19;
static constexpr double CROWN_TOP = 0.034;
static constexpr double SHOULDER_HALF = 0.225;
static constexpr double SHOULDER_TOP = 0.02;

static constexpr std::array<SectionPoint, 6> BallastSection(uint32_t tone)
{
	return {{
		{-BALLAST_HALF, WAY_FOOT, tone, 0.0, WayBed::Shoulder}, {-SHOULDER_HALF, SHOULDER_TOP, tone, 0.0, WayBed::Shoulder}, {-CROWN_HALF, CROWN_TOP, tone, 0.0, WayBed::Ballast},
		{CROWN_HALF, CROWN_TOP, tone, 0.0, WayBed::Shoulder}, {SHOULDER_HALF, SHOULDER_TOP, tone, 0.0, WayBed::Shoulder}, {BALLAST_HALF, WAY_FOOT, tone, 0.0, WayBed::Shoulder},
	}};
}

static constexpr std::array<SectionPoint, 6> BALLAST_SECTION = BallastSection(BALLAST);
static constexpr std::array<SectionPoint, 6> DISTANT_BALLAST_SECTION = BallastSection(DISTANT_BALLAST);
static constexpr std::array<SectionPoint, 4> RAIL_SECTION = {{
	{-RAIL_HALF, 0.046, RAIL_WEB}, {-RAIL_HALF, RAIL_TOP, RAIL_HEAD, STEEL_GLOSS}, {RAIL_HALF, RAIL_TOP, RAIL_WEB}, {RAIL_HALF, 0.046, RAIL_WEB},
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

static ModelMesh SleeperModel(const WayLine &line, const Sleeper &sleeper)
{
	double half_length = sleeper.HalfLength();
	MapVector middle = line.At(sleeper.share, sleeper.Middle());
	ModelMesh timber = Block(middle, line.Along(sleeper.share), {-SLEEPER_HALF_WIDTH, -half_length, SLEEPER_FOOT}, {SLEEPER_HALF_WIDTH, half_length, SLEEPER_TOP});
	return timber.Paint(TIMBER).Vary(TIMBER_VARIETY, sleeper.seed);
}

static Vec3 Raised(const MapVector &point, double height)
{
	return {point.x, point.y, height};
}

static Portion Around(const WayLine &line, double share, double reach)
{
	double half = reach / line.Span();
	return {line, std::max(share - half, 0.0), std::min(share + half, 1.0)};
}

static ModelMesh TurnoutGear(const TrackPiece &main, const TrackPiece &branch, const Turnout &turnout)
{
	const WayLine &line = main.Line();
	auto from_toe = [&](double reach) { return turnout.main_toe + Turnout::Inward(turnout.main_toe) * reach / line.Span(); };
	double machine = from_toe(MACHINE_REACH);
	ModelMesh steel = Block(line.At(machine, -turnout.side * MACHINE_OFFSET), line.Along(machine), MACHINE_LOW, MACHINE_HIGH);
	for (double reach : ROD_REACHES) {
		double share = from_toe(reach);
		steel.Append(Strut(Raised(line.At(share, -turnout.side * MACHINE_OFFSET), ROD_HEIGHT), Raised(line.At(share, turnout.side * (GAUGE_HALF + ROD_OVERHANG)), ROD_HEIGHT), ROD_HALF));
	}
	steel.Paint(MAST).Gloss(MAST_GLOSS);
	if (!turnout.frog.has_value()) return steel;
	const Frog &frog = *turnout.frog;
	ModelMesh nose = Block(frog.point, frog.heading, {-FROG_HALF_LENGTH, -FROG_HALF_WIDTH, SLEEPER_TOP}, {FROG_HALF_LENGTH, FROG_HALF_WIDTH, RAIL_TOP + FROG_PROUD});
	steel.Append(nose.Paint(RAIL_HEAD).Gloss(STEEL_GLOSS));
	steel.Append(Laid(Around(line, frog.main_share, CHECK_HALF), Shifted(RAIL_SECTION, -turnout.side * CHECK_LATERAL), CHECK_ROWS));
	return steel.Append(Laid(Around(branch.Line(), frog.branch_share, CHECK_HALF), Shifted(RAIL_SECTION, turnout.branch_side * CHECK_LATERAL), CHECK_ROWS));
}

static ModelMesh RailPiece(const TileTrack &track, size_t rank, std::span<const Sleeper> sleepers)
{
	const TrackPiece &piece = track.Pieces()[rank];
	if (track.Site().detail == WayDetail::Simple) return piece.Body(DISTANT_BALLAST_SECTION, true);
	ModelMesh mesh = piece.Body(BALLAST_SECTION, true);
	if (std::optional<uint32_t> seed = track.BallastSeed(piece); seed.has_value()) mesh.Vary(BALLAST_VARIETY, *seed);
	for (const Sleeper &sleeper : sleepers) mesh.Append(SleeperModel(piece.Line(), sleeper));
	mesh.Append(piece.Pair(RAIL_SECTION, GAUGE_HALF));
	if (rank != MAIN_LINE) return mesh;
	for (const Turnout &turnout : track.Turnouts()) {
		if (turnout.main == MAIN_LINE) mesh.Append(TurnoutGear(piece, track.Pieces()[turnout.branch], turnout));
	}
	return mesh;
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

static ModelMesh PieceModel(const TileTrack &track, size_t rank, std::span<const Sleeper> sleepers)
{
	const TrackPiece &piece = track.Pieces()[rank];
	switch (track.Site().look) {
		case RailLook::Monorail: return piece.Body(MONORAIL_SECTION, true);
		case RailLook::Maglev: return MaglevPiece(piece, track.Site().detail);
		default: return RailPiece(track, rank, sleepers);
	}
}

static void Lay(ModelMesh &mesh, ModelMesh piece, double lift, const Footing &footing)
{
	piece.Move({0.0, 0.0, lift});
	mesh.Append(Drape(piece, footing));
}

void LayTrack(ModelMesh &mesh, const TileTrack &track, const Footing &footing)
{
	const std::vector<TrackPiece> &pieces = track.Pieces();
	std::vector<std::vector<Sleeper>> sleepers = track.Sleepers();
	for (size_t rank = 0; rank < pieces.size(); rank++) {
		const TrackPiece &piece = pieces[rank];
		Lay(mesh, PieceModel(track, rank, sleepers[rank]), track.Lift(piece), piece.FootingOn(footing));
	}
}

/* Where along a run a piece's wires hang, and how far apart its masts stand. */
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

static MapVector MastFoot(const WayLine &line, double share, double side)
{
	return line.At(share, side * MAST_OFFSET);
}

/* A mast beside the track with its arm reaching over to hold the wires. */
static ModelMesh Mast(const WayLine &line, double share, double side)
{
	MapVector foot = MastFoot(line, share, side);
	ModelMesh mast = Block(foot, line.Along(share), {-MAST_HALF, -MAST_HALF, -0.02}, {MAST_HALF, MAST_HALF, MAST_HEIGHT}).Paint(MAST).Gloss(MAST_GLOSS);
	return mast.Append(Laid(Stretch{foot, line.At(share, -side * ARM_REACH)}, ARM_SECTION, 1));
}

static double TrackGap(const MapVector &foot)
{
	auto [cx, cy] = TileOf(foot);
	double gap = std::numeric_limits<double>::infinity();
	for (int ty = cy - 1; ty <= cy + 1; ty++) {
		for (int tx = cx - 1; tx <= cx + 1; tx++) {
			if (!OnMap(tx, ty)) continue;
			for (Track track : SetTrackBitIterator(TrackBitsOf(tx, ty))) {
				const Bend &line = DrawnTrack::Of(tx, ty, track).line;
				gap = std::min(gap, Length(foot - line.Centre(line.ShareOf(foot))));
			}
		}
	}
	return gap;
}

static std::optional<double> MastSide(const WayLine &line, double share, bool clearing, const MastBan &banned)
{
	std::span<const double> sides = clearing ? std::span(MAST_SIDES) : std::span(MAST_SIDES).first(1);
	for (double side : sides) {
		MapVector foot = MastFoot(line, share, side);
		if ((!clearing || TrackGap(foot) >= MAST_CLEARANCE) && !(banned && banned(foot))) return side;
	}
	return std::nullopt;
}

void LayCatenary(ModelMesh &mesh, const TileTrack &track, const Footing &footing, const MastBan &banned)
{
	const TrackSite &site = track.Site();
	if (!site.wired || site.bits == TRACK_BIT_NONE || site.detail == WayDetail::Simple) return;
	const std::vector<TrackPiece> &pieces = track.Pieces();
	bool run = std::ranges::any_of(pieces, [](const TrackPiece &piece) { return piece.Distances().has_value(); });
	for (size_t rank = 0; rank < pieces.size(); rank++) {
		const TrackPiece &piece = pieces[rank];
		const WayLine &line = piece.Line();
		WireRun wires = WiresOf(piece);
		ModelMesh parts = piece.Body(WIRE_SECTION, false);
		if (!track.Branch(rank)) parts.Append(Messenger(line, wires));
		if (piece.Distances().has_value() || rank == MAIN_LINE) {
			for (double share : wires.SharesAt(0.0)) {
				std::optional<double> side = MastSide(line, share, site.grounded && !run, banned);
				if (side.has_value()) parts.Append(Mast(line, share, *side));
			}
		}
		Lay(mesh, std::move(parts), track.Lift(piece), piece.FootingOn(footing));
	}
}
