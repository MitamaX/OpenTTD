/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file track_models.cpp Railway track as 3D pieces: ballast, sleepers and rails, the guideways of monorail and maglev, rails set into a road, and catenary. */

#include "../../stdafx.h"
#include "track_models.h"

#include <array>
#include <cmath>
#include <optional>
#include <utility>

#include "../../map_func.h"
#include "../../track_func.h"
#include "../core/seed.h"
#include "../core/tones.h"
#include "../map/tile_shapes.h"
#include "../map/way_profile.h"

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
static constexpr double DROPPER_HALF = 0.002;
static constexpr std::array<double, 3> DROPPER_SHARES = {0.25, 0.5, 0.75};

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

/* How a piece's end meets the track beyond: the direction that track runs on in when one piece carries it on, and whether none does. */
struct PieceEnd {
	MapVector beyond{};
	bool open = false;
};

/* A piece's ends only meet pieces of the tile across whose ground stands level with this one's at the joint. */
static PieceEnd EndAt(int tx, int ty, const MapVector &joint, const MapVector &arrival)
{
	MapVector probe = joint + arrival * HALF_TILE;
	int nx = static_cast<int>(std::floor(probe.x));
	int ny = static_cast<int>(std::floor(probe.y));
	if (!OnMap(nx, ny) || TileGround(tx, ty).Level(joint.x, joint.y) != TileGround(nx, ny).Level(joint.x, joint.y)) return {{}, true};

	PieceEnd end{{}, true};
	int partners = 0;
	MapVector origin = {static_cast<double>(nx), static_cast<double>(ny)};
	for (Track track : SetTrackBitIterator(static_cast<TrackBits>(_world_tiles.NetworkAt(TileXY(nx, ny)).track))) {
		auto [from, to] = TRACK_ENDS[track];
		from = origin + from;
		to = origin + to;
		bool starts = from.x == joint.x && from.y == joint.y;
		bool ends = to.x == joint.x && to.y == joint.y;
		if (!starts && !ends) continue;
		end.beyond = Unit(starts ? to - from : from - to);
		end.open = false;
		partners++;
	}
	if (partners > 1) end.beyond = {};
	return end;
}

/* A piece laid on the ground and the pieces beyond it decide how its ends are cut, and an eased run carries it through its joints at one level;
 * on a bridge it runs plainly from edge to edge. */
class TrackPiece {
public:
	TrackPiece(const TrackSite &site, Track track) : site(site), track(track)
	{
		MapVector origin = {static_cast<double>(site.tx), static_cast<double>(site.ty)};
		auto [from, to] = TRACK_ENDS[track];
		this->stretch = {origin + from, origin + to};
		if (!site.grounded) return;

		this->course = WayCourse::OfTrack(site.tx, site.ty, track);
		MapVector along = this->stretch.Along();
		PieceEnd tail = EndAt(site.tx, site.ty, this->stretch.from, along * -1.0);
		PieceEnd head = EndAt(site.tx, site.ty, this->stretch.to, along);
		if (this->course.has_value() && Heads(this->course->Before())) tail = {this->course->Before() * -1.0, false};
		if (this->course.has_value() && Heads(this->course->After())) head = {this->course->After(), false};
		this->stretch.before = tail.beyond * -1.0;
		this->stretch.after = head.beyond;
		this->open_from = tail.open;
		this->open_to = head.open;
	}

	/* Pieces crossing on one tile, and the same piece on the tiles either side, stand a hair apart so their faces never fight. */
	double Lift() const
	{
		int parity = (this->site.tx + this->site.ty) & 1;
		return LIFT_STEP * (static_cast<int>(this->track) + static_cast<int>(TRACK_END) * parity);
	}

	ModelMesh Body(Section section, bool capped) const
	{
		ModelMesh body = Laid(this->stretch, section, this->Rows());
		if (capped && this->open_from) body.Append(EndCap(this->stretch, section, false));
		if (capped && this->open_to) body.Append(EndCap(this->stretch, section, true));
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
			pair.Append(Laid(this->stretch, shifted, this->Rows()));
		}
		return pair;
	}

	/* Sleepers lie on equal slots along the piece, so the same pitch runs on across every seam. */
	ModelMesh Sleepers() const
	{
		ModelMesh sleepers;
		double span = this->stretch.Span();
		int count = std::max(1, static_cast<int>(std::lround(span * SLEEPERS_PER_TILE)));
		SeedDice dice(Hash32(TRACK_SALT + static_cast<uint32_t>(TileXY(this->site.tx, this->site.ty).base() * TRACK_END + this->track)));
		for (int slot = 0; slot < count; slot++) {
			double share = (slot + 0.5) / count;
			ModelMesh sleeper = Block(this->stretch.At(share, 0.0), this->stretch.Along(), SLEEPER_LOW, SLEEPER_HIGH);
			sleepers.Append(sleeper.Paint(TIMBER).Vary(TIMBER_VARIETY, dice.Next()));
		}
		return sleepers;
	}

	const Stretch &Run() const { return this->stretch; }

	/* An eased piece is laid at its course's level, read along its own cuts; any other on what it stands on. */
	Footing FootingOn(const Footing &ground) const
	{
		if (!this->course.has_value()) return ground;
		return [course = *this->course, stretch = this->stretch](double x, double y) { return course.Level(stretch.ShareOf({x, y})); };
	}

private:
	int Rows() const { return std::max(1, static_cast<int>(std::lround(this->stretch.Span() * ROWS_PER_TILE))); }

	const TrackSite &site;
	Track track;
	Stretch stretch{};
	std::optional<WayCourse> course;
	bool open_from = false;
	bool open_to = false;
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
	piece.Transform(Mat4::Translation({0.0, 0.0, lift}));
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

/* The messenger wire sags from arm to arm over each piece, holding the contact wire level on droppers. */
static ModelMesh Messenger(const Stretch &run)
{
	auto at = [&run](double share) {
		MapVector point = run.At(share, 0.0);
		double sag = 4.0 * share * (1.0 - share) * MESSENGER_SAG;
		return Vec3{point.x, point.y, ARM_LOW - sag};
	};
	ModelMesh wires = Strut(at(0.0), at(HALF_TILE), MESSENGER_HALF);
	wires.Append(Strut(at(HALF_TILE), at(1.0), MESSENGER_HALF));
	for (double share : DROPPER_SHARES) {
		Vec3 top = at(share);
		wires.Append(Strut({top.x, top.y, WIRE_HEIGHT}, top, DROPPER_HALF));
	}
	return wires.Paint(WIRE);
}

/* One mast stands at the start of a tile's first piece, its arm reaching over the track to hold the wires every piece hangs. */
void LayCatenary(ModelMesh &mesh, const TrackSite &site, const Footing &footing)
{
	if (!site.wired || site.bits == TRACK_BIT_NONE || site.detail == WayDetail::Simple) return;
	bool first = true;
	for (Track track : SetTrackBitIterator(site.bits)) {
		TrackPiece piece(site, track);
		ModelMesh parts = piece.Body(WIRE_SECTION, false);
		parts.Append(Messenger(piece.Run()));
		if (first) {
			const Stretch &run = piece.Run();
			MapVector foot = run.At(0.0, MAST_LATERAL);
			parts.Append(Block(foot, run.Along(), {-MAST_HALF, -MAST_HALF, -0.02}, {MAST_HALF, MAST_HALF, MAST_HEIGHT}).Paint(MAST).Gloss(MAST_GLOSS));
			Stretch arm = {foot, run.At(0.0, ARM_REACH)};
			parts.Append(Laid(arm, ARM_SECTION, 1));
			first = false;
		}
		Lay(mesh, std::move(parts), piece.Lift(), piece.FootingOn(footing));
	}
}
