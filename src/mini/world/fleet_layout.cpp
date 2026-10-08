/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fleet_layout.cpp Where every vehicle in view stands this frame: units laid nose to tail along their path, ships riding the swell, aircraft banking. */

#include "../../stdafx.h"
#include "fleet_layout.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../../aircraft.h"
#include "../../ground_vehicle.hpp"
#include "../../train.h"
#include "../../vehicle_base.h"
#include "../core/seed.h"
#include "../gpu/draw_list.h"
#include "../map/tile_shapes.h"
#include "../map/vehicle_motion.h"
#include "seabed.h"
#include "shadow_map.h"
#include "vehicle_looks.h"

#include "../../safeguards.h"

static constexpr Vec3 X_AXIS = {1.0, 0.0, 0.0};
static constexpr Vec3 Y_AXIS = {0.0, 1.0, 0.0};
static constexpr Vec3 Z_AXIS = {0.0, 0.0, 1.0};
static constexpr double SWELL_LEVELS = 0.012;
static constexpr double SWELL_PITCH = 0.025;
static constexpr double SWELL_ROLL = 0.04;
static constexpr double BANK_PER_TURN = 0.35;
static constexpr double MAX_BANK = 0.55;
static constexpr double AIRBORNE_LEVELS = 1.0;
static constexpr double ROTOR_TURNS_PER_SECOND = 3.3;
static constexpr double TRAIL_TILES_PER_SECOND = 1.6;
/* A steam engine's plume, the longest of what a vehicle leaves behind, trails it for as long as smoke.vert has a puff rise. */
static constexpr double PLUME_SECONDS = 11.0;
/* A unit stands up to a glide of two tiles from where the game last put it, raised onto its ride and settled nose to tail along its path. */
static constexpr double PLACING_SLACK_TILES = 3.0;
/* The game keeps a stopped rotor's animation state at zero. */
static constexpr uint8_t ROTOR_STOPPED = 0;

Mat4 Attitude::Turn() const
{
	return Mat4::Turn(Z_AXIS, this->yaw) * Mat4::Turn(Y_AXIS, -this->pitch) * Mat4::Turn(X_AXIS, this->roll);
}

/* A direction into the frame a turn puts things in, by the turn's transpose. */
static Vec3 IntoFrame(const Mat4 &turn, const Vec3 &v)
{
	return {
		turn.At(0, 0) * v.x + turn.At(1, 0) * v.y + turn.At(2, 0) * v.z,
		turn.At(0, 1) * v.x + turn.At(1, 1) * v.y + turn.At(2, 1) * v.z,
		turn.At(0, 2) * v.x + turn.At(1, 2) * v.y + turn.At(2, 2) * v.z,
	};
}

std::optional<double> PlacedUnit::Meets(const Vec3 &origin, const Vec3 &direction, double margin) const
{
	Vec3 grown = this->half + Vec3{margin, margin, margin};
	return BoxEntry(IntoFrame(this->turn, origin - this->centre), IntoFrame(this->turn, direction), Vec3{0.0, 0.0, 0.0} - grown, grown);
}

static double FlatDistance(const WorldPoint &a, const WorldPoint &b)
{
	return std::hypot(b.x - a.x, b.y - a.y);
}

/* The point an arc length along a path, carried on past its last point along its last stretch. */
static WorldPoint Along(std::span<const WorldPoint> path, double arc)
{
	for (size_t point = 1; point < path.size(); point++) {
		double span = FlatDistance(path[point - 1], path[point]);
		if (arc <= span || point + 1 == path.size()) return Between(path[point - 1], path[point], span > 0.0 ? arc / span : 0.0);
		arc -= span;
	}
	return path.front();
}

static WorldPoint Ahead(const WorldPoint &from, double bearing, double reach)
{
	return {from.x + std::cos(bearing) * reach, from.y + std::sin(bearing) * reach, from.level};
}

/* Units the game shows sideways round, such as the rear of a pair of engines, wear their model the other way about. */
static bool IsReversed(const Vehicle *unit)
{
	if (unit->type != VEH_TRAIN) return false;
	const Train *train = Train::From(unit);
	return train->IsRearDualheaded() || train->flags.Test(VehicleRailFlag::Flipped);
}

static double UnitTiles(const Vehicle *unit)
{
	return unit->GetGroundVehicleCache()->cached_veh_length / static_cast<double>(TILE_SIZE);
}

/* How near its top speed a vehicle runs. */
static double PaceOf(const Vehicle *head)
{
	return head->cur_speed / static_cast<double>(std::max<uint16_t>(head->vcache.cached_max_speed, 1));
}

/* Where the game put a unit at its last tick. */
static Vec3 TickPoint(const Vehicle *unit)
{
	return RenderPoint({unit->x_pos / static_cast<double>(TILE_SIZE), unit->y_pos / static_cast<double>(TILE_SIZE), unit->z_pos / static_cast<double>(TILE_HEIGHT)});
}

/* The box about a vehicle's units, grown by how far they reach and by the trail it leaves, stretched down the sun's rays to the lowest ground its shadow may fall on, meets the view. */
bool FleetLayout::MayShow(const SceneView &view, const Vehicle *head, double reach) const
{
	Vec3 low = TickPoint(head);
	Vec3 high = low;
	if (head->IsGroundVehicle()) {
		for (const Vehicle *unit = head->Next(); unit != nullptr; unit = unit->Next()) {
			Vec3 at = TickPoint(unit);
			low = {std::min(low.x, at.x), std::min(low.y, at.y), std::min(low.z, at.z)};
			high = {std::max(high.x, at.x), std::max(high.y, at.y), std::max(high.z, at.z)};
		}
	}
	bool trails = head->type == VEH_TRAIN || head->type == VEH_SHIP;
	double grown = reach + PLACING_SLACK_TILES + (trails ? PaceOf(head) * TRAIL_TILES_PER_SECOND * PLUME_SECONDS : 0.0);
	low = low - Vec3{grown, grown, grown};
	high = high + Vec3{grown, grown, grown};

	double floor = -DEEPEST_SINK * LevelRise();
	MapVector fall = ShadowFall();
	double drop = high.z - floor;
	low = {low.x + std::min(fall.x * drop, 0.0), low.y + std::min(fall.y * drop, 0.0), std::min(low.z, floor)};
	high = {high.x + std::max(fall.x * drop, 0.0), high.y + std::max(fall.y * drop, 0.0), high.z};
	return BoxMeets(view.frustum, low, high);
}

void FleetLayout::Lay(const SceneView &view, double reach)
{
	this->clock = view.clock;
	this->units.clear();
	this->wakes.clear();
	this->funnels.clear();
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->type > VEH_AIRCRAFT || !v->IsPrimaryVehicle() || !this->MayShow(view, v, reach)) continue;
		switch (v->type) {
			case VEH_SHIP: this->LayShip(v); break;
			case VEH_AIRCRAFT: this->LayAircraft(v); break;
			default: this->LayConsist(v); break;
		}
	}
}

/* The game spaces units in path steps, which on a diagonal lie further apart than the units are long, so the units are laid
 * nose to tail at their true length along the path through their centres, from a nose half a unit ahead of the first. */
void FleetLayout::LayConsist(const Vehicle *head)
{
	this->links.clear();
	for (const Vehicle *unit = head; unit != nullptr; unit = unit->Next()) {
		if (!unit->vehstatus.Test(VehState::Hidden)) this->links.push_back({unit, _vehicle_motion.Position(unit), UnitTiles(unit)});
	}
	if (this->links.empty()) return;

	const Link &first = this->links.front();
	const Link &last = this->links.back();
	this->path.clear();
	this->path.push_back(Ahead(first.centre, _vehicle_motion.Bearing(first.unit), first.length * 0.5));
	for (const Link &link : this->links) this->path.push_back(link.centre);
	this->path.push_back(Ahead(last.centre, _vehicle_motion.Bearing(last.unit), -last.length * 0.5));

	double arc = 0.0;
	for (const Link &link : this->links) {
		WorldPoint front = VehicleMotion::Grounded(link.unit, Along(this->path, arc));
		WorldPoint back = VehicleMotion::Grounded(link.unit, Along(this->path, arc + link.length));
		arc += link.length;
		double run = FlatDistance(back, front);
		double heading = std::atan2(front.y - back.y, front.x - back.x);
		double yaw = heading + (IsReversed(link.unit) ? std::numbers::pi : 0.0);
		double pitch = std::atan2((front.level - back.level) * LevelRise(), std::max(run, 1e-6)) * (IsReversed(link.unit) ? -1.0 : 1.0);
		VehicleLook look = LookOf(link.unit);
		this->Add(link.unit, look, Between(back, front, 0.5), {yaw, pitch, 0.0}, link.unit->GetGroundVehicleCache()->cached_veh_length / static_cast<double>(VEHICLE_LENGTH));
		if (look == VehicleLook::SteamEngine) this->AddFunnel(link.unit, heading);
	}
}

/* A ship rocks and heaves gently on the swell, each in its own time. */
void FleetLayout::LayShip(const Vehicle *ship)
{
	if (ship->vehstatus.Test(VehState::Hidden)) return;
	double phase = this->clock + (ship->index.base() % 97) * 0.37;
	WorldPoint at = _vehicle_motion.Position(ship);
	at.level += SWELL_LEVELS * std::sin(phase * 1.3);
	double bearing = _vehicle_motion.Bearing(ship);
	this->Add(ship, LookOf(ship), at, {bearing, SWELL_PITCH * std::sin(phase * 0.9), SWELL_ROLL * std::sin(phase * 0.7 + 1.0)}, 1.0);
	double pace = PaceOf(ship);
	if (pace <= 0.0) return;
	const PlacedUnit &hull = this->units.back();
	this->wakes.push_back({hull.centre, {std::cos(bearing), std::sin(bearing)}, hull.half.x, hull.half.y, std::min(pace, 1.0)});
}

/* Steam leaves an engine's chimney and is left behind as the engine runs on, the faster the further. */
void FleetLayout::AddFunnel(const Vehicle *engine, double heading)
{
	const PlacedUnit &placed = this->units.back();
	const VehicleInstance &instance = placed.instance;
	Vec3 mouth = {STEAM_CHIMNEY_FOOT.x * instance.length, STEAM_CHIMNEY_FOOT.y, STEAM_CHIMNEY_FOOT.z + STEAM_CHIMNEY_HEIGHT};
	Vec3 at = Vec3{instance.x, instance.y, instance.z} + Transformed(placed.turn, mouth);
	Vec3 motion = Vec3{std::cos(heading), std::sin(heading), 0.0} * (PaceOf(engine->First()) * TRAIL_TILES_PER_SECOND);
	this->funnels.push_back({at, STEAM_CHIMNEY_MOUTH, motion, Hash32(engine->index.base())});
}

/* Planes bank into their turns while airborne; a helicopter's rotor turns while the game has it running. */
void FleetLayout::LayAircraft(const Vehicle *aircraft)
{
	if (aircraft->vehstatus.Test(VehState::Hidden)) return;
	WorldPoint at = _vehicle_motion.Position(aircraft);
	double yaw = _vehicle_motion.Bearing(aircraft);
	bool airborne = at.level > GroundLevel(at.x, at.y) + AIRBORNE_LEVELS;
	bool helicopter = aircraft->subtype == AIR_HELICOPTER;
	double bank = airborne && !helicopter ? std::clamp(-_vehicle_motion.TurnRate(aircraft) * BANK_PER_TURN, -MAX_BANK, MAX_BANK) : 0.0;
	this->Add(aircraft, LookOf(aircraft), at, {yaw, 0.0, bank}, 1.0);
	if (!helicopter) return;

	const Vehicle *rotor = aircraft->Next() != nullptr ? aircraft->Next()->Next() : nullptr;
	bool turning = rotor != nullptr && Aircraft::From(rotor)->state != ROTOR_STOPPED;
	double spin = turning ? std::fmod(this->clock * ROTOR_TURNS_PER_SECOND, 1.0) * 2.0 * std::numbers::pi : yaw;
	this->Add(aircraft, VehicleLook::Rotor, at, {spin, 0.0, 0.0}, 1.0);
}

void FleetLayout::Add(const Vehicle *unit, VehicleLook look, const WorldPoint &ground, const Attitude &attitude, double length)
{
	const VehicleBounds &box = this->bounds[to_underlying(look)];
	Vec3 middle = {(box.low.x + box.high.x) * 0.5 * length, (box.low.y + box.high.y) * 0.5, (box.low.z + box.high.z) * 0.5};
	Vec3 half = {(box.high.x - box.low.x) * 0.5 * length, (box.high.y - box.low.y) * 0.5, (box.high.z - box.low.z) * 0.5};
	Vec3 place = RenderPoint(ground) + Vec3{0.0, 0.0, RideHeightOf(unit)};
	Mat4 turn = attitude.Turn();
	VehiclePaint paint = PaintOf(unit);
	VehicleInstance instance = {
		static_cast<float>(place.x), static_cast<float>(place.y), static_cast<float>(place.z),
		static_cast<float>(attitude.yaw), static_cast<float>(attitude.pitch), static_cast<float>(attitude.roll), static_cast<float>(length),
		InstanceColour(paint.primary), InstanceColour(paint.secondary), InstanceColour(paint.cargo, paint.load),
	};
	this->units.push_back({instance, look, place + Transformed(turn, middle), turn, half, Length(half), unit->First()->index});
}
