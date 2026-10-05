/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file depot_panel.cpp The shared depot workbench: the buy list beside the draft and every depot of one vehicle type. */

#include "../../stdafx.h"
#include "depot_panel.h"

#include "../../cargotype.h"
#include "../../command_func.h"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../../core/format.hpp"
#include "../../core/math_func.hpp"
#include "../../depot_base.h"
#include "../../depot_cmd.h"
#include "../../depot_map.h"
#include "../../engine_base.h"
#include "../../engine_cmd.h"
#include "../../engine_gui.h"
#include "../../mini_ui.h"
#include "../../network/network_type.h"
#include "../../station_base.h"
#include "../../strings_func.h"
#include "../../train.h"
#include "../../train_cmd.h"
#include "../../vehicle_base.h"
#include "../../vehicle_cmd.h"
#include "../../vehicle_func.h"
#include "../../vehiclelist.h"
#include "../core/tones.h"
#include "../fleet/consist_draft.h"
#include "../fleet/fleet_deploy.h"
#include "../ui/ui_text.h"
#include "vehicle_types.h"
#include "window_links.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static constexpr int MIN_BLOCK_WIDTH = 4;

struct BuyRow {
	EngineID engine;
	int64_t score;
	bool hidden;
};

static Rml::String ColourCode(uint32_t argb)
{
	return fmt::format("#{:06x}", argb & 0xFFFFFF);
}

/* A block is as long as the unit it stands for, so a strip reads like the train. */
static StripBlock UnitBlock(int length, uint32_t fill, bool engine, bool active)
{
	return {fmt::format("{}dp", std::max(MIN_BLOCK_WIDTH, 3 * length / 2)), ColourCode(fill), engine, active};
}

static bool IsWagon(const Engine &e)
{
	return e.type == VEH_TRAIN && e.VehInfo<RailVehicleInfo>().railveh_type == RAILVEH_WAGON;
}

static int64_t BuyScore(const Engine &e)
{
	if (e.type != VEH_TRAIN) return static_cast<int64_t>(e.GetDisplayDefaultCapacity()) * 1000 + e.GetDisplayMaxSpeed();
	return IsWagon(e) ? e.GetDisplayDefaultCapacity() : e.GetPower();
}

static bool CanCouple(const Vehicle *marked, const Vehicle &head)
{
	return marked != nullptr && marked->type == VEH_TRAIN && head.type == VEH_TRAIN && marked->First() != head.First() && marked->tile == head.tile;
}

static Rml::Vector<StripBlock> TrainBlocks(const Train &head, VehicleID marked)
{
	Rml::Vector<StripBlock> blocks;
	for (const Train *u = &head; u != nullptr; u = u->GetNextUnit()) {
		uint32_t fill = u->cargo_cap > 0 && IsValidCargoType(u->cargo_type) ? CargoRgb(u->cargo_type) : MINI_CH_TILE;
		blocks.push_back(UnitBlock(std::max<int>(1, u->gcache.cached_veh_length), fill, !IsWagon(*u->GetEngine()), u->index == marked));
	}
	return blocks;
}

static std::vector<VehicleID> TrainUnits(const Train &head)
{
	std::vector<VehicleID> units;
	for (const Train *u = &head; u != nullptr; u = u->GetNextUnit()) units.push_back(u->index);
	return units;
}

static Rml::Vector<StripBlock> DraftBlocks(const ConsistDraft &draft)
{
	Rml::Vector<StripBlock> blocks;
	for (EngineID id : draft.Units()) {
		const Engine &e = *Engine::Get(id);
		CargoType cargo = e.GetDefaultCargoType();
		uint32_t fill = e.GetDisplayDefaultCapacity() > 0 && IsValidCargoType(cargo) ? CargoRgb(cargo) : MINI_CH_TILE;
		blocks.push_back(UnitBlock(UnitLength(&e), fill, e.type == VEH_TRAIN && !IsWagon(e), false));
	}
	return blocks;
}

DepotPanel::DepotPanel() : WindowPanel("depot", "차고", VehicleTypeTabs())
{
	this->wide = true;
}

bool DepotPanel::IsAlive() const
{
	return Company::IsValidID(_local_company);
}

void DepotPanel::Fill()
{
	VehicleType type = this->Type();
	ConsistDraft &draft = FleetDraft(type);
	draft.Prune();
	this->Validate(type);

	this->FillCatalogue(type);
	this->NextColumn();
	this->FillInspected();
	this->FillDraft(type, draft);
	this->FillDepots(type, draft);
	this->FillCommands(type, draft);
}

void DepotPanel::Validate(VehicleType type)
{
	const Vehicle *marked = Vehicle::GetIfValid(this->marked);
	if (marked != nullptr && (marked->type != type || !marked->First()->IsChainInDepot())) this->marked = VehicleID::Invalid();
	const Engine *inspected = Engine::GetIfValid(this->inspected);
	if (inspected != nullptr && (inspected->type != type || !inspected->IsEnabled())) this->inspected = EngineID::Invalid();
}

/* The row under the pointer decides what the spec rows show, and it keeps the last one once the pointer leaves the list. */
void DepotPanel::FillCatalogue(VehicleType type)
{
	std::vector<BuyRow> engines;
	std::vector<BuyRow> wagons;
	int hidden = 0;
	for (const Engine *e : Engine::IterateType(type)) {
		if (!e->IsEnabled() || !e->company_avail.Test(_local_company)) continue;
		bool is_hidden = e->IsHidden(_local_company);
		if (is_hidden) hidden++;
		if (is_hidden && !this->show_hidden) continue;
		(IsWagon(*e) ? wagons : engines).push_back({e->index, BuyScore(*e), is_hidden});
	}
	std::ranges::sort(engines, std::greater{}, &BuyRow::score);
	std::ranges::sort(wagons, std::greater{}, &BuyRow::score);

	if (hidden > 0) {
		std::string toggle = this->show_hidden ? fmt::format("숨긴 엔진 {}종 감추기", hidden) : fmt::format("숨긴 엔진 {}종 보기", hidden);
		this->Section().Add(LedgerLine::Text(std::move(toggle), Tone::Dim)
				.OnClick([this] { this->show_hidden = !this->show_hidden; }));
	}

	auto add_rows = [&](Rml::String title, const std::vector<BuyRow> &rows) {
		if (rows.empty()) return;
		LedgerSection &list = this->Section(std::move(title));
		for (const BuyRow &row : rows) {
			EngineID engine = row.engine;
			Tone tone = row.hidden ? Tone::Dim : Tone::Plain;
			list.Add(LedgerLine(GameText(STR_ENGINE_NAME, engine), GameText(STR_JUST_CURRENCY_LONG, Engine::Get(engine)->GetCost()), tone).Tint(tone)
					.OnClick([type, engine] { FleetDraft(type).Add(engine); })
					.OnHover([this, engine] { this->inspected = engine; }));
		}
	};
	add_rows(type == VEH_TRAIN ? "기관차" : "엔진", engines);
	add_rows("화차", wagons);
	if (engines.empty() && wagons.empty()) this->Section().Add(LedgerLine::Text("구매 가능 엔진 없음", Tone::Dim));
}

void DepotPanel::FillInspected()
{
	const Engine *e = Engine::GetIfValid(this->inspected);
	if (e == nullptr) return;

	EngineID engine = e->index;
	bool hidden = e->IsHidden(_local_company);
	LedgerSection &spec = this->Section(GameText(STR_ENGINE_NAME, engine));
	spec.Add(LedgerLine::Text(StrMakeValid(GetEngineInfoString(engine), {})));
	if (!IsWagon(*e)) spec.Add({"신뢰도", fmt::format("{}%", ToPercent16(e->reliability))});
	spec.Add(LedgerLine::Text(hidden ? "숨김 해제" : "구매 목록에서 숨기기", Tone::Dim).OnClick([engine, hidden] { Command<CMD_SET_VEHICLE_VISIBILITY>::Post(engine, !hidden); }));
}

void DepotPanel::FillDraft(VehicleType type, const ConsistDraft &draft)
{
	LedgerSection &design = this->Section("설계");
	if (draft.Empty()) {
		design.Add(LedgerLine::Text(type == VEH_TRAIN ? "엔진 목록을 눌러 편성 구성" : "엔진 목록을 눌러 선택", Tone::Dim));
	} else {
		design.Add(LedgerLine::Strip(DraftBlocks(draft), [type](size_t unit) { FleetDraft(type).Remove(unit); }));
		DraftSummary sum = draft.Summary();
		design.Add({"합계", GameText(STR_JUST_CURRENCY_LONG, sum.cost), Tone::Accent});
		if (type == VEH_TRAIN) {
			design.Add(LedgerLine::Text(GameText(STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED, sum.weight, sum.power, PackVelocity(sum.speed, type))));
			design.Add({"길이", fmt::format("{:.1f}타일", sum.length / static_cast<double>(TILE_SIZE))});
		}
		if (sum.capacity > 0) design.Add({"용량", fmt::format("{}", sum.capacity)});
		design.Add(LedgerLine::Text("차고 행 클릭으로 생산 · 블록 클릭으로 제외", Tone::Dim));
	}
	if (_deploy.Running(type)) design.Add(LedgerLine::Text(fmt::format("생산 중 {} / {}", _deploy.Unit(), _deploy.Units()), Tone::Accent));
}

void DepotPanel::FillDepots(VehicleType type, const ConsistDraft &draft)
{
	LedgerSection &yard = this->Section("차고");
	const Vehicle *marked = Vehicle::GetIfValid(this->marked);
	if (marked != nullptr && type == VEH_TRAIN) {
		yard.Add(LedgerLine::Text("표시 차량: 같은 차고 편성 클릭으로 연결", Tone::Accent));
		if (marked->First() != marked) {
			VehicleID unit = marked->index;
			TileIndex tile = marked->tile;
			yard.Add(LedgerLine::Text("새 편성으로 분리", Tone::Accent).OnClick([this, unit, tile] {
				Command<CMD_MOVE_RAIL_VEHICLE>::Post(STR_ERROR_CAN_T_MOVE_VEHICLE, tile, unit, VehicleID::Invalid(), false);
				this->marked = VehicleID::Invalid();
			}));
		}
	}

	size_t before = yard.lines.size();
	if (type == VEH_AIRCRAFT) {
		for (const Station *st : Station::Iterate()) {
			if (st->owner != _local_company || !st->facilities.Test(StationFacility::Airport) || !st->airport.HasHangar()) continue;
			this->AddDepot(yard, type, st->airport.GetHangarTile(0), st->index.base(), DepotID::Invalid(), draft);
		}
	} else {
		for (const Depot *d : Depot::Iterate()) {
			if (!IsDepotTile(d->xy) || GetDepotVehicleType(d->xy) != type || GetTileOwner(d->xy) != _local_company) continue;
			this->AddDepot(yard, type, d->xy, d->index.base(), d->index, draft);
		}
	}
	if (yard.lines.size() == before) yard.Add(LedgerLine::Text("차고 없음", Tone::Dim));
}

/* Several depots in one town share a generated name, so the row renames in place.
 * A hangar belongs to its station and has no depot of its own to rename. */
void DepotPanel::AddDepot(LedgerSection &yard, VehicleType type, TileIndex tile, uint destination, DepotID depot, const ConsistDraft &draft)
{
	bool producing = !draft.Empty();
	LedgerLine &name = yard.Add(LedgerLine(GameText(STR_DEPOT_NAME, type, destination), producing ? "생산" : "", Tone::Accent));
	name.Tint(producing ? Tone::Accent : Tone::Plain).OnClick([type, tile] {
		if (!IsDepotTile(tile)) return;
		if (FleetDraft(type).Empty()) {
			ScrollToTile(tile);
		} else {
			_deploy.Start(tile, type, FleetDraft(type).Units());
		}
	});
	if (depot != DepotID::Invalid()) {
		name.Renames(fmt::format("depot{}", tile.base()), [depot](std::string text) {
			if (!text.empty()) Command<CMD_RENAME_DEPOT>::Post(STR_ERROR_CAN_T_RENAME_DEPOT, depot, std::move(text));
		});
	}

	VehicleList chains;
	VehicleList wagons;
	BuildDepotVehicleList(type, tile, &chains, &wagons);
	if (!chains.empty() || !wagons.empty()) {
		/* Emptying a depot cannot be taken back, so the row arms first and sells on the next click. */
		bool armed = this->sell_armed == tile;
		yard.Add(LedgerLine::Text(armed ? "전체 매각 · 다시 눌러 확정" : "전체 매각", armed ? Tone::Loss : Tone::Dim).OnClick([this, type, tile, armed] {
			if (!armed) {
				this->sell_armed = tile;
				return;
			}
			Command<CMD_DEPOT_SELL_ALL_VEHICLES>::Post(GetCmdSellAllVehMsg(type), tile, type);
			this->sell_armed = INVALID_TILE;
			this->marked = VehicleID::Invalid();
		}));
		yard.Add(LedgerLine::Text("전체 교체", Tone::Dim).OnClick([type, tile] { Command<CMD_DEPOT_MASS_AUTOREPLACE>::Post(GetCmdAutoreplaceVehMsg(type), tile, type); }));
	}
	for (const Vehicle *head : chains) this->AddConsist(yard, *head);
	for (const Vehicle *head : wagons) this->AddConsist(yard, *head);
}

/* With a unit marked the row is a coupling target; otherwise it is the way from the yard to the consist's own window. */
void DepotPanel::AddConsist(LedgerSection &yard, const Vehicle &head)
{
	std::vector<VehicleID> units = head.type == VEH_TRAIN ? TrainUnits(*Train::From(&head)) : std::vector<VehicleID>{head.index};
	std::string name = head.IsPrimaryVehicle() ? GameText(STR_VEHICLE_NAME, head.index) : GameText(STR_ENGINE_NAME, head.engine_type);
	if (units.size() > 1) name = fmt::format("{} · {}량", name, units.size());

	VehicleID id = head.index;
	bool stopped = head.vehstatus.Test(VehState::Stopped);
	yard.Add(LedgerLine(std::move(name)).Tint(stopped ? Tone::Plain : Tone::Warn).Mark(this->marked == id).OnClick([this, id] {
		const Vehicle *v = Vehicle::GetIfValid(id);
		if (v == nullptr) return;
		if (!CanCouple(Vehicle::GetIfValid(this->marked), *v) && v->IsPrimaryVehicle()) {
			OpenVehicleWindow(id);
		} else {
			this->MarkUnit(id, true);
		}
	}));
	if (head.type == VEH_TRAIN) {
		yard.Add(LedgerLine::Strip(TrainBlocks(*Train::From(&head), this->marked), [this, units](size_t unit) { this->MarkUnit(units[unit], false); }));
	}
}

void DepotPanel::FillCommands(VehicleType type, const ConsistDraft &draft)
{
	VehicleID marked = Vehicle::IsValidID(this->marked) ? this->marked : VehicleID::Invalid();
	bool has_mark = marked != VehicleID::Invalid();
	this->commands = {
		{"매각", has_mark, [this, marked] {
			const Vehicle *v = Vehicle::GetIfValid(marked);
			if (v == nullptr) return;
			Command<CMD_SELL_VEHICLE>::Post(GetCmdSellVehMsg(v->type), v->tile, v->index, v->type == VEH_TRAIN && v->First() == v, true, INVALID_CLIENT_ID);
			this->marked = VehicleID::Invalid();
		}},
		{"설계 비우기", !draft.Empty(), [type] { FleetDraft(type).Clear(); }},
		{"복제", has_mark, [marked] {
			const Vehicle *v = Vehicle::GetIfValid(marked);
			if (v != nullptr) Command<CMD_CLONE_VEHICLE>::Post(GetCmdBuildVehMsg(v->type), v->tile, v->First()->index, false);
		}},
	};
}

void DepotPanel::MarkUnit(VehicleID target, bool attach)
{
	const Vehicle *marked = Vehicle::GetIfValid(this->marked);
	const Vehicle *unit = Vehicle::GetIfValid(target);
	if (unit == nullptr) {
		this->marked = VehicleID::Invalid();
	} else if (attach && CanCouple(marked, *unit)) {
		Command<CMD_MOVE_RAIL_VEHICLE>::Post(STR_ERROR_CAN_T_MOVE_VEHICLE, marked->tile, marked->index, unit->Last()->index, marked->First() == marked);
		this->marked = VehicleID::Invalid();
	} else {
		this->marked = this->marked == target ? VehicleID::Invalid() : target;
	}
}
