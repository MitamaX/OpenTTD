/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file hill_relief.h The ground of the land as its meshes and everything standing on it read it. */

#ifndef MINI_WORLD_HILL_RELIEF_H
#define MINI_WORLD_HILL_RELIEF_H

#include <array>
#include <memory>
#include <variant>

#include "../core/camera.h"
#include "../core/ground_trace.h"
#include "lazy_lattice.h"
#include "seabed.h"
#include "shore_relief.h"
#include "tile_relief.h"

inline constexpr int LANDFORM_REACH = 2;
inline constexpr int ROLL_SPREAD = 1;
inline constexpr double CREST_WAVE = 0.15;
inline constexpr int FINE_RELIEF_DIVISIONS = 2;
inline constexpr double RELIEF_CELL_PIXELS = 32.0;
inline constexpr double FINE_RELIEF_PIXELS = RELIEF_CELL_PIXELS * FINE_RELIEF_DIVISIONS;
inline constexpr double FINE_RELIEF_LEAD = 1.4;
inline constexpr double FINE_RELIEF_LAG = 1.25;
inline constexpr double FINE_RELIEF_MOVING_LAG = 2.0;
inline constexpr double FINE_RELIEF_CHOSEN_PIXELS = FINE_RELIEF_PIXELS / FINE_RELIEF_LEAD;
inline constexpr double FINE_RELIEF_KEPT_PIXELS = FINE_RELIEF_CHOSEN_PIXELS / FINE_RELIEF_LAG;
inline constexpr double FINE_RELIEF_MOVING_KEPT_PIXELS = FINE_RELIEF_CHOSEN_PIXELS / FINE_RELIEF_MOVING_LAG;

class Landform {
public:
	explicit Landform(const TileSpan &tiles);

	double LevelAt(double x, double y) const;
	WorldPoint At(const MapVector &at) const { return {at.x, at.y, this->LevelAt(at.x, at.y)}; }
	/* How the ground climbs along map x and map y at a point, in levels per tile. */
	MapVector Climb(double x, double y) const;

	bool Rolls(int tx, int ty) const;
	bool RollsRound(int cx, int cy) const;
	bool Bends(int tx, int ty) const;
	std::array<double, 4> Corners(int tx, int ty) const;
	double Rolled(double x, double y) const;
	Vec3 RolledNormal(double x, double y) const;
	const Seabed &Bed() const { return this->bed; }
	const BankSite &BanksAround(int tx, int ty) const { return this->banks.Around(this->bed, tx, ty); }

private:
	double Corner(int cx, int cy) const;
	double MeetingCorner(int cx, int cy) const;
	bool Planar(int tx, int ty) const;

	LazyLattice<bool> rolling;
	LazyLattice<double> corners;
	Seabed bed;
	SeaBanks banks;
};

class HillRelief : public TileRelief {
public:
	HillRelief(const Landform &land, int tx, int ty);

	Vec3 Normal(double x, double y, const Vec3 &faceted) const override;
	bool HoldsWater() const override { return false; }

private:
	double Curved(double x, double y) const override { return this->land.Rolled(x, y); }
	bool Curves(int nx, int ny) const override { return this->land.Rolls(nx, ny); }
	double Held(double, double, double corner) const override { return corner; }

	const Landform &land;
};

class LaidGround {
public:
	LaidGround(const Landform &land, int tx, int ty);

	static bool RollsAt(const Landform &land, int tx, int ty);

	double Level(double x, double y) const;
	double Level(const MapVector &at) const { return this->Level(at.x, at.y); }
	WorldPoint At(const MapVector &at) const { return {at.x, at.y, this->Level(at)}; }
	const TileRelief *Relief() const;
	bool Banked() const;

private:
	int tx;
	int ty;
	std::variant<std::monostate, HillRelief, ShoreRelief> relief;
};

class RolledSurface : public DrawnSurface {
public:
	RolledSurface(std::shared_ptr<const Landform> land, int tx, int ty) : land(std::move(land)), relief(*this->land, tx, ty) {}

	double Level(double x, double y) const override { return this->relief.Level(x, y); }
	bool Curves() const override { return true; }
	int Divisions(double tile_pixels) const override { return tile_pixels >= FINE_RELIEF_MOVING_KEPT_PIXELS ? FINE_RELIEF_DIVISIONS : 1; }

private:
	std::shared_ptr<const Landform> land;
	HillRelief relief;
};

class RolledSpan : public GroundSpan {
public:
	explicit RolledSpan(const TileSpan &tiles) : land(std::make_shared<Landform>(tiles)) {}

	std::unique_ptr<DrawnSurface> Surface(int tx, int ty) const override;

private:
	std::shared_ptr<const Landform> land;
};

class RolledLand : public GroundForm {
public:
	std::unique_ptr<GroundSpan> Over(const TileSpan &tiles) const override { return std::make_unique<RolledSpan>(tiles); }
};

#endif /* MINI_WORLD_HILL_RELIEF_H */
