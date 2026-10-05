/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file station_list_panel.cpp The company's stations, ranked; doubles as the stop picker while orders are added. */

#include "../../stdafx.h"
#include "station_list_panel.h"

#include "../../cargotype.h"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../../core/format.hpp"
#include "../../core/math_func.hpp"
#include "../../station_base.h"
#include "../input/input_mode.h"
#include "../ui/ui_text.h"
#include "grades.h"
#include "window_links.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

static void PickStation(StationID station)
{
	const Station *st = Station::GetIfValid(station);
	if (st == nullptr) return;
	if (!_mode.AppendOrder(st->xy)) OpenStationWindow(station);
}

static DirectoryEntry StationEntry(const Station &st)
{
	DirectoryEntry entry;
	entry.name = GameText(STR_STATION_NAME, st.index);

	uint rating_sum = 0;
	uint rated = 0;
	for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
		const GoodsEntry &goods = st.goods[cs->Index()];
		entry.amount += goods.TotalCount();
		if (!goods.HasRating()) continue;
		rating_sum += goods.rating;
		rated++;
	}
	if (rated > 0) {
		uint percent = ToPercent8(rating_sum / rated);
		entry.grade = percent;
		entry.grade_text = fmt::format("{}%", percent);
		entry.grade_tone = ShareTone(percent);
	}

	StationID station = st.index;
	entry.open = [station] { PickStation(station); };
	return entry;
}

StationListPanel::StationListPanel() : DirectoryPanel("station-list", "역 목록", "화물", "평가")
{
}

bool StationListPanel::IsAlive() const
{
	return Company::IsValidID(_local_company);
}

void StationListPanel::Fill()
{
	DirectoryPanel::Fill();
	if (_mode.PickingOrders()) this->Section().Add(LedgerLine::Text("행 클릭으로 목적지 추가", Tone::Accent));
}

std::vector<DirectoryEntry> StationListPanel::Entries() const
{
	std::vector<DirectoryEntry> entries;
	for (const Station *st : Station::Iterate()) {
		if (st->owner == _local_company) entries.push_back(StationEntry(*st));
	}
	return entries;
}

Rml::String StationListPanel::Emptiness() const
{
	return "역 없음";
}
