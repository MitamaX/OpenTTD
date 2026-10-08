/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file colony_panel.cpp The top-left corner: year gauge, company, money and the time controls. */

#include "../../stdafx.h"
#include "colony_panel.h"

#include <RmlUi/Core.h>

#include "../../command_func.h"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../../core/format.hpp"
#include "../../gfx_func.h"
#include "../../misc_cmd.h"
#include "../../network/network.h"
#include "../../openttd.h"
#include "../../timer/timer_game_calendar.h"
#include "../ui/pixel_style.h"
#include "../ui/tone.h"
#include "../ui/ui_text.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

enum SpeedChoice : int {
	SPEED_PAUSE,
	SPEED_PLAY,
	SPEED_FAST,
};

static constexpr const char *SPEED_ICONS[] = {"icons/pause.svg", "icons/play.svg", "icons/fast.svg"};
static constexpr int GAUGE_SEGMENTS = 28;
static constexpr double DEGREES_PER_TURN = 360.0;
static constexpr double DAYS_PER_MONTH = 31.0;
static constexpr double MONTHS_PER_YEAR = 12.0;
static constexpr uint16_t NORMAL_GAME_SPEED = 100;
static constexpr float WIDTH_STEP_DP = 8.0f;
static constexpr std::chrono::milliseconds FUNDS_INTERVAL{250};

ColonyPanel::ColonyPanel() : HudPart("colony"), funds_beat(FUNDS_INTERVAL)
{
	for (int i = 0; i < GAUGE_SEGMENTS; i++) {
		this->gauge.push_back({fmt::format("rotate({:.2f}deg)", DEGREES_PER_TURN * (i + 0.5) / GAUGE_SEGMENTS)});
	}
	for (const char *icon : SPEED_ICONS) this->speeds.push_back({icon});
}

void ColonyPanel::Bind(Rml::DataModelConstructor &model)
{
	if (Rml::StructHandle<GaugeSegment> segment = model.RegisterStruct<GaugeSegment>()) {
		segment.RegisterMember("angle", &GaugeSegment::angle);
		segment.RegisterMember("done", &GaugeSegment::done);
	}
	model.RegisterArray<Rml::Vector<GaugeSegment>>();
	if (Rml::StructHandle<SpeedButton> speed = model.RegisterStruct<SpeedButton>()) {
		speed.RegisterMember("icon", &SpeedButton::icon);
		speed.RegisterMember("active", &SpeedButton::active);
		speed.RegisterMember("off", &SpeedButton::off);
	}
	model.RegisterArray<Rml::Vector<SpeedButton>>();

	this->Expose(model, "name", &this->name);
	this->Expose(model, "date", &this->date);
	this->Expose(model, "money", &this->money);
	this->Expose(model, "vehicles", &this->vehicles);
	this->Expose(model, "warning", &this->warning);
	this->Expose(model, "funds_tone", &this->funds_tone);
	this->Expose(model, "gauge", &this->gauge);
	this->Expose(model, "speeds", &this->speeds);
	model.BindEventCallback("speed", &ColonyPanel::SetSpeed, this);
}

void ColonyPanel::Collect()
{
	const Company *company = Company::GetIfValid(_local_company);
	this->name = company != nullptr ? GameText(STR_COMPANY_NAME, company->index) : Rml::String();
	this->date = GameText(STR_JUST_DATE_LONG, TimerGameCalendar::date);
	if (this->funds_beat.Due()) this->CollectFunds(company);

	this->CollectGauge();
	this->CollectSpeeds();
	this->HoldWidth();
}

/* Running out of money ends the game, so the panel says so rather than
 * leaving the balance to be read as just another number. */
void ColonyPanel::CollectFunds(const Company *company)
{
	this->money.clear();
	this->vehicles.clear();
	this->warning.clear();
	this->funds_tone = ToneName(Tone::Plain);
	if (company == nullptr) return;

	uint count = 0;
	for (int t = 0; t < VEH_COMPANY_END; t++) count += company->group_all[t].num_vehicle;

	this->money = GameText(STR_JUST_CURRENCY_LONG, company->money);
	this->vehicles = fmt::format("VEH {}", count);
	if (company->months_of_bankruptcy > 0) this->warning = fmt::format("경고 {}", company->months_of_bankruptcy);
	this->funds_tone = ToneName(company->money < 0 ? Tone::Loss : (company->months_of_bankruptcy > 0 ? Tone::Warn : Tone::Plain));
}

/* The ring fills clockwise from the top as the calendar year runs. */
void ColonyPanel::CollectGauge()
{
	TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::date);
	double year_done = (ymd.month + (ymd.day - 1) / DAYS_PER_MONTH) / MONTHS_PER_YEAR;
	for (int i = 0; i < GAUGE_SEGMENTS; i++) this->gauge[i].done = (i + 0.5) / GAUGE_SEGMENTS < year_done;
}

void ColonyPanel::CollectSpeeds()
{
	int active = _pause_mode.Any() ? SPEED_PAUSE : (_game_speed == NORMAL_GAME_SPEED ? SPEED_PLAY : SPEED_FAST);
	for (int i = 0; i < static_cast<int>(this->speeds.size()); i++) {
		this->speeds[i].active = i == active;
		this->speeds[i].off = i == SPEED_FAST && _networking;
	}
}

/* Text widths shift every tick; a quantised width that never shrinks keeps
 * the panel from breathing. */
void ColonyPanel::HoldWidth()
{
	Rml::Element *root = this->Root();
	if (root == nullptr) return;

	float step = WIDTH_STEP_DP * root->GetContext()->GetDensityIndependentPixelRatio();
	float width = std::ceil(root->GetBox().GetSize(Rml::BoxArea::Content).x / step) * step;
	this->held_width = std::max(this->held_width, width);
	SetPixels(*root, Rml::PropertyId::MinWidth, this->held_width);
}

static void Resume()
{
	if (_pause_mode.Test(PauseMode::Normal)) Command<CMD_PAUSE>::Post(PauseMode::Normal, false);
}

void ColonyPanel::SetSpeed(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	switch (ArgumentIndex(arguments)) {
		case SPEED_PAUSE:
			Command<CMD_PAUSE>::Post(PauseMode::Normal, !_pause_mode.Test(PauseMode::Normal));
			break;

		case SPEED_PLAY:
			Resume();
			ChangeGameSpeed(false);
			break;

		case SPEED_FAST:
			if (_networking) break;
			Resume();
			ChangeGameSpeed(true);
			break;

		default:
			break;
	}
}
