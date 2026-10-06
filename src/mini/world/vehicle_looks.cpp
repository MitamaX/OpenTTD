/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_looks.cpp Which model a vehicle's unit wears, in whose colours, carrying what, and how high it rides. */

#include "../../stdafx.h"
#include "vehicle_looks.h"

#include <algorithm>
#include <array>

#include "../../aircraft.h"
#include "../../cargotype.h"
#include "../../company_base.h"
#include "../../engine_base.h"
#include "../../rail_map.h"
#include "../../road.h"
#include "../../roadveh.h"
#include "../../settings_type.h"
#include "../../train.h"
#include "../../vehicle_func.h"
#include "../core/tones.h"
#include "road_models.h"
#include "track_models.h"

#include "../../safeguards.h"

/* Engines this fast or faster wear a streamlined body. */
static constexpr uint STREAMLINED_SPEED = 160;
static constexpr uint32_t RGB_MASK = 0xFFFFFF;
static constexpr uint32_t UNOWNED_PRIMARY = 0x8A8F96;
static constexpr uint32_t UNOWNED_SECONDARY = 0xD8D4CA;
static constexpr uint32_t BURNT = 0x2E2C2A;

/** What a load looks like from outside, which picks the body built to carry it. */
enum class CargoKind : uint8_t {
	People,
	Mail,
	Valuables,
	Liquid,
	Chilled,
	Loose,
	Powder,
	Long,
	Livestock,
	Boxed,
	End,
};

using KindLooks = std::array<VehicleLook, static_cast<size_t>(CargoKind::End)>;

static constexpr KindLooks WAGON_LOOKS = {
	VehicleLook::Coach, VehicleLook::MailVan, VehicleLook::ArmouredVan, VehicleLook::TankWagon, VehicleLook::Reefer,
	VehicleLook::OpenWagon, VehicleLook::CoveredHopper, VehicleLook::FlatWagon, VehicleLook::LivestockVan, VehicleLook::BoxVan,
};

static constexpr KindLooks LORRY_LOOKS = {
	VehicleLook::Bus, VehicleLook::BoxTruck, VehicleLook::BoxTruck, VehicleLook::TankerTruck, VehicleLook::BoxTruck,
	VehicleLook::TipperTruck, VehicleLook::TipperTruck, VehicleLook::FlatbedTruck, VehicleLook::LivestockTruck, VehicleLook::BoxTruck,
};

static constexpr KindLooks SHIP_LOOKS = {
	VehicleLook::Ferry, VehicleLook::CargoShip, VehicleLook::CargoShip, VehicleLook::OilTanker, VehicleLook::CargoShip,
	VehicleLook::CargoShip, VehicleLook::CargoShip, VehicleLook::CargoShip, VehicleLook::CargoShip, VehicleLook::CargoShip,
};

struct CargoTone {
	CargoLabel label;
	uint32_t rgb;
};

/* Loads seen in the open take their natural colour; the rest show their legend colour. */
static constexpr std::array<CargoTone, 10> CARGO_TONES = {{
	{CT_COAL, 0x2B2A28}, {CT_IRON_ORE, 0x8C4A32}, {CT_COPPER_ORE, 0x5E7A5C}, {CT_GRAIN, 0xD6B058}, {CT_WHEAT, 0xD6B058},
	{CT_MAIZE, 0xE0B848}, {CT_WOOD, 0x8A5E3A}, {CT_STEEL, 0x8E979F}, {CT_FRUIT, 0x9CC04A}, {CT_PAPER, 0xE8E2D2},
}};

static CargoKind KindOf(CargoType cargo)
{
	if (!IsValidCargoType(cargo)) return CargoKind::Boxed;
	const CargoSpec *spec = CargoSpec::Get(cargo);
	if (spec->label == CT_LIVESTOCK) return CargoKind::Livestock;
	if (spec->label == CT_WOOD || spec->label == CT_STEEL || spec->classes.Test(CargoClass::Oversized)) return CargoKind::Long;
	if (spec->classes.Test(CargoClass::Passengers)) return CargoKind::People;
	if (spec->classes.Test(CargoClass::Mail)) return CargoKind::Mail;
	if (spec->classes.Test(CargoClass::Armoured)) return CargoKind::Valuables;
	if (spec->classes.Test(CargoClass::Liquid)) return CargoKind::Liquid;
	if (spec->classes.Test(CargoClass::Refrigerated)) return CargoKind::Chilled;
	if (spec->classes.Test(CargoClass::Bulk)) return spec->classes.Any({CargoClass::Covered, CargoClass::Powderized}) ? CargoKind::Powder : CargoKind::Loose;
	return CargoKind::Boxed;
}

static VehicleLook LookFor(const KindLooks &looks, const Vehicle *unit)
{
	return looks[static_cast<size_t>(KindOf(unit->cargo_type))];
}

static RailLook TrackLookUnder(const Vehicle *unit)
{
	RailType railtype = GetTileRailType(unit->tile);
	return railtype == INVALID_RAILTYPE ? RailLook::Rail : RailLookOf(railtype);
}

static VehicleLook EngineLook(const Engine *engine)
{
	bool streamlined = engine->GetDisplayMaxSpeed() >= STREAMLINED_SPEED;
	switch (engine->VehInfo<RailVehicleInfo>().engclass) {
		case EC_STEAM: return VehicleLook::SteamEngine;
		case EC_DIESEL: return streamlined ? VehicleLook::DieselStreamliner : VehicleLook::DieselEngine;
		case EC_ELECTRIC: return streamlined ? VehicleLook::ElectricStreamliner : VehicleLook::ElectricEngine;
		case EC_MONORAIL: return VehicleLook::MonorailEngine;
		default: return VehicleLook::MaglevEngine;
	}
}

static VehicleLook TrainLook(const Vehicle *unit)
{
	const Engine *engine = Engine::Get(unit->engine_type);
	if (engine->VehInfo<RailVehicleInfo>().railveh_type != RAILVEH_WAGON) return EngineLook(engine);
	if (TrackLookUnder(unit) != RailLook::Rail) return VehicleLook::SleekCoach;
	return LookFor(WAGON_LOOKS, unit);
}

static VehicleLook RoadVehicleLook(const Vehicle *unit)
{
	if (RoadTypeIsTram(RoadVehicle::From(unit)->roadtype)) return VehicleLook::Tram;
	return LookFor(LORRY_LOOKS, unit);
}

static VehicleLook AircraftLook(const Vehicle *unit)
{
	if (unit->subtype == AIR_HELICOPTER) return VehicleLook::Helicopter;
	return (AircraftVehInfo(unit->engine_type)->subtype & AIR_FAST) != 0 ? VehicleLook::Jet : VehicleLook::PropPlane;
}

VehicleLook LookOf(const Vehicle *unit)
{
	switch (unit->type) {
		case VEH_TRAIN: return TrainLook(unit);
		case VEH_ROAD: return RoadVehicleLook(unit);
		case VEH_SHIP: return LookFor(SHIP_LOOKS, unit);
		default: return AircraftLook(unit);
	}
}

static uint32_t CargoRgbOf(CargoType cargo)
{
	if (!IsValidCargoType(cargo)) return UNOWNED_PRIMARY;
	CargoLabel label = CargoSpec::Get(cargo)->label;
	for (const CargoTone &tone : CARGO_TONES) {
		if (tone.label == label) return tone.rgb;
	}
	return CargoRgb(cargo) & RGB_MASK;
}

VehiclePaint PaintOf(const Vehicle *unit)
{
	double load = unit->cargo_cap > 0 ? static_cast<double>(unit->cargo.StoredCount()) / unit->cargo_cap : 0.0;
	VehiclePaint paint = {UNOWNED_PRIMARY, UNOWNED_SECONDARY, CargoRgbOf(unit->cargo_type), std::min(load, 1.0)};
	if (unit->vehstatus.Test(VehState::Crashed)) {
		paint.primary = BURNT;
		paint.secondary = BURNT;
	} else if (Company::IsValidID(unit->owner)) {
		EngineID parent = unit->IsGroundVehicle() ? unit->GetGroundVehicleCache()->first_engine : EngineID::Invalid();
		const Livery *livery = GetEngineLivery(unit->engine_type, unit->owner, parent, unit, _settings_client.gui.liveries);
		paint.primary = _company_rgb[livery->colour1] & RGB_MASK;
		paint.secondary = _company_rgb[livery->colour2] & RGB_MASK;
	}
	return paint;
}

double RideHeightOf(const Vehicle *unit)
{
	switch (unit->type) {
		case VEH_TRAIN: return RideHeight(TrackLookUnder(unit));
		case VEH_ROAD: return ASPHALT_TOP;
		default: return 0.0;
	}
}
