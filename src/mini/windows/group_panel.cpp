/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file group_panel.cpp The fleet workbench: groups and autoreplace beside the vehicles of the chosen group. */

#include "../../stdafx.h"
#include "group_panel.h"

#include "../../autoreplace_cmd.h"
#include "../../autoreplace_func.h"
#include "../../command_func.h"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../../core/format.hpp"
#include "../../engine_base.h"
#include "../../group.h"
#include "../../group_cmd.h"
#include "../../vehicle_base.h"
#include "../../vehicle_cmd.h"
#include "../../vehicle_func.h"
#include "../../vehiclelist.h"
#include "../ui/ui_text.h"
#include "vehicle_types.h"
#include "window_links.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static bool IsSpecial(GroupID group)
{
	return group == ALL_GROUP || group == DEFAULT_GROUP;
}

static bool IsFleetMember(const Vehicle &v, VehicleType type)
{
	return v.type == type && v.IsPrimaryVehicle() && v.owner == _local_company;
}

static LedgerLine Toggle(Rml::String label, bool on, LedgerLine::Action flip)
{
	LedgerLine line(std::move(label), on ? "켜짐" : "꺼짐", on ? Tone::Warn : Tone::Plain);
	line.OnClick(std::move(flip));
	return line;
}

static void SetFlag(GroupID group, GroupFlag flag, bool on)
{
	Command<CMD_SET_GROUP_FLAG>::Post(group, flag, on, false);
}

static void MoveToGroup(GroupID group, VehicleID vehicle)
{
	Command<CMD_ADD_VEHICLE_GROUP>::Post(STR_ERROR_GROUP_CAN_T_ADD_VEHICLE, group, vehicle, false, VehicleListIdentifier{});
}

GroupPanel::GroupPanel() : WindowPanel("groups", "차량군", VehicleTypeTabs())
{
	this->wide = true;
}

bool GroupPanel::IsAlive() const
{
	return Company::IsValidID(_local_company);
}

void GroupPanel::Fill()
{
	VehicleType type = this->Type();
	this->Validate(type);
	this->FillGroups(type);
	this->FillReplacement(type, *Company::Get(_local_company));
	this->NextColumn();
	if (IsSpecial(this->group)) {
		this->FillFleet(type);
	} else {
		this->FillMembers(type);
	}
	this->FillCommands(type);
}

void GroupPanel::Validate(VehicleType type)
{
	const Group *g = Group::GetIfValid(this->group);
	if (!IsSpecial(this->group) && (g == nullptr || g->owner != _local_company || g->vehicle_type != type)) this->group = ALL_GROUP;
	const Engine *e = Engine::GetIfValid(this->engine);
	if (e != nullptr && e->type != type) this->engine = EngineID::Invalid();
}

void GroupPanel::FillGroups(VehicleType type)
{
	std::vector<const Group *> groups;
	for (const Group *g : Group::Iterate()) {
		if (g->owner == _local_company && g->vehicle_type == type) groups.push_back(g);
	}
	std::ranges::sort(groups, std::less{}, &Group::number);

	LedgerSection &list = this->Section("그룹");
	list.Add(this->GroupLine(ALL_GROUP, GameText(STR_GROUP_ALL_TRAINS + type), GetGroupNumVehicle(_local_company, ALL_GROUP, type)));
	list.Add(this->GroupLine(DEFAULT_GROUP, GameText(STR_GROUP_DEFAULT_TRAINS + type), GetGroupNumVehicle(_local_company, DEFAULT_GROUP, type)));
	for (const Group *g : groups) {
		GroupID id = g->index;
		list.Add(this->GroupLine(id, GameText(STR_GROUP_NAME, id), GetGroupNumVehicle(_local_company, id, type)).Renames(fmt::format("group{}", id.base()), [id](std::string name) {
			if (!name.empty()) Command<CMD_ALTER_GROUP>::Post(STR_ERROR_GROUP_CAN_T_RENAME, AlterGroupMode::Rename, id, GroupID::Invalid(), std::move(name));
		}));
	}
}

LedgerLine GroupPanel::GroupLine(GroupID group, Rml::String name, uint count)
{
	LedgerLine line(std::move(name), fmt::format("{}대", count));
	line.Tint(Tone::Plain).Mark(this->group == group).OnClick([this, group] {
		this->group = group;
		this->engine = EngineID::Invalid();
	});
	return line;
}

/* The rules apply to the chosen group; a chosen group can also be kept out of a fleet-wide replacement. */
void GroupPanel::FillReplacement(VehicleType type, const Company &company)
{
	LedgerSection &replace = this->Section("자동 교체");
	if (const Group *g = Group::GetIfValid(this->group); g != nullptr) {
		GroupID id = g->index;
		bool protect = g->flags.Test(GroupFlag::ReplaceProtection);
		replace.Add(Toggle("교체 보호", protect, [id, protect] { SetFlag(id, GroupFlag::ReplaceProtection, !protect); }));
		if (type == VEH_TRAIN) {
			bool wagons = g->flags.Test(GroupFlag::ReplaceWagonRemoval);
			replace.Add(Toggle("화차 제거", wagons, [id, wagons] { SetFlag(id, GroupFlag::ReplaceWagonRemoval, !wagons); }));
		}
	}

	size_t owned_start = replace.lines.size();
	for (const Engine *e : Engine::IterateType(type)) {
		uint count = GetGroupNumEngines(_local_company, this->group, e->index);
		EngineID replacement = EngineReplacementForCompany(&company, e->index, this->group);
		if (count == 0 && replacement == EngineID::Invalid()) continue;

		std::string name = GameText(STR_ENGINE_NAME, e->index);
		if (replacement != EngineID::Invalid()) name = fmt::format("{} → {}", name, GameText(STR_ENGINE_NAME, replacement));
		EngineID id = e->index;
		Tone tone = replacement != EngineID::Invalid() ? Tone::Warn : Tone::Plain;
		replace.Add(LedgerLine(std::move(name), fmt::format("{}대", count)).Tint(tone).Mark(this->engine == id).OnClick([this, id] {
			this->engine = this->engine == id ? EngineID::Invalid() : id;
		}));
	}
	if (replace.lines.size() == owned_start) replace.Add(LedgerLine::Text("보유 엔진 없음", Tone::Dim));

	if (Engine::GetIfValid(this->engine) == nullptr) return;
	GroupID group = this->group;
	EngineID from = this->engine;
	EngineID current = EngineReplacementForCompany(&company, from, group);
	if (current != EngineID::Invalid()) {
		replace.Add(LedgerLine::Text("교체 해제", Tone::Loss).OnClick([group, from] { Command<CMD_SET_AUTOREPLACE>::Post(group, from, EngineID::Invalid(), false); }));
	}
	replace.Add(LedgerLine::Text("교체할 새 엔진 클릭", Tone::Dim));
	for (const Engine *e : Engine::IterateType(type)) {
		if (e->index == from || !CheckAutoreplaceValidity(from, e->index, _local_company)) continue;
		EngineID to = e->index;
		replace.Add(LedgerLine::Text(GameText(STR_ENGINE_NAME, to)).Mark(to == current).OnClick([group, from, to] { Command<CMD_SET_AUTOREPLACE>::Post(group, from, to, false); }));
	}
}

/* A fleet is managed by finding the losers, so the list carries this year's profit and puts the worst first. */
void GroupPanel::FillFleet(VehicleType type)
{
	std::vector<const Vehicle *> fleet;
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (!IsFleetMember(*v, type)) continue;
		if (this->group == DEFAULT_GROUP && v->group_id != DEFAULT_GROUP) continue;
		fleet.push_back(v);
	}
	std::ranges::sort(fleet, std::less{}, &Vehicle::GetDisplayProfitThisYear);

	LedgerSection &list = this->Section("차량");
	for (const Vehicle *v : fleet) {
		Money profit = v->GetDisplayProfitThisYear();
		VehicleID head = v->First()->index;
		list.Add(LedgerLine(GameText(STR_VEHICLE_NAME, v->index), GameText(STR_JUST_CURRENCY_SHORT, profit), profit < 0 ? Tone::Loss : Tone::Plain)
				.Tint(Tone::Plain).OnClick([head] { OpenVehicleWindow(head); }));
	}
	if (list.lines.empty()) list.Add(LedgerLine::Text("차량 없음", Tone::Dim));
}

void GroupPanel::FillMembers(VehicleType type)
{
	GroupID group = this->group;
	LedgerSection &members = this->Section("소속 차량. 클릭으로 제외");
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (!IsFleetMember(*v, type) || v->group_id != group) continue;
		VehicleID id = v->index;
		members.Add(LedgerLine::Text(GameText(STR_VEHICLE_NAME, id)).OnClick([id] { MoveToGroup(DEFAULT_GROUP, id); }));
	}
	if (members.lines.empty()) members.Add(LedgerLine::Text("소속 차량 없음", Tone::Dim));

	LedgerSection &others = this->Section("클릭으로 추가");
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (!IsFleetMember(*v, type) || v->group_id == group) continue;
		VehicleID id = v->index;
		others.Add(LedgerLine::Text(GameText(STR_VEHICLE_NAME, id)).OnClick([group, id] { if (Group::IsValidID(group)) MoveToGroup(group, id); }));
	}
	if (others.lines.empty()) others.Add(LedgerLine::Text("없음", Tone::Dim));
}

/* Sending the group to depot is what sets its replacement rules off. */
void GroupPanel::FillCommands(VehicleType type)
{
	GroupID group = this->group;
	VehicleListIdentifier scope(VL_GROUP_LIST, type, _local_company, group);
	this->commands = {
		{"새 그룹", true, [type] { Command<CMD_CREATE_GROUP>::Post(STR_ERROR_GROUP_CAN_T_CREATE, type, GroupID::Invalid()); }},
		{"그룹 삭제", Group::IsValidID(group), [this, group] {
			Command<CMD_DELETE_GROUP>::Post(STR_ERROR_GROUP_CAN_T_DELETE, group);
			this->group = ALL_GROUP;
		}},
		{"전체 출발", true, [scope] { Command<CMD_MASS_START_STOP>::Post(TileIndex{}, true, true, scope); }},
		{"전체 정지", true, [scope] { Command<CMD_MASS_START_STOP>::Post(TileIndex{}, false, true, scope); }},
		{"전체 차고로", true, [type, scope] { Command<CMD_SEND_VEHICLE_TO_DEPOT>::Post(GetCmdSendToDepotMsg(type), VehicleID::Invalid(), DepotCommandFlag::MassSend, scope); }},
	};
}
