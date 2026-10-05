/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town_panel.cpp A town: growth, the local authority, cargo and the official cargo graph. */

#include "../../stdafx.h"
#include "town_panel.h"

#include "../../cargotype.h"
#include "../../command_func.h"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../../core/format.hpp"
#include "../../core/math_func.hpp"
#include "../../economy_func.h"
#include "../../graph_gui.h"
#include "../../landscape.h"
#include "../../settings_type.h"
#include "../../tile_map.h"
#include "../../timer/timer_game_economy.h"
#include "../../timer/timer_game_tick.h"
#include "../../town_cmd.h"
#include "../../window_type.h"
#include "../ui/ui_text.h"
#include "grades.h"
#include "window_links.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

enum TownTab : int {
	TT_GROWTH,
	TT_AUTHORITY,
	TT_CARGO,
	TT_CARGO_GRAPH,
};

static constexpr TownProductionEffect TOWN_PRODUCTION_EFFECTS[] = {TPE_PASSENGERS, TPE_MAIL};

static void OpenCargoGraph(WindowNumber number)
{
	ShowTownCargoGraph(number);
}

static TownActions EnabledTownActions()
{
	TownActions enabled{};
	enabled.Set();
	if (!_settings_game.economy.fund_roads) enabled.Reset(TownAction::RoadRebuild);
	if (!_settings_game.economy.fund_buildings) enabled.Reset(TownAction::FundBuildings);
	if (!_settings_game.economy.exclusive_rights) enabled.Reset(TownAction::BuyRights);
	if (!_settings_game.economy.bribe) enabled.Reset(TownAction::Bribe);
	return enabled;
}

static Money TownActionPrice(TownAction action)
{
	return _price[PR_TOWN_ACTION] * GetTownActionCost(action) >> 8;
}

/* Snow and desert towns only ask for their cargo where the climate makes them need it. */
static bool NeedsGrowthCargo(const Town &town, uint goal)
{
	if (goal == 0) return false;
	if (goal == TOWN_GROWTH_WINTER) return TileHeight(town.xy) >= LowestSnowLine() && town.cache.population > 90;
	if (goal == TOWN_GROWTH_DESERT) return GetTropicZone(town.xy) == TROPICZONE_DESERT && town.cache.population > 60;
	return true;
}

static LedgerLine GrowthCargoLine(const Town &town, TownAcceptanceEffect effect, const CargoSpec &cargo)
{
	uint goal = town.goal[effect];
	uint received = town.received[effect].old_act;
	if (goal == TOWN_GROWTH_WINTER || goal == TOWN_GROWTH_DESERT) {
		bool supplied = received > 0;
		return {GameText(cargo.name), supplied ? "공급됨" : "필요", supplied ? Tone::Plain : Tone::Warn};
	}
	return {GameText(cargo.name), fmt::format("{} / {}", received, goal), received >= goal ? Tone::Plain : Tone::Warn};
}

TownPanel::TownPanel(TownID town) :
	WindowPanel(fmt::format("town{}", town.base()), {}, {"상태", "당국", GameText(STR_VEHICLE_DETAIL_TAB_INFORMATION), "화물"}),
	town(town)
{
}

bool TownPanel::IsAlive() const
{
	return Town::IsValidID(this->town);
}

std::optional<CameraShot> TownPanel::Camera() const
{
	return CameraShot{CarrierSubject::Town, this->town.base(), Town::Get(this->town)->xy};
}

std::optional<EmbedTarget> TownPanel::Embed() const
{
	if (this->tab != TT_CARGO_GRAPH) return std::nullopt;
	return EmbedTarget{{WC_TOWN_CARGO_GRAPH, OpenCargoGraph}, this->town};
}

bool TownPanel::Renamable() const
{
	return true;
}

void TownPanel::Rename(std::string name)
{
	Command<CMD_RENAME_TOWN>::Post(STR_ERROR_CAN_T_RENAME_TOWN, this->town, std::move(name));
}

void TownPanel::Fill()
{
	const Town &town = *Town::Get(this->town);
	this->title = GameText(STR_TOWN_NAME, this->town);
	switch (this->tab) {
		case TT_GROWTH: this->FillGrowth(town); break;
		case TT_AUTHORITY: this->FillAuthority(town); break;
		case TT_CARGO: this->FillCargo(town); break;
		default: break;
	}
	this->FillCommands(town);
}

void TownPanel::FillGrowth(const Town &town)
{
	LedgerSection &growth = this->Section();
	growth.Add(LedgerLine::Text(GameText(STR_TOWN_VIEW_POPULATION_HOUSES, town.cache.population, town.cache.num_houses)));
	if (town.flags.Test(TownFlag::IsGrowing)) {
		StringID pace = town.fund_buildings_months == 0 ? STR_TOWN_VIEW_TOWN_GROWS_EVERY : STR_TOWN_VIEW_TOWN_GROWS_EVERY_FUNDED;
		growth.Add(LedgerLine::Text(GameText(pace, RoundDivSU(town.growth_rate + 1, Ticks::DAY_TICKS))));
	} else {
		growth.Add(LedgerLine::Text(GameText(STR_TOWN_VIEW_TOWN_GROW_STOPPED), Tone::Warn));
	}
	if (town.larger_town) growth.Add(LedgerLine::Text("대도시", Tone::Accent));
	if (_settings_game.economy.station_noise_level) {
		growth.Add(LedgerLine::Text(GameText(STR_TOWN_VIEW_NOISE_IN_TOWN, town.noise_reached, town.MaxTownNoise())));
	}
}

void TownPanel::FillAuthority(const Town &town)
{
	LedgerSection &ratings = this->Section();
	for (const Company *c : Company::Iterate()) {
		bool exclusive = town.exclusivity == c->index;
		if (!town.have_ratings.Test(c->index) && !exclusive) continue;
		int rating = town.ratings[c->index];
		std::string name = GameText(STR_COMPANY_NAME, c->index);
		if (exclusive) name += " · 독점";
		ratings.Add({std::move(name), GameText(TownRatingString(rating)), TownRatingTone(rating)});
	}
	if (ratings.lines.empty()) ratings.Add(LedgerLine::Text("회사 평가 없음", Tone::Dim));

	LedgerSection &actions = this->Section(GameText(STR_LOCAL_AUTHORITY_ACTIONS_TITLE));
	TownActions enabled = EnabledTownActions();
	TownActions available = GetMaskOfTownActions(_local_company, &town);
	for (TownAction action = {}; action != TownAction::End; ++action) {
		if (enabled.Test(action)) actions.Add(this->ActionLine(action, available.Test(action)));
	}
}

LedgerLine TownPanel::ActionLine(TownAction action, bool available)
{
	Tone tone = available ? Tone::Plain : Tone::Dim;
	LedgerLine line(GameText(STR_LOCAL_AUTHORITY_ACTION_SMALL_ADVERTISING_CAMPAIGN + to_underlying(action)), GameText(STR_JUST_CURRENCY_LONG, TownActionPrice(action)), tone);
	line.Tint(tone).Mark(this->chosen == action);
	if (available) line.OnClick([this, action] { this->chosen = this->chosen == action ? std::nullopt : std::optional(action); });
	return line;
}

void TownPanel::FillCargo(const Town &town)
{
	StringID last = TimerGameEconomy::UsingWallclockUnits() ? STR_TOWN_VIEW_CARGO_LAST_MINUTE_MAX : STR_TOWN_VIEW_CARGO_LAST_MONTH_MAX;
	LedgerSection &supplied = this->Section();
	for (TownProductionEffect effect : TOWN_PRODUCTION_EFFECTS) {
		for (const CargoSpec *cs : CargoSpec::town_production_cargoes[effect]) {
			auto it = town.GetCargoSupplied(cs->Index());
			bool known = it != std::end(town.supplied);
			uint transported = known ? it->history[LAST_MONTH].transported : 0;
			uint production = known ? it->history[LAST_MONTH].production : 0;
			supplied.Add(LedgerLine::Text(GameText(last, 1ULL << cs->Index(), transported, production)));
		}
	}

	LedgerSection &growth = this->Section(GameText(STR_TOWN_VIEW_CARGO_FOR_TOWNGROWTH));
	for (int i = TAE_BEGIN; i < TAE_END; i++) {
		TownAcceptanceEffect effect = static_cast<TownAcceptanceEffect>(i);
		if (!NeedsGrowthCargo(town, town.goal[effect])) continue;
		if (const CargoSpec *cargo = FindFirstCargoWithTownAcceptanceEffect(effect); cargo != nullptr) growth.Add(GrowthCargoLine(town, effect, *cargo));
	}
	if (growth.lines.empty()) this->sections.pop_back();
}

void TownPanel::FillCommands(const Town &town)
{
	bool doable = this->chosen.has_value() && GetMaskOfTownActions(_local_company, &town).Test(*this->chosen);
	TownAction action = this->chosen.value_or(TownAction::End);
	TileIndex xy = town.xy;
	TownID id = town.index;
	this->commands = {
		{"실행", doable, [this, xy, id, action] { Command<CMD_DO_TOWN_ACTION>::Post(STR_ERROR_CAN_T_DO_THIS, xy, id, action); this->chosen.reset(); }},
		{"이동", true, [xy] { ScrollToTile(xy); }},
	};
}
