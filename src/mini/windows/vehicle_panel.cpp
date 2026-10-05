/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_panel.cpp A vehicle: where it is headed, what it carries, its orders and its facts. */

#include "../../stdafx.h"
#include "vehicle_panel.h"

#include "../../cargotype.h"
#include "../../command_func.h"
#include "../../company_func.h"
#include "../../core/format.hpp"
#include "../../core/math_func.hpp"
#include "../../engine_base.h"
#include "../../gfx_func.h"
#include "../../ground_vehicle.hpp"
#include "../../order_base.h"
#include "../../order_cmd.h"
#include "../../order_func.h"
#include "../../roadveh_cmd.h"
#include "../../settings_type.h"
#include "../../strings_func.h"
#include "../../timer/timer_game_calendar.h"
#include "../../timer/timer_game_economy.h"
#include "../../train.h"
#include "../../train_cmd.h"
#include "../../vehicle_base.h"
#include "../../vehicle_cmd.h"
#include "../../vehicle_func.h"
#include "../input/input_mode.h"
#include "../ui/ui_text.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

enum VehicleTab : int {
	VT_STATUS,
	VT_CARGO,
	VT_ORDERS,
	VT_INFO,
};

static constexpr std::string_view NON_STOP_NAMES[] = {"모든 역 정차", "비정차", "경유", "비정차 경유"};
static constexpr std::string_view DEPOT_ACTION_NAMES[] = {"항상 입고", "필요할 때만", "입고 후 정지", "입고 후 간격 조정"};

static constexpr uint SERVICE_STEP_PERCENT = 5;
static constexpr uint SERVICE_STEP_MINUTES = 1;
static constexpr uint SERVICE_STEP_DAYS = 10;

using VehicleAct = std::function<void(const Vehicle &v)>;

/* Rows act on the vehicle as it is when clicked; a tick may have moved or removed it since the rows were built. */
static LedgerLine::Action OnVehicle(VehicleID id, VehicleAct act)
{
	return [id, act = std::move(act)] {
		if (const Vehicle *v = Vehicle::GetIfValid(id); v != nullptr) act(*v);
	};
}

static StringID LoadName(OrderLoadType type)
{
	switch (type) {
		case OrderLoadType::FullLoad: return STR_ORDER_DROP_FULL_LOAD_ALL;
		case OrderLoadType::FullLoadAny: return STR_ORDER_DROP_FULL_LOAD_ANY;
		case OrderLoadType::NoLoad: return STR_ORDER_DROP_NO_LOADING;
		default: return STR_ORDER_DROP_LOAD_IF_POSSIBLE;
	}
}

static StringID UnloadName(OrderUnloadType type)
{
	switch (type) {
		case OrderUnloadType::Unload: return STR_ORDER_DROP_UNLOAD;
		case OrderUnloadType::Transfer: return STR_ORDER_DROP_TRANSFER;
		case OrderUnloadType::NoUnload: return STR_ORDER_DROP_NO_UNLOADING;
		default: return STR_ORDER_DROP_UNLOAD_IF_ACCEPTED;
	}
}

static OrderLoadType NextLoad(OrderLoadType type)
{
	switch (type) {
		case OrderLoadType::LoadIfPossible: return OrderLoadType::FullLoad;
		case OrderLoadType::FullLoad: return OrderLoadType::FullLoadAny;
		case OrderLoadType::FullLoadAny: return OrderLoadType::NoLoad;
		default: return OrderLoadType::LoadIfPossible;
	}
}

static OrderUnloadType NextUnload(OrderUnloadType type)
{
	switch (type) {
		case OrderUnloadType::UnloadIfPossible: return OrderUnloadType::Unload;
		case OrderUnloadType::Unload: return OrderUnloadType::Transfer;
		case OrderUnloadType::Transfer: return OrderUnloadType::NoUnload;
		default: return OrderUnloadType::UnloadIfPossible;
	}
}

static int NonStopIndex(OrderNonStopFlags flags)
{
	return (flags.Test(OrderNonStopFlag::NoIntermediate) ? 1 : 0) | (flags.Test(OrderNonStopFlag::NoDestination) ? 2 : 0);
}

static OrderNonStopFlags NonStopFlags(int index)
{
	OrderNonStopFlags flags{};
	if ((index & 1) != 0) flags.Set(OrderNonStopFlag::NoIntermediate);
	if ((index & 2) != 0) flags.Set(OrderNonStopFlag::NoDestination);
	return flags;
}

static OrderDepotAction DepotAction(const Order &order)
{
	OrderDepotActionFlags flags = order.GetDepotActionType();
	if (flags.Test(OrderDepotActionFlag::Unbunch)) return OrderDepotAction::Unbunch;
	if (flags.Test(OrderDepotActionFlag::Halt)) return OrderDepotAction::Stop;
	if (order.GetDepotOrderType().Test(OrderDepotTypeFlag::Service)) return OrderDepotAction::Service;
	return OrderDepotAction::AlwaysGo;
}

static CargoTypes RefitMask(const Vehicle &v)
{
	CargoTypes mask = 0;
	for (const Vehicle *u = &v; u != nullptr; u = u->Next()) mask |= u->GetEngine()->info.refit_mask;
	return mask;
}

static bool Carries(const Vehicle &v, CargoType cargo)
{
	for (const Vehicle *u = &v; u != nullptr; u = u->Next()) {
		if (u->cargo_cap > 0 && u->cargo_type == cargo) return true;
	}
	return false;
}

static std::string RefitName(CargoType cargo)
{
	if (cargo == CARGO_NO_REFIT) return "안 함";
	if (cargo == CARGO_AUTO_REFIT) return "자동";
	return GameText(CargoSpec::Get(cargo)->name);
}

static void ModifyOrder(VehicleID id, VehicleOrderID index, ModifyOrderFlags flag, uint16_t value)
{
	if (const Vehicle *v = Vehicle::GetIfValid(id); v != nullptr) Command<CMD_MODIFY_ORDER>::Post(STR_ERROR_CAN_T_MODIFY_THIS_ORDER, v->tile, id, index, flag, value);
}

static LedgerLine StatusLine(const Vehicle &v)
{
	if (v.vehstatus.Test(VehState::Crashed)) return LedgerLine::Text(GameText(STR_VEHICLE_STATUS_CRASHED), Tone::Loss);
	if (v.vehstatus.Test(VehState::Stopped)) return LedgerLine::Text(GameText(STR_VEHICLE_STATUS_STOPPED), Tone::Loss);
	if (v.current_order.IsType(OT_GOTO_STATION)) return LedgerLine::Text(GameText(STR_STATION_NAME, v.current_order.GetDestination().ToStationID()), Tone::Accent);
	if (v.current_order.IsType(OT_GOTO_DEPOT)) return LedgerLine::Text("차고로 이동 중", Tone::Accent);
	return LedgerLine::Text("-", Tone::Dim);
}

/* A route with several depot stops read as a list of identical rows, so the row names the depot. */
static std::string OrderTarget(const Vehicle &v, const Order &order)
{
	switch (order.GetType()) {
		case OT_GOTO_STATION: return GameText(STR_STATION_NAME, order.GetDestination().ToStationID());
		case OT_GOTO_WAYPOINT: return GameText(STR_WAYPOINT_NAME, order.GetDestination().ToStationID());
		case OT_GOTO_DEPOT:
			if (order.GetDepotActionType().Test(OrderDepotActionFlag::NearestDepot)) return "가까운 차고";
			return GameText(STR_DEPOT_NAME, v.type, order.GetDestination());
		case OT_CONDITIONAL: return fmt::format("조건 {}번", order.GetConditionSkipToOrder() + 1);
		default: return {};
	}
}

static void AppendMark(std::string &marks, std::string_view mark)
{
	marks += " · ";
	marks += mark;
}

static std::string OrderMarks(const Order &order)
{
	std::string marks;
	if (order.IsType(OT_GOTO_STATION)) {
		OrderNonStopFlags non_stop = order.GetNonStopType();
		if (non_stop.Test(OrderNonStopFlag::NoIntermediate)) AppendMark(marks, "비정차");
		if (non_stop.Test(OrderNonStopFlag::NoDestination)) AppendMark(marks, "경유");
		switch (order.GetLoadType()) {
			case OrderLoadType::FullLoad:
			case OrderLoadType::FullLoadAny: AppendMark(marks, "만재"); break;
			case OrderLoadType::NoLoad: AppendMark(marks, "무적재"); break;
			default: break;
		}
		switch (order.GetUnloadType()) {
			case OrderUnloadType::Unload: AppendMark(marks, "강제 하차"); break;
			case OrderUnloadType::Transfer: AppendMark(marks, "환승"); break;
			case OrderUnloadType::NoUnload: AppendMark(marks, "무하차"); break;
			default: break;
		}
	} else if (order.IsType(OT_GOTO_DEPOT)) {
		if (order.GetDepotActionType().Test(OrderDepotActionFlag::Unbunch)) AppendMark(marks, "간격 조정");
		if (order.GetDepotActionType().Test(OrderDepotActionFlag::Halt)) AppendMark(marks, "정지");
		if (order.GetDepotOrderType().Test(OrderDepotTypeFlag::Service)) AppendMark(marks, "필요할 때만");
	}
	if (order.IsRefit()) AppendMark(marks, fmt::format("개조 {}", order.IsAutoRefit() ? std::string("자동") : GameText(CargoSpec::Get(order.GetRefitCargo())->name)));
	return marks;
}

struct CargoLoad {
	CargoType cargo;
	uint capacity = 0;
	uint stored = 0;
};

static std::vector<CargoLoad> CargoLoads(const Vehicle &v)
{
	std::vector<CargoLoad> loads;
	for (const Vehicle *u = &v; u != nullptr; u = u->Next()) {
		if (u->cargo_cap == 0 || !IsValidCargoType(u->cargo_type)) continue;
		auto it = std::ranges::find(loads, u->cargo_type, &CargoLoad::cargo);
		CargoLoad &load = it != loads.end() ? *it : loads.emplace_back(CargoLoad{u->cargo_type});
		load.capacity += u->cargo_cap;
		load.stored += u->cargo.StoredCount();
	}
	return loads;
}

/* Only another list holds the stops worth taking, so a vehicle already sharing this one is no source. */
static std::vector<const Vehicle *> OrderSources(const Vehicle &v)
{
	std::vector<const Vehicle *> sources;
	for (const Vehicle *other : Vehicle::Iterate()) {
		if (other->type != v.type || !other->IsPrimaryVehicle() || other->owner != _local_company) continue;
		if (other->index == v.index || other->GetNumOrders() == 0) continue;
		if (v.orders != nullptr && other->orders == v.orders) continue;
		sources.push_back(other);
	}
	return sources;
}

static Money ConsistValue(const Vehicle &v)
{
	Money value = 0;
	for (const Vehicle *u = &v; u != nullptr; u = u->Next()) value += u->value;
	return value;
}

static std::string WeightPowerSpeed(const Vehicle &v)
{
	const GroundVehicleCache &cache = *v.GetGroundVehicleCache();
	int64_t speed = PackVelocity(v.GetDisplayMaxSpeed(), v.type);
	bool without_effort = v.type == VEH_TRAIN && (_settings_game.vehicle.train_acceleration_model == AM_ORIGINAL ||
			Train::From(&v)->GetAccelerationType() == VehicleAccelerationModel::Maglev);
	if (without_effort) return GameText(STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED, cache.cached_weight, cache.cached_power, speed);
	return GameText(STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED_MAX_TE, cache.cached_weight, cache.cached_power, speed, cache.cached_max_te);
}

static bool HasRealisticWeight(const Vehicle &v)
{
	return v.type == VEH_TRAIN || (v.type == VEH_ROAD && _settings_game.vehicle.roadveh_acceleration_model != AM_ORIGINAL);
}

VehiclePanel::VehiclePanel(VehicleID vehicle) :
	WindowPanel(fmt::format("vehicle{}", vehicle.base()), {}, {"상태", GameText(STR_VEHICLE_DETAIL_TAB_CARGO), "주문", GameText(STR_VEHICLE_DETAIL_TAB_INFORMATION)}),
	vehicle(vehicle)
{
}

bool VehiclePanel::IsAlive() const
{
	return Vehicle::IsValidID(this->vehicle);
}

std::optional<CameraShot> VehiclePanel::Camera() const
{
	return CameraShot{CarrierSubject::Vehicle, static_cast<int>(this->vehicle.base()), this->vehicle};
}

bool VehiclePanel::Renamable() const
{
	return Vehicle::Get(this->vehicle)->owner == _local_company;
}

void VehiclePanel::Rename(std::string name)
{
	Command<CMD_RENAME_VEHICLE>::Post(STR_ERROR_CAN_T_RENAME_TRAIN + Vehicle::Get(this->vehicle)->type, this->vehicle, std::move(name));
}

void VehiclePanel::Fill()
{
	const Vehicle &v = *Vehicle::Get(this->vehicle);
	this->title = GameText(STR_VEHICLE_NAME, this->vehicle);
	switch (this->tab) {
		case VT_STATUS: this->FillStatus(v); break;
		case VT_CARGO: this->FillCargo(v); break;
		case VT_ORDERS: this->FillOrders(v); break;
		case VT_INFO: this->FillInfo(v); break;
	}
	this->FillCommands(v);
}

/* Unsticking a vehicle has no other home in the mini UI: a train held at a
 * danger signal or facing the wrong way can only be fixed from here. */
void VehiclePanel::FillStatus(const Vehicle &v)
{
	LedgerSection &status = this->Section();
	status.Add(StatusLine(v));
	status.Add({"속도", fmt::format("{} / {}", v.GetDisplaySpeed(), v.GetDisplayMaxSpeed())});
	status.Add(LedgerLine::Text(GameText(STR_VEHICLE_INFO_RELIABILITY_BREAKDOWNS, v.reliability * 100 >> 16, v.breakdowns_since_last_service)));
	if (v.owner != _local_company) return;

	if (v.type == VEH_TRAIN) {
		status.Add(LedgerLine::Text("방향 전환").OnClick(OnVehicle(v.index, [](const Vehicle &u) {
			Command<CMD_REVERSE_TRAIN_DIRECTION>::Post(STR_ERROR_CAN_T_REVERSE_DIRECTION_TRAIN, u.tile, u.index, false);
		})));
		if (Train::From(&v)->flags.Test(VehicleRailFlag::Stuck)) {
			status.Add(LedgerLine::Text("신호 강행", Tone::Warn).OnClick(OnVehicle(v.index, [](const Vehicle &u) {
				Command<CMD_FORCE_TRAIN_PROCEED>::Post(STR_ERROR_CAN_T_MAKE_TRAIN_PASS_SIGNAL, u.tile, u.index);
			})));
		}
	}
	if (v.type == VEH_ROAD) {
		status.Add(LedgerLine::Text("회차").OnClick(OnVehicle(v.index, [](const Vehicle &u) {
			Command<CMD_TURN_ROADVEH>::Post(STR_ERROR_CAN_T_MAKE_ROAD_VEHICLE_TURN, u.tile, u.index);
		})));
	}
}

void VehiclePanel::FillCargo(const Vehicle &v)
{
	LedgerSection &cargo = this->Section();
	for (const CargoLoad &load : CargoLoads(v)) cargo.Add({GameText(CargoSpec::Get(load.cargo)->name), fmt::format("{} / {}", load.stored, load.capacity)});
	if (cargo.lines.empty()) cargo.Add(LedgerLine::Text("적재 화물 없음", Tone::Dim));
	if (v.owner == _local_company && v.IsStoppedInDepot()) this->FillRefits(v);
}

void VehiclePanel::FillRefits(const Vehicle &v)
{
	CargoTypes mask = RefitMask(v);
	LedgerSection &refits = this->Section("개조");
	for (const CargoSpec *cs : _sorted_cargo_specs) {
		CargoType cargo = cs->Index();
		if (!HasBit(mask, cargo)) continue;
		refits.Add(LedgerLine::Text(GameText(cs->name)).Mark(Carries(v, cargo)).OnClick(OnVehicle(v.index, [cargo](const Vehicle &u) {
			Command<CMD_REFIT_VEHICLE>::Post(GetCmdRefitVehMsg(u.type), u.tile, u.index, cargo, 0xFF, false, false, 0);
		})));
	}
	this->DropEmptySection();
}

void VehiclePanel::FillOrders(const Vehicle &v)
{
	if (this->selected_order.has_value() && *this->selected_order >= v.GetNumOrders()) this->selected_order.reset();

	LedgerSection &orders = this->Section();
	VehicleOrderID index = 0;
	for (const Order &order : v.Orders()) {
		if (!OrderTarget(v, order).empty()) orders.Add(this->OrderLine(v, index, order));
		index++;
	}
	if (v.GetNumOrders() == 0) orders.Add(LedgerLine::Text("주문 없음", Tone::Dim));
	if (v.owner != _local_company) return;

	VehicleID id = v.index;
	bool picking = _mode.OrderVehicle() == id;
	orders.Add(LedgerLine::Text(picking ? "추가 중. 지도에서 목적지 클릭, ESC 종료" : "+ 목적지 추가", Tone::Accent).Mark(picking).OnClick([id] { _mode.TogglePickOrders(id); }));
	if (v.orders != nullptr && v.orders->GetNumVehicles() > 1) {
		orders.Add(LedgerLine::Text("주문 공유 해제", Tone::Warn).OnClick(OnVehicle(id, [](const Vehicle &u) {
			Command<CMD_CLONE_ORDER>::Post(STR_ERROR_CAN_T_STOP_SHARING_ORDER_LIST, u.tile, CO_UNSHARE, u.index, VehicleID::Invalid());
		})));
	}

	if (this->selected_order.has_value()) {
		if (const Order *order = v.GetOrder(*this->selected_order); order != nullptr) this->FillOrderEditor(v, *this->selected_order, *order);
	}
	this->FillOrderSources(v);
}

LedgerLine VehiclePanel::OrderLine(const Vehicle &v, VehicleOrderID index, const Order &order)
{
	LedgerLine line(fmt::format("{}. {}{}", index + 1, OrderTarget(v, order), OrderMarks(order)));
	line.Tint(index == v.cur_real_order_index ? Tone::Warn : Tone::Plain).Mark(this->selected_order == index).OnClick([this, index] {
		this->selected_order = this->selected_order == index ? std::nullopt : std::optional(index);
		this->order_refit_open = false;
	});
	return line;
}

void VehiclePanel::FillOrderEditor(const Vehicle &v, VehicleOrderID index, const Order &order)
{
	LedgerSection &editor = this->Section(fmt::format("{}번 주문", index + 1));
	VehicleID id = v.index;
	editor.Add(LedgerLine::Text("위로").OnClick([this, index] { this->MoveOrder(index, index - 1); }));
	editor.Add(LedgerLine::Text("아래로").OnClick([this, index] { this->MoveOrder(index, index + 1); }));
	editor.Add(LedgerLine::Text("여기로 건너뛰기").OnClick(OnVehicle(id, [index](const Vehicle &u) {
		Command<CMD_SKIP_TO_ORDER>::Post(STR_ERROR_CAN_T_SKIP_TO_ORDER, u.tile, u.index, index);
	})));

	if (order.IsType(OT_GOTO_STATION)) {
		OrderLoadType load = NextLoad(order.GetLoadType());
		OrderUnloadType unload = NextUnload(order.GetUnloadType());
		editor.Add(LedgerLine("적재", GameText(LoadName(order.GetLoadType()))).OnClick([id, index, load] { ModifyOrder(id, index, MOF_LOAD, to_underlying(load)); }));
		editor.Add(LedgerLine("하차", GameText(UnloadName(order.GetUnloadType()))).OnClick([id, index, unload] { ModifyOrder(id, index, MOF_UNLOAD, to_underlying(unload)); }));
	}
	/* Whether a train stops at what it passes is a per-order choice on a shared line, not a global setting. */
	if (order.IsType(OT_GOTO_STATION) && v.IsGroundVehicle()) {
		int non_stop = NonStopIndex(order.GetNonStopType());
		OrderNonStopFlags next = NonStopFlags((non_stop + 1) % static_cast<int>(std::size(NON_STOP_NAMES)));
		editor.Add(LedgerLine("정차", Rml::String(NON_STOP_NAMES[non_stop])).OnClick([id, index, next] { ModifyOrder(id, index, MOF_NON_STOP, next.base()); }));
	}
	/* Spacing a fleet out is what a depot order is normally for, so the row cycles the depot action. */
	if (order.IsType(OT_GOTO_DEPOT)) {
		OrderDepotAction action = DepotAction(order);
		OrderDepotAction next = static_cast<OrderDepotAction>((to_underlying(action) + 1) % to_underlying(OrderDepotAction::End));
		editor.Add(LedgerLine("차고 동작", Rml::String(DEPOT_ACTION_NAMES[to_underlying(action)])).OnClick([id, index, next] { ModifyOrder(id, index, MOF_DEPOT_ACTION, to_underlying(next)); }));
	}
	/* A route that runs loaded both ways needs the vehicle to change what it carries
	 * along the way, so the refit rides on the order instead of on the vehicle. */
	bool refittable = (order.IsType(OT_GOTO_STATION) || order.IsType(OT_GOTO_DEPOT)) && order.GetLoadType() != OrderLoadType::NoLoad && RefitMask(v) != 0;
	if (refittable) {
		editor.Add(LedgerLine("개조", RefitName(order.GetRefitCargo())).Mark(this->order_refit_open).OnClick([this] { this->order_refit_open = !this->order_refit_open; }));
		if (this->order_refit_open) this->AddOrderRefits(editor, v, index, order);
	}

	editor.Add(LedgerLine::Text("삭제", Tone::Loss).OnClick([this, id, index] {
		if (const Vehicle *u = Vehicle::GetIfValid(id); u != nullptr) Command<CMD_DELETE_ORDER>::Post(STR_ERROR_CAN_T_DELETE_THIS_ORDER, u->tile, id, index);
		this->selected_order.reset();
	}));
}

/* Only cargoes the consist can carry are offered, and a station order can leave the choice to the station. */
void VehiclePanel::AddOrderRefits(LedgerSection &editor, const Vehicle &v, VehicleOrderID index, const Order &order)
{
	CargoType current = order.GetRefitCargo();
	auto refit_row = [&](CargoType cargo) {
		editor.Add(LedgerLine::Text(RefitName(cargo)).Mark(current == cargo).OnClick([this, index, cargo] { this->RefitOrder(index, cargo); }));
	};

	refit_row(CARGO_NO_REFIT);
	if (order.IsType(OT_GOTO_STATION)) refit_row(CARGO_AUTO_REFIT);
	CargoTypes mask = RefitMask(v);
	for (const CargoSpec *cs : _sorted_cargo_specs) {
		if (HasBit(mask, cs->Index())) refit_row(cs->Index());
	}
}

/* A fleet runs one route, so taking the list off a vehicle that already has it beats entering the stops again. */
void VehiclePanel::FillOrderSources(const Vehicle &v)
{
	std::vector<const Vehicle *> sources = OrderSources(v);
	if (sources.empty()) return;

	LedgerSection &copies = this->Section("주문 가져오기");
	copies.Add(LedgerLine("방식", this->share_orders ? "공유" : "복사", Tone::Accent).OnClick([this] { this->share_orders = !this->share_orders; }));
	VehicleID id = v.index;
	for (const Vehicle *source : sources) {
		VehicleID from = source->index;
		copies.Add(LedgerLine(GameText(STR_VEHICLE_NAME, from), fmt::format("{}개", source->GetNumOrders())).Tint(Tone::Plain).OnClick([this, id, from] {
			const Vehicle *u = Vehicle::GetIfValid(id);
			if (u == nullptr) return;
			StringID error = this->share_orders ? STR_ERROR_CAN_T_SHARE_ORDER_LIST : STR_ERROR_CAN_T_COPY_ORDER_LIST;
			Command<CMD_CLONE_ORDER>::Post(error, u->tile, this->share_orders ? CO_SHARE : CO_COPY, id, from);
		}));
	}
}

void VehiclePanel::FillInfo(const Vehicle &v)
{
	LedgerSection &info = this->Section();
	info.Add({"구매", fmt::format("{}년", v.build_year.base())});
	info.Add({"가치", GameText(STR_JUST_CURRENCY_LONG, ConsistValue(v))});
	info.Add({"유지비", fmt::format("{}/년", GameText(STR_JUST_CURRENCY_LONG, v.GetDisplayRunningCost()))});
	info.Add({"차령", fmt::format("{}년 / {}년", v.age.base() / CalendarTime::DAYS_IN_LEAP_YEAR, v.max_age.base() / CalendarTime::DAYS_IN_LEAP_YEAR)});
	info.Add(LedgerLine::Text(GameText(STR_VEHICLE_INFO_PROFIT_THIS_YEAR_LAST_YEAR, v.GetDisplayProfitThisYear(), v.GetDisplayProfitLastYear())));
	if (v.type == VEH_TRAIN) info.Add({"총길이", fmt::format("{:.1f}타일", Train::From(&v)->gcache.cached_total_length / static_cast<double>(TILE_SIZE))});
	if (v.owner == _local_company) this->AddServiceInterval(info, v);
	if (HasRealisticWeight(v)) info.Add(LedgerLine::Text(WeightPowerSpeed(v)));
}

/* How often a vehicle services decides how often it breaks down, so the interval is the one setting that belongs on the vehicle. */
void VehiclePanel::AddServiceInterval(LedgerSection &info, const Vehicle &v)
{
	bool percent = v.ServiceIntervalIsPercent();
	bool wallclock = TimerGameEconomy::UsingWallclockUnits();
	uint interval = v.GetServiceInterval();
	uint lowest = percent ? MIN_SERVINT_PERCENT : (wallclock ? MIN_SERVINT_MINUTES : MIN_SERVINT_DAYS);
	uint highest = percent ? MAX_SERVINT_PERCENT : (wallclock ? MAX_SERVINT_MINUTES : MAX_SERVINT_DAYS);
	uint step = percent ? SERVICE_STEP_PERCENT : (wallclock ? SERVICE_STEP_MINUTES : SERVICE_STEP_DAYS);

	std::string shown = percent ? fmt::format("{}%", interval) : fmt::format("{}", interval);
	if (!v.ServiceIntervalIsCustom()) shown += " 기본";
	info.Add({"정비 간격", std::move(shown)});

	VehicleID id = v.index;
	uint longer = std::min(interval + step, highest);
	uint shorter = std::max(interval > step ? interval - step : lowest, lowest);
	info.Add(LedgerLine::Text("간격 늘리기").OnClick([id, longer, percent] { Command<CMD_CHANGE_SERVICE_INT>::Post(id, static_cast<uint16_t>(longer), true, percent); }));
	info.Add(LedgerLine::Text("간격 줄이기").OnClick([id, shorter, percent] { Command<CMD_CHANGE_SERVICE_INT>::Post(id, static_cast<uint16_t>(shorter), true, percent); }));
	if (v.ServiceIntervalIsCustom()) {
		info.Add(LedgerLine::Text("간격 기본값").OnClick([id, interval, percent] { Command<CMD_CHANGE_SERVICE_INT>::Post(id, static_cast<uint16_t>(interval), false, percent); }));
	}
}

void VehiclePanel::FillCommands(const Vehicle &v)
{
	bool own = v.owner == _local_company;
	bool stopped = v.vehstatus.Test(VehState::Stopped);
	VehicleID id = v.index;
	this->commands = {
		{stopped ? "출발" : "정지", own, OnVehicle(id, [](const Vehicle &u) {
			Command<CMD_START_STOP_VEHICLE>::Post(STR_ERROR_CAN_T_STOP_START_TRAIN + u.type, u.tile, u.index, false);
		})},
		{"차고로", own, OnVehicle(id, [](const Vehicle &u) {
			Command<CMD_SEND_VEHICLE_TO_DEPOT>::Post(GetCmdSendToDepotMsg(&u), u.index, _ctrl_pressed ? DepotCommandFlag::Service : DepotCommandFlags{}, {});
		})},
		{"개조", own, [this] { this->SelectTab(VT_CARGO); }},
		{"주문", own, [this] { this->SelectTab(VT_ORDERS); }},
		{"추적", true, [id] { _mode.ToggleFollow(id); }, _mode.FollowedVehicle() == id},
	};
}

void VehiclePanel::MoveOrder(VehicleOrderID from, int to)
{
	const Vehicle *v = Vehicle::GetIfValid(this->vehicle);
	if (v == nullptr || to < 0 || to >= v->GetNumOrders()) return;

	VehicleOrderID target = static_cast<VehicleOrderID>(to);
	if (Command<CMD_MOVE_ORDER>::Post(STR_ERROR_CAN_T_MOVE_THIS_ORDER, v->tile, v->index, from, target)) this->selected_order = target;
}

void VehiclePanel::RefitOrder(VehicleOrderID index, CargoType cargo)
{
	const Vehicle *v = Vehicle::GetIfValid(this->vehicle);
	if (v != nullptr) Command<CMD_ORDER_REFIT>::Post(STR_ERROR_CAN_T_MODIFY_THIS_ORDER, v->tile, v->index, index, cargo);
	this->order_refit_open = false;
}
