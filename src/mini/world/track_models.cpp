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
#include <cstdint>
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
static constexpr uint32_t WIRE = 0x5C5148;
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
static constexpr double MAST_HALF = 0.016;
static constexpr double MAST_CLEARANCE = BALLAST_HALF + MAST_HALF;
static constexpr double MAST_HEIGHT = 0.46;
static constexpr double ARM_HIGH = 0.432;
static constexpr double MESSENGER_SAG = 0.025;
static constexpr double WIRE_HALF = 0.0045;
static constexpr double MESSENGER_STEPS_PER_TILE = 6.0;
static constexpr double DROPPER_HALF = 0.002;
static constexpr std::array<double, 3> DROPPER_SHARES = {0.25, 0.5, 0.75};
static constexpr double MAST_SPACING = 1.6;
static constexpr double MAST_SAGITTA = 0.08;
static constexpr int MOST_SPACING_HALVINGS = 1;
static constexpr double MAST_APART = MAST_OFFSET;
static constexpr double SUPPORT_MERGE = 1.0e-6;

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

static RunStretch LoneStretch(const Bend &line)
{
	double outer = OuterSide(line.MeanCurvature());
	return {{0.0, outer}, {line.Span(), outer}};
}

static int Halvings(const Bend &line, double base)
{
	double curvature = line.MeanSharpness();
	if (curvature <= 0.0) return 0;
	double chord = std::sqrt(8.0 * MAST_SAGITTA / curvature);
	return std::clamp(static_cast<int>(std::ceil(std::log2(base / chord))), 0, MOST_SPACING_HALVINGS);
}

class WireRun {
public:
	WireRun(const Bend &line, const std::optional<RunStretch> &run) :
		stretch(run.value_or(LoneStretch(line))), base(run.has_value() ? MAST_SPACING : line.Span()), halvings(Halvings(line, this->base)) {}

	std::vector<double> Stations() const
	{
		const auto &[first, last] = this->stretch;
		double finest = std::ldexp(this->base, -MOST_SPACING_HALVINGS);
		int64_t every = int64_t{1} << (MOST_SPACING_HALVINGS - this->halvings);
		double low = std::min(first.distance, last.distance);
		double high = std::max(first.distance, last.distance);
		std::vector<double> shares;
		for (int64_t grid = static_cast<int64_t>(std::ceil(low / finest)); grid * finest < high; grid++) {
			if (grid % every == 0) shares.push_back((grid * finest - first.distance) / (last.distance - first.distance));
		}
		return shares;
	}

	double OuterAt(double share) const
	{
		return share < 0.5 ? this->stretch.first.outer : this->stretch.second.outer;
	}

	double Slack(double low, double high) const
	{
		double reach = std::abs(this->DistanceAt(high) - this->DistanceAt(low)) / this->Spacing();
		return std::min(1.0, reach * reach);
	}

	std::vector<double> SpanPoints(double span_share) const
	{
		const auto &[first, last] = this->stretch;
		double low = std::min(first.distance, last.distance);
		double high = std::max(first.distance, last.distance);
		std::vector<double> shares;
		for (double span = std::floor(low / this->Spacing()); (span + span_share) * this->Spacing() < high; span++) {
			double distance = (span + span_share) * this->Spacing();
			if (distance > low) shares.push_back((distance - first.distance) / (last.distance - first.distance));
		}
		return shares;
	}

private:
	double Spacing() const { return std::ldexp(this->base, -this->halvings); }

	double DistanceAt(double share) const { return std::lerp(this->stretch.first.distance, this->stretch.second.distance, share); }

	RunStretch stretch;
	double base;
	int halvings;
};

class WireSpans {
public:
	WireSpans(const WayLine &line, std::vector<double> supports)
	{
		supports.push_back(0.0);
		supports.push_back(1.0);
		std::ranges::sort(supports);
		for (double share : supports) {
			if (!this->shares.empty() && share - this->shares.back() < SUPPORT_MERGE) continue;
			this->shares.push_back(share);
			this->points.push_back(line.At(share, 0.0));
		}
	}

	Vec3 At(double share, double height) const
	{
		auto [span, along] = this->SpanOf(share);
		MapVector point = this->points[span] + (this->points[span + 1] - this->points[span]) * along;
		return {point.x, point.y, height};
	}

	std::pair<double, double> Around(double share) const
	{
		size_t span = this->SpanOf(share).first;
		return {this->shares[span], this->shares[span + 1]};
	}

	template <typename Height>
	ModelMesh Strung(double steps_per_tile, Height height) const
	{
		ModelMesh wire;
		for (size_t span = 0; span + 1 < this->shares.size(); span++) {
			int steps = std::max(1, static_cast<int>(std::lround(Length(this->points[span + 1] - this->points[span]) * steps_per_tile)));
			for (int step = 0; step < steps; step++) {
				double from = std::lerp(this->shares[span], this->shares[span + 1], static_cast<double>(step) / steps);
				double to = std::lerp(this->shares[span], this->shares[span + 1], static_cast<double>(step + 1) / steps);
				wire.Append(Strut(this->At(from, height(from)), this->At(to, height(to)), WIRE_HALF));
			}
		}
		return wire;
	}

	ModelMesh Taut(double height) const
	{
		return this->Strung(0.0, [height](double) { return height; });
	}

private:
	std::pair<size_t, double> SpanOf(double share) const
	{
		size_t span = std::clamp<size_t>(std::upper_bound(this->shares.begin(), this->shares.end(), share) - this->shares.begin(), 1, this->shares.size() - 1) - 1;
		return {span, (share - this->shares[span]) / (this->shares[span + 1] - this->shares[span])};
	}

	std::vector<double> shares;
	std::vector<MapVector> points;
};

/* The messenger wire sags between its real neighbouring supports, holding the contact wire level on droppers. */
static ModelMesh Messenger(const WireSpans &spans, const WireRun &wires)
{
	auto sagging = [&](double share) {
		auto [low, high] = spans.Around(share);
		double along = (share - low) / (high - low);
		return ARM_LOW - 4.0 * along * (1.0 - along) * MESSENGER_SAG * wires.Slack(low, high);
	};
	ModelMesh messenger = spans.Strung(MESSENGER_STEPS_PER_TILE, sagging);
	for (double span_share : DROPPER_SHARES) {
		for (double share : wires.SpanPoints(span_share)) messenger.Append(Strut(spans.At(share, WIRE_HEIGHT), spans.At(share, sagging(share)), DROPPER_HALF));
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
	return mast.Append(Laid(Stretch{foot, line.At(share, 0.0)}, ARM_SECTION, 1));
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

static std::optional<double> MastSide(const Bend &line, double outer, double share, bool clearing, const MastBan &banned, std::span<const MapVector> standing)
{
	std::array<double, 2> order = {outer, -outer};
	for (double side : std::span(order).first(clearing ? order.size() : 1)) {
		MapVector foot = MastFoot(line, share, side);
		if (clearing && TrackGap(foot) < MAST_CLEARANCE) continue;
		if (banned && banned(foot)) continue;
		if (std::ranges::any_of(standing, [&](const MapVector &other) { return Length(other - foot) < MAST_APART; })) continue;
		return side;
	}
	return std::nullopt;
}

void LayCatenary(ModelMesh &mesh, const TileTrack &track, const Footing &footing, const MastBan &banned)
{
	const TrackSite &site = track.Site();
	if (!site.wired || site.bits == TRACK_BIT_NONE || site.detail == WayDetail::Simple) return;
	const std::vector<TrackPiece> &pieces = track.Pieces();
	std::vector<MapVector> standing;
	for (size_t rank = 0; rank < pieces.size(); rank++) {
		const TrackPiece &piece = pieces[rank];
		const Bend &line = piece.Line();
		WireRun wires(line, piece.Stretch());
		std::vector<double> stations = wires.Stations();
		WireSpans spans(line, stations);
		ModelMesh parts = spans.Taut(WIRE_HEIGHT).Paint(WIRE);
		if (!track.Branch(rank)) parts.Append(Messenger(spans, wires));
		if (piece.Stretch().has_value() || rank == MAIN_LINE) {
			for (double share : stations) {
				std::optional<double> side = MastSide(line, wires.OuterAt(share), share, site.grounded, banned, standing);
				if (!side.has_value()) continue;
				standing.push_back(MastFoot(line, share, *side));
				parts.Append(Mast(line, share, *side));
			}
		}
		Lay(mesh, std::move(parts), track.Lift(piece), piece.FootingOn(footing));
	}
}
