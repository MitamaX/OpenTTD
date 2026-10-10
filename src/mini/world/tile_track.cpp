/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tile_track.cpp A tile's rail pieces taken together, ranked from the tile's main line down: where each is laid, how high, and where its sleepers lie. */

#include "../../stdafx.h"
#include "tile_track.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <tuple>

#include "../../core/bitmath_func.hpp"
#include "../../map_func.h"
#include "../../track_func.h"
#include "../core/seed.h"
#include "../map/track_pieces.h"

#include "../../safeguards.h"

static constexpr double ROWS_PER_TILE = 4.0;
/* A piece's line turns no further than this from one row of its faces to the next, in radians, so its bends read as curves. */
static constexpr double ROW_TURN_LIMIT = 0.08;
static constexpr int TURN_SAMPLES = 8;
static constexpr double LIFT_STEP = 0.0012;
static constexpr uint32_t TRACK_SALT = 0x7AC4B0A2U;
static constexpr double SLEEPERS_PER_TILE = 7.0;
static constexpr double SLEEPER_GAP = 0.03;

static DrawnTrack Drawn(int tx, int ty, Track track, bool grounded)
{
	if (grounded) return DrawnTrack::Of(tx, ty, track);
	MapVector origin = {static_cast<double>(tx), static_cast<double>(ty)};
	auto [from, to] = TRACK_ENDS[track];
	MapVector along = Unit(to - from);
	return {tx, ty, origin + from, origin + to, Bend(origin + from, origin + to, along, along), std::nullopt, false, false};
}

/* How far a line turns along its length, turns either way both counted. */
static double Turn(const WayLine &line)
{
	double turn = 0.0;
	MapVector previous = line.Along(0.0);
	for (int sample = 1; sample <= TURN_SAMPLES; sample++) {
		MapVector next = line.Along(static_cast<double>(sample) / TURN_SAMPLES);
		turn += std::acos(std::clamp(Dot(previous, next), -1.0, 1.0));
		previous = next;
	}
	return turn;
}

static int RowsAlong(const WayLine &line)
{
	int along = static_cast<int>(std::lround(line.Span() * ROWS_PER_TILE));
	int turning = static_cast<int>(std::ceil(Turn(line) / ROW_TURN_LIMIT));
	return std::max({1, along, turning});
}

TrackPiece::TrackPiece(int tx, int ty, Track track, bool grounded) : track(track), drawn(Drawn(tx, ty, track, grounded)), rows(RowsAlong(this->drawn.line))
{
}

ModelMesh TrackPiece::Body(Section section, bool capped) const
{
	ModelMesh body = Laid(this->Line(), section, this->rows);
	if (capped && this->drawn.open_first) body.Append(EndCap(this->Line(), section, false));
	if (capped && this->drawn.open_last) body.Append(EndCap(this->Line(), section, true));
	return body;
}

std::optional<std::pair<double, double>> TrackPiece::Distances() const
{
	if (!this->drawn.course.has_value()) return std::nullopt;
	return this->drawn.course->Distances();
}

/* An eased piece is laid at its course's level; any other on what it stands on. */
Footing TrackPiece::FootingOn(const Footing &ground) const
{
	if (!this->drawn.course.has_value()) return ground;
	return [course = *this->drawn.course](double x, double y) { return course.Level(course.ShareAt({x, y})); };
}

struct Plank {
	MapVector middle;
	MapVector length;
	double half_length;
	double half_width;

	static Plank Of(const WayLine &line, const Sleeper &sleeper)
	{
		return {line.At(sleeper.share, sleeper.Middle()), RightOf(line.Along(sleeper.share)), sleeper.HalfLength(), SLEEPER_HALF_WIDTH + SLEEPER_GAP / 2.0};
	}

	static double Reach(const Plank &plank, const MapVector &axis)
	{
		return std::abs(Dot(plank.length, axis)) * plank.half_length + std::abs(Dot(RightOf(plank.length), axis)) * plank.half_width;
	}

	bool Overlaps(const Plank &other) const
	{
		MapVector between = other.middle - this->middle;
		for (const MapVector &axis : {this->length, RightOf(this->length), other.length, RightOf(other.length)}) {
			if (std::abs(Dot(between, axis)) > Reach(*this, axis) + Reach(other, axis)) return false;
		}
		return true;
	}
};

using TrackRank = std::tuple<int, bool, int>;

static TrackRank RankOf(int tx, int ty, Track track)
{
	return {StraightOnEnds(tx, ty, track), IsDiagonalTrack(track), -static_cast<int>(track)};
}

TileTrack::TileTrack(const TrackSite &site) : site(site)
{
	std::vector<std::pair<TrackRank, Track>> ranked;
	for (Track track : SetTrackBitIterator(site.bits)) ranked.emplace_back(HasAtMostOneBit(site.bits) ? TrackRank{} : RankOf(site.tx, site.ty, track), track);
	std::ranges::sort(ranked, std::greater{});
	for (const auto &[rank, track] : ranked) this->pieces.emplace_back(site.tx, site.ty, track, site.grounded);
}

/* Pieces crossing on one tile, and the same piece on the tiles either side, stand a hair apart so their faces never fight; eased pieces never overlap. */
double TileTrack::Lift(const TrackPiece &piece) const
{
	if (piece.Eased()) return 0.0;
	int parity = (this->site.tx + this->site.ty) & 1;
	return LIFT_STEP * (static_cast<int>(piece.Id()) + static_cast<int>(TRACK_END) * parity);
}

uint32_t TileTrack::BallastSeed(const TrackPiece &piece) const
{
	return Hash32(TRACK_SALT ^ static_cast<uint32_t>(this->site.tx * TRACK_END + piece.Id()) ^ Hash32(this->site.ty));
}

std::vector<std::vector<Sleeper>> TileTrack::Sleepers() const
{
	std::vector<std::vector<Sleeper>> laid(this->pieces.size());
	if (this->site.look != RailLook::Rail || this->site.detail != WayDetail::Full) return laid;
	std::vector<Plank> above;
	for (size_t rank = 0; rank < this->pieces.size(); rank++) {
		const WayLine &line = this->pieces[rank].Line();
		std::vector<Plank> planks;
		int count = std::max(1, static_cast<int>(std::lround(line.Span() * SLEEPERS_PER_TILE)));
		SeedDice dice(Hash32(TRACK_SALT + static_cast<uint32_t>(TileXY(this->site.tx, this->site.ty).base() * TRACK_END + this->pieces[rank].Id())));
		for (int slot = 0; slot < count; slot++) {
			Sleeper sleeper = {(slot + 0.5) / count, -SLEEPER_HALF_LENGTH, SLEEPER_HALF_LENGTH, dice.Next()};
			Plank plank = Plank::Of(line, sleeper);
			if (std::ranges::any_of(above, [&](const Plank &placed) { return placed.Overlaps(plank); })) continue;
			laid[rank].push_back(sleeper);
			planks.push_back(plank);
		}
		above.insert(above.end(), planks.begin(), planks.end());
	}
	return laid;
}
