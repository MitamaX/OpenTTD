/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_parts.h The parts vehicle models share: bodies laid along their length, noses, wheels, bogies, windows and lamps. */

#ifndef MINI_WORLD_VEHICLE_PARTS_H
#define MINI_WORLD_VEHICLE_PARTS_H

#include <span>
#include <utility>

#include "../model/model_shapes.h"
#include "vehicle_models.h"

/* A full length unit of a ground vehicle in tiles, and how far its body reaches either way of its middle, short of its couplers. */
inline constexpr double UNIT_TILES = 0.5;
inline constexpr double UNIT_REACH = 0.228;

inline constexpr uint32_t GLASS = 0x26323C;
inline constexpr uint32_t RUNNING_GEAR = 0x2A2A2C;
inline constexpr uint32_t TYRE = 0x1C1C1E;
inline constexpr uint32_t ROOF_GREY = 0x8E9094;
inline constexpr uint32_t STEEL = 0x9CA2A8;
inline constexpr uint32_t CREAM = 0xEDE8DA;
inline constexpr uint32_t HEADLAMP = 0xFFF0C8;
inline constexpr uint32_t TAIL_LAMP = 0xFF3828;
inline constexpr uint32_t SLAT_GAP = 0x2A2622;
inline constexpr uint32_t TIMBER_DECK = 0x6A5A48;
inline constexpr uint32_t PANTOGRAPH = 0x3A3C40;
inline constexpr uint32_t PRIMARY = PaintworkTone(Paintwork::Primary, 1.0);
inline constexpr uint32_t DARK_PRIMARY = PaintworkTone(Paintwork::Primary, 0.6);
inline constexpr uint32_t MUTED_PRIMARY = PaintworkTone(Paintwork::Primary, 0.75);
inline constexpr uint32_t SECONDARY = PaintworkTone(Paintwork::Secondary, 1.0);
inline constexpr uint32_t DARK_SECONDARY = PaintworkTone(Paintwork::Secondary, 0.7);
inline constexpr uint32_t CARGO = PaintworkTone(Paintwork::Cargo, 1.0);
inline constexpr uint32_t BRIGHT_CARGO = PaintworkTone(Paintwork::Cargo, 1.2);
inline constexpr double GLASS_GLOSS = 0.8;
inline constexpr double PAINT_GLOSS = 0.45;
inline constexpr double METAL_GLOSS = 0.7;
inline constexpr double LAMP_GLOW = 0.8;

/* A body's cross section square to its length: half its width, its foot and top, and how much its top edges are bevelled. */
struct HullProfile {
	double half_width;
	double low;
	double high;
	double bevel;

	HullProfile Grown(double by) const { return {this->half_width + by, this->low, this->high + by, this->bevel}; }
};

/* How a nose narrows and lowers toward its tip, as shares of the body, and how quickly it starts to: one is a straight wedge, more is rounder. */
struct NoseShape {
	double width;
	double height;
	double curve;
};

/* The parts a model is built of; fine parts are left out of the simple model, and coarse ones stand in for them there. */
class VehicleKit {
public:
	explicit VehicleKit(VehicleDetail detail) : detail(detail) {}

	bool Full() const { return this->detail == VehicleDetail::Full; }

	VehicleKit &Body(const ModelMesh &part)
	{
		this->mesh.Append(part);
		return *this;
	}

	VehicleKit &Fine(const ModelMesh &part)
	{
		if (this->Full()) this->mesh.Append(part);
		return *this;
	}

	VehicleKit &Coarse(const ModelMesh &part)
	{
		if (!this->Full()) this->mesh.Append(part);
		return *this;
	}

	ModelMesh Done() { return std::move(this->mesh); }

private:
	VehicleDetail detail;
	ModelMesh mesh;
};

ModelMesh Hull(const HullProfile &profile, double back, double front);
/* A band along both sides of a body, standing a little proud of it. */
ModelMesh SideBand(double back, double front, double half_width, double low, double high, uint32_t tone);
/* A hull from where the body ends to its tip, narrowing and lowering toward it over several rings, faceted. */
ModelMesh Nose(const HullProfile &profile, double from, double to, const NoseShape &shape, double base);
/* A solid turned about an axis along the length, its rings running from back to front at their reach along x. */
ModelMesh Turned(std::span<const LatheRing> profile, int sides, double axis_z);
ModelMesh Barrel(double back, double front, double radius, double axis_z, int sides);
ModelMesh Plank(const Vec3 &low, const Vec3 &high, uint32_t tone);
ModelMesh Wheel(double x, double y, double radius, double width);
ModelMesh WheelPair(double x, double half_track, double radius, double width);
ModelMesh Bogie(double x, double half_track, double radius);
/* Window boxes standing proud of both sides of a body, evenly spread between its ends. */
ModelMesh WindowRow(double back, double front, double low, double high, double half_width, int count);
ModelMesh Windscreen(double x, double half_width, double low, double high);
/* A pair of lamps on the face at x, either side of the middle, glowing. */
ModelMesh LampPair(double x, double half_spacing, double z, double half, uint32_t tone);
ModelMesh Mirrored(const ModelMesh &part);

ModelMesh RailVehicleModel(VehicleLook look, VehicleDetail detail);
ModelMesh RoadVehicleModel(VehicleLook look, VehicleDetail detail);
ModelMesh ShipModel(VehicleLook look, VehicleDetail detail);
ModelMesh AircraftModel(VehicleLook look, VehicleDetail detail);

#endif /* MINI_WORLD_VEHICLE_PARTS_H */
