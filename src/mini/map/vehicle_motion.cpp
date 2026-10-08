/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_motion.cpp Smooth vehicle positions between game ticks. */

#include "../../stdafx.h"
#include "vehicle_motion.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../../bridge_map.h"
#include "../../map_func.h"
#include "../../pbs.h"
#include "../../tile_map.h"
#include "../../timer/timer_game_tick.h"
#include "../../train.h"
#include "../../vehicle_base.h"
#include "tile_shapes.h"
#include "track_pieces.h"
#include "train_route.h"
#include "way_course.h"

#include "../../safeguards.h"

static constexpr uint64_t PRUNE_INTERVAL_MASK = 0xFF;
static constexpr uint64_t STALE_FRAMES = PRUNE_INTERVAL_MASK + 1;
static constexpr double MAX_GLIDE_DISTANCE = 2 * TILE_SIZE;
static constexpr double MAX_STRIDE_PER_TICK = TILE_SIZE;
static constexpr double AIRCRAFT_STEP_PROGRESS = 256.0;
static constexpr double MAX_STEP_SHARE = 1.0;
static constexpr double TURN_EASE_MS = 110.0;
static constexpr double SNAP_TURN = 0.75 * std::numbers::pi;
static constexpr double MS_PER_SECOND = 1000.0;

VehicleMotion _vehicle_motion;

/* Inside a tunnel or on a bridge the game keeps one of its ends as the vehicle's tile. */
static bool RidesDrawnGround(const Vehicle *v)
{
	return v->IsGroundVehicle() && !IsTileType(v->tile, MP_TUNNELBRIDGE);
}

/* A unit is shown a little short of where the game has it, so it may still stand on the ramp of a bridge's head the game has it past. */
static bool OnBridgeHead(const WorldPoint &point)
{
	auto [tx, ty] = TileOf(Flat(point));
	return OnMap(tx, ty) && IsBridgeTile(TileXY(tx, ty));
}

static MapVector FacingOf(Direction direction)
{
	TileIndexDiffC step = TileIndexDiffCByDir(direction);
	return Unit({static_cast<double>(step.x), static_cast<double>(step.y)});
}

/* The bearing a direction points along, from map X toward map Y. */
static double BearingOf(Direction direction)
{
	MapVector facing = FacingOf(direction);
	return std::atan2(facing.y, facing.x);
}

/* The game turns a train about by swapping its units' places end for end, and marks each turn on its front. */
static bool TurnedAbout(const Vehicle *v)
{
	return v->type == VEH_TRAIN && Train::From(v->First())->flags.Test(VehicleRailFlag::Reversed);
}

static WorldPoint InTiles(const TickTrail::Spot &spot)
{
	return {spot.x / TILE_SIZE, spot.y / TILE_SIZE, spot.z / TILE_HEIGHT};
}

/* The game moves a unit a whole step at a time once its front has gathered the progress a step takes; the share gathered toward the next step. */
static double StepShare(const Vehicle *front)
{
	if (front->cur_speed == 0) return 0.0;
	double needed = front->type == VEH_AIRCRAFT ? AIRCRAFT_STEP_PROGRESS : front->GetAdvanceDistance();
	return std::min(front->progress / needed, MAX_STEP_SHARE);
}

/* The smaller signed angle turning from one bearing to another. */
static double TurnBetween(double from, double to)
{
	return std::remainder(to - from, 2.0 * std::numbers::pi);
}

void VehicleMotion::Advance(double delta_ms)
{
	this->frame_ms = delta_ms;
	this->tick = TimerGameTick::counter;
	if ((++this->frames & PRUNE_INTERVAL_MASK) == 0) {
		std::erase_if(this->snapshots, [this](const auto &entry) { return std::max(entry.second.seen, entry.second.turned) + STALE_FRAMES < this->frames; });
	}
}

/* A unit stands where its trail passes the tick the clock shows, a train along its track; one new to view, jumping or turned about starts its trail afresh. */
WorldPoint VehicleMotion::Gliding(const Vehicle *v)
{
	Snapshot &e = this->snapshots[v->index.base()];
	TickTrail::Spot spot = SpotOf(v, this->tick);
	bool turned_about = TurnedAbout(v);
	bool continues = e.seen + 1 >= this->frames && !e.trail.Empty() && e.turned_about == turned_about && Glides(e.trail.Latest(), spot);
	e.trail.Note(spot, continues);
	if (v->type == VEH_TRAIN) e.Pass({v->tile, Train::From(v)->track & TRACK_BIT_MASK}, continues);
	e.seen = this->frames;
	e.turned_about = turned_about;

	TickTrail::Bracket bracket = e.trail.About(_tick_clock.Shown());
	WorldPoint from = InTiles(bracket.from);
	WorldPoint to = InTiles(bracket.to);
	if (!FollowsTrack(v)) return Between(from, to, bracket.share);
	MapVector at = TrackRoute(Flat(from), Flat(to), this->TakenBy(v)).At(bracket.share);
	return {at.x, at.y, std::lerp(from.level, to.level, bracket.share)};
}

WorldPoint VehicleMotion::Position(const Vehicle *v)
{
	return Grounded(v, this->Gliding(v));
}

WorldPoint VehicleMotion::Onward(const Vehicle *v, const WorldPoint &from, double reach)
{
	if (!FollowsTrack(v)) {
		double bearing = this->Bearing(v);
		return {from.x + std::cos(bearing) * reach, from.y + std::sin(bearing) * reach, from.level};
	}
	MapVector facing = FacingOf(v->direction);
	MapVector at = TrainAhead(Flat(from), reach < 0.0 ? facing * -1.0 : facing, std::abs(reach), this->TakenBy(v));
	return {at.x, at.y, from.level};
}

/* A unit eases toward the way it faces once a frame; a turn of more than three quarters of a half turn is a reversal and snaps,
 * and a unit not turned in the frame before, new or out of sight until now, faces its way at once. */
VehicleMotion::Snapshot &VehicleMotion::Turned(const Vehicle *v)
{
	Snapshot &e = this->snapshots[v->index.base()];
	double target = BearingOf(v->direction);
	if (e.turned == 0 || e.turned + 1 < this->frames) {
		e.bearing = target;
		e.turn_rate = 0.0;
	} else if (e.turned != this->frames) {
		double turn = TurnBetween(e.bearing, target);
		double eased = std::abs(turn) > SNAP_TURN ? turn : turn * (1.0 - std::exp(-this->frame_ms / TURN_EASE_MS));
		e.bearing += eased;
		e.turn_rate = std::abs(turn) > SNAP_TURN || this->frame_ms <= 0.0 ? 0.0 : eased * MS_PER_SECOND / this->frame_ms;
	}
	e.turned = this->frames;
	return e;
}

double VehicleMotion::Bearing(const Vehicle *v)
{
	return this->Turned(v).bearing;
}

double VehicleMotion::TurnRate(const Vehicle *v)
{
	return this->Turned(v).turn_rate;
}

TrackTaken VehicleMotion::TakenBy(const Vehicle *v) const
{
	return [this, v](TileIndex tile) {
		TrackBits taken = GetReservedTrackbits(tile);
		for (const Train *unit = Train::From(v->First()); unit != nullptr; unit = unit->Next()) {
			if (unit->tile == tile) taken |= unit->track & TRACK_BIT_MASK;
			auto known = this->snapshots.find(unit->index.base());
			if (known != this->snapshots.end() && known->second.left.tile == tile) taken |= known->second.left.track;
		}
		return taken;
	};
}

bool VehicleMotion::FollowsTrack(const Vehicle *v)
{
	return v->type == VEH_TRAIN && RidesDrawnGround(v);
}

WorldPoint VehicleMotion::Grounded(const Vehicle *v, const WorldPoint &point)
{
	if (!RidesDrawnGround(v) || OnBridgeHead(point)) return point;
	return v->type == VEH_TRAIN ? TrainPoint(point.x, point.y) : RoadPoint(point.x, point.y);
}

/* A unit stands that share of its next step further along the way it faces. */
TickTrail::Spot VehicleMotion::SpotOf(const Vehicle *v, uint64_t tick)
{
	TileIndexDiffC step = TileIndexDiffCByDir(v->direction);
	double share = StepShare(v->First());
	return {tick, v->x_pos + step.x * share, v->y_pos + step.y * share, static_cast<double>(v->z_pos)};
}

bool VehicleMotion::Glides(const TickTrail::Spot &from, const TickTrail::Spot &to)
{
	if (to.tick < from.tick) return false;
	double reach = std::max(MAX_GLIDE_DISTANCE, (to.tick - from.tick) * MAX_STRIDE_PER_TICK);
	return std::abs(to.x - from.x) <= reach && std::abs(to.y - from.y) <= reach;
}

/* A unit keeps the tile it stood on before the one it stands on, as the game forgets which pieces a train took there once it is gone. */
void VehicleMotion::Snapshot::Pass(const Passage &now, bool continues)
{
	if (!continues) {
		this->left = {};
	} else if (now.tile != this->on.tile) {
		this->left = this->on;
	}
	this->on = now;
}

void VehicleMotion::Clear()
{
	this->snapshots.clear();
}
