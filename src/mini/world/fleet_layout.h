/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fleet_layout.h Where every vehicle on the map stands this frame: units laid nose to tail along their path, ships riding the swell, aircraft banking. */

#ifndef MINI_WORLD_FLEET_LAYOUT_H
#define MINI_WORLD_FLEET_LAYOUT_H

#include <array>
#include <optional>
#include <span>
#include <vector>

#include "../../vehicle_type.h"
#include "../core/camera.h"
#include "smoke_vent.h"
#include "vehicle_models.h"

struct Vehicle;

/* How a unit is turned: about the upright, nose up, and its left side up, in radians. */
struct Attitude {
	double yaw;
	double pitch;
	double roll;

	Mat4 Turn() const;
};

/* A unit as drawn this frame: its copy and model, the box it fills in render space and the vehicle a click on it opens. */
struct PlacedUnit {
	VehicleInstance instance;
	VehicleLook look;
	Vec3 centre;
	Mat4 turn;
	Vec3 half;
	double radius;
	VehicleID vehicle;

	/* How far along a sight line it meets the unit's box grown by a margin, or nothing when it misses. */
	std::optional<double> Meets(const Vec3 &origin, const Vec3 &direction, double margin) const;
};

/* A ship under way as its wake sees it: where its hull's middle lies, which way it heads, its half length and half beam, and how near its top speed it runs. */
struct ShipWake {
	Vec3 centre;
	MapVector heading;
	double half_length;
	double half_beam;
	double pace;
};

class FleetLayout {
public:
	explicit FleetLayout(std::span<const VehicleBounds, VEHICLE_LOOKS> bounds) : bounds(bounds) {}

	/* Reads the game's vehicles, so it runs only while the game's state holds still. */
	void Lay(double clock);
	std::span<const PlacedUnit> Units() const { return this->units; }
	std::span<const ShipWake> Wakes() const { return this->wakes; }
	std::span<const SmokeVent> Funnels() const { return this->funnels; }

private:
	struct Link {
		const Vehicle *unit;
		WorldPoint centre;
		double length;
	};

	void LayConsist(const Vehicle *head);
	void LayShip(const Vehicle *ship);
	void LayAircraft(const Vehicle *aircraft);
	void Add(const Vehicle *unit, VehicleLook look, const WorldPoint &ground, const Attitude &attitude, double length);
	void AddFunnel(const Vehicle *engine, double heading);

	std::span<const VehicleBounds, VEHICLE_LOOKS> bounds;
	std::vector<PlacedUnit> units;
	std::vector<ShipWake> wakes;
	std::vector<SmokeVent> funnels;
	std::vector<Link> links;
	std::vector<WorldPoint> path;
	double clock = 0.0;
};

#endif /* MINI_WORLD_FLEET_LAYOUT_H */
