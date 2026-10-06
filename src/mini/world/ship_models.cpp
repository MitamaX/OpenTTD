/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ship_models.cpp Low poly ferries, cargo ships stacked with containers and oil tankers. */

#include "../../stdafx.h"
#include "vehicle_parts.h"

#include "../core/seed.h"

#include "../../safeguards.h"

static constexpr double DRAUGHT = -0.035;
static constexpr NoseShape BOW = {0.1, 1.0, 1.6};
static constexpr uint32_t DECK = 0x9A8466;
static constexpr uint32_t TANKER_DECK = 0x7A3A2E;
static constexpr uint32_t LIFEBOAT = 0xE07020;
static constexpr uint32_t BOOT_TOPPING = 0x8E2A22;
static constexpr uint32_t CONTAINER_SEED = 0xC0A7;
static constexpr double CONTAINER_LENGTH = 0.06;
static constexpr double CONTAINER_WIDTH = 0.045;
static constexpr double CONTAINER_HEIGHT = 0.032;

/* A hull from its stern to where its bow begins, then the bow drawn to a point, with a red band below the waterline. */
static void ShipHull(VehicleKit &kit, double half_width, double freeboard, double stern, double bow_from, double bow)
{
	HullProfile hull = {half_width, DRAUGHT, freeboard, 0.0};
	kit.Body(Hull(hull, stern, bow_from).Paint(DARK_PRIMARY).Gloss(PAINT_GLOSS));
	kit.Body(Nose(hull, bow_from, bow, BOW, DRAUGHT).Paint(DARK_PRIMARY).Gloss(PAINT_GLOSS));
	HullProfile boot = {half_width + 0.002, DRAUGHT, 0.004, 0.0};
	kit.Fine(Hull(boot, stern - 0.002, bow_from).Paint(BOOT_TOPPING));
	kit.Fine(Nose(boot, bow_from, bow, BOW, DRAUGHT).Paint(BOOT_TOPPING));
	kit.Body(Plank({stern + 0.01, -half_width + 0.01, freeboard}, {bow_from, half_width - 0.01, freeboard + 0.004}, DECK));
}

/* A deckhouse of a few tiers stepping in as they rise, with windows round each, and a bridge on top. */
static void Deckhouse(VehicleKit &kit, double back, double front, double half_width, double low, int tiers)
{
	constexpr double TIER = 0.045;
	for (int tier = 0; tier < tiers; tier++) {
		double inset = tier * 0.012;
		double z = low + tier * TIER;
		kit.Body(Hull({half_width - inset, z, z + TIER, 0.006}, back + inset * 2.0, front - inset).Paint(CREAM));
		kit.Fine(WindowRow(back + inset * 2.0 + 0.01, front - inset - 0.01, z + 0.018, z + 0.032, half_width - inset, 6));
	}
	kit.Fine(Windscreen(front - (tiers - 1) * 0.012, half_width - 0.02, low + (tiers - 1) * TIER + 0.018, low + tiers * TIER - 0.01));
}

static void Funnel(VehicleKit &kit, double x, double base, double radius)
{
	kit.Body(Column(8, radius, radius * 0.85, 0.075).Transform(Mat4::Translation({x, 0.0, base})).Paint(PRIMARY).Gloss(PAINT_GLOSS));
	kit.Fine(Column(8, radius * 0.88, radius * 0.86, 0.02).Transform(Mat4::Translation({x, 0.0, base + 0.06})).Paint(SECONDARY));
}

static void MastLight(VehicleKit &kit, double x, double base, double height)
{
	kit.Fine(Prism(5, 0.004, height).Transform(Mat4::Translation({x, 0.0, base})).Paint(RUNNING_GEAR));
	kit.Fine(Plank({x - 0.006, -0.006, base + height}, {x + 0.006, 0.006, base + height + 0.012}, HEADLAMP).Glow(LAMP_GLOW));
}

static ModelMesh Ferry(VehicleDetail detail)
{
	VehicleKit kit(detail);
	ShipHull(kit, 0.12, 0.06, -0.38, 0.2, 0.42);
	Deckhouse(kit, -0.33, 0.18, 0.105, 0.064, 3);
	Funnel(kit, -0.2, 0.199, 0.032);
	MastLight(kit, 0.06, 0.199, 0.06);
	for (double x : {-0.24, -0.12, 0.0, 0.12}) {
		ModelMesh boat = Plank({x - 0.03, 0.105, 0.115}, {x + 0.03, 0.118, 0.13}, LIFEBOAT);
		kit.Fine(boat.Append(Mirrored(boat)));
	}
	return kit.Done();
}

/* Containers stacked on the hold in rows, each a shade of the cargo's colour, so they show only while the ship carries some. */
static ModelMesh ContainerStacks(double back, double front, double half_width, double deck)
{
	ModelMesh stacks;
	SeedDice dice(CONTAINER_SEED);
	int rows = static_cast<int>((front - back) / (CONTAINER_LENGTH + 0.006));
	int columns = static_cast<int>(half_width * 2.0 / (CONTAINER_WIDTH + 0.004));
	for (int row = 0; row < rows; row++) {
		double x = back + row * (CONTAINER_LENGTH + 0.006);
		for (int column = 0; column < columns; column++) {
			double y = -half_width + column * (CONTAINER_WIDTH + 0.004);
			int height = 1 + static_cast<int>(dice.Below(3));
			for (int level = 0; level < height; level++) {
				double z = deck + level * CONTAINER_HEIGHT;
				uint32_t tone = PaintworkTone(Paintwork::Cargo, dice.Between(0.6, 1.4));
				stacks.Append(Plank({x, y, z}, {x + CONTAINER_LENGTH, y + CONTAINER_WIDTH, z + CONTAINER_HEIGHT - 0.002}, tone));
			}
		}
	}
	return stacks;
}

static ModelMesh CargoShip(VehicleDetail detail)
{
	VehicleKit kit(detail);
	ShipHull(kit, 0.13, 0.07, -0.42, 0.24, 0.45);
	Deckhouse(kit, -0.41, -0.29, 0.11, 0.074, 3);
	Funnel(kit, -0.385, 0.209, 0.026);
	kit.Fine(ContainerStacks(-0.27, 0.25, 0.11, 0.074));
	kit.Coarse(Plank({-0.27, -0.11, 0.074}, {0.25, 0.11, 0.074 + CONTAINER_HEIGHT * 2.0}, CARGO));
	for (double x : {-0.1, 0.12}) {
		kit.Fine(Prism(6, 0.008, 0.13).Transform(Mat4::Translation({x, 0.0, 0.074})).Paint(SECONDARY));
		kit.Fine(Plank({x - 0.004, -0.004, 0.18}, {x + 0.11, 0.004, 0.188}, SECONDARY));
	}
	MastLight(kit, 0.3, 0.074, 0.09);
	return kit.Done();
}

static ModelMesh OilTanker(VehicleDetail detail)
{
	VehicleKit kit(detail);
	ShipHull(kit, 0.13, 0.05, -0.42, 0.28, 0.46);
	kit.Body(Plank({-0.3, -0.118, 0.054}, {0.27, 0.118, 0.058}, TANKER_DECK));
	Deckhouse(kit, -0.41, -0.3, 0.11, 0.054, 3);
	Funnel(kit, -0.39, 0.189, 0.026);
	for (double y : {-0.04, 0.0, 0.04}) kit.Fine(Barrel(-0.29, 0.26, 0.008, 0.068, 6).Transform(Mat4::Translation({0.0, y, 0.0})).Paint(STEEL).Gloss(METAL_GLOSS));
	for (double x : {-0.15, 0.0, 0.15}) kit.Fine(Plank({x - 0.015, -0.1, 0.058}, {x + 0.015, 0.1, 0.075}, STEEL));
	MastLight(kit, 0.3, 0.054, 0.08);
	return kit.Done();
}

ModelMesh ShipModel(VehicleLook look, VehicleDetail detail)
{
	switch (look) {
		case VehicleLook::Ferry: return Ferry(detail);
		case VehicleLook::CargoShip: return CargoShip(detail);
		default: return OilTanker(detail);
	}
}
