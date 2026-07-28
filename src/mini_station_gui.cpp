/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mini_station_gui.cpp Station window in the reference structure: a description strip, tabbed detail sections and icon commands. */

#include "stdafx.h"

#include "cargotype.h"
#include "command_func.h"
#include "company_func.h"
#include "core/math_func.hpp"
#include "industry.h"
#include "map_func.h"
#include "mini_ui.h"
#include "order_base.h"
#include "station_base.h"
#include "station_cmd.h"
#include "strings_func.h"
#include "textbuf_gui.h"
#include "timer/timer_game_calendar.h"
#include "vehicle_base.h"
#include "vehicle_gui.h"
#include "vehiclelist.h"
#include "viewport_func.h"
#include "window_gui.h"

#include "widgets/station_widget.h"

#include "table/strings.h"

#include "safeguards.h"

static constexpr WidgetID WID_MS_DESC = WID_SV_CATCHMENT + 1;
static constexpr WidgetID WID_MS_TAB_STATUS = WID_SV_CATCHMENT + 2;
static constexpr WidgetID WID_MS_TAB_INFO = WID_SV_CATCHMENT + 3;
static constexpr WidgetID WID_MS_TAB_INDUSTRY = WID_SV_CATCHMENT + 4;
static constexpr WidgetID WID_MS_TAB_VEHICLES = WID_SV_CATCHMENT + 5;
static constexpr WidgetID WID_MS_BODY = WID_SV_CATCHMENT + 6;

static constexpr NWidgetPart _nested_mini_station_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_SV_CAPTION), SetStringTip(STR_JUST_STRING, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_SV_RENAME), SetAspect(WidgetDimensions::ASPECT_RENAME), SetSpriteTip(SPR_RENAME, STR_STATION_VIEW_EDIT_TOOLTIP),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_MS_DESC), SetMinimalSize(260, 0), SetMinimalTextLines(1, WidgetDimensions::unscaled.framerect.Vertical()), SetFill(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_MS_TAB_STATUS), SetStringTip(STR_JUST_STRING), SetFill(1, 0),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_MS_TAB_INFO), SetStringTip(STR_JUST_STRING), SetFill(1, 0),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_MS_TAB_INDUSTRY), SetStringTip(STR_JUST_STRING), SetFill(1, 0),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_MS_TAB_VEHICLES), SetStringTip(STR_JUST_STRING), SetFill(1, 0),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_MS_BODY), SetMinimalTextLines(10, WidgetDimensions::unscaled.framerect.Vertical()), SetFill(1, 1), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_SV_CATCHMENT), SetAspect(WidgetDimensions::ASPECT_LOCATION), SetStringTip(STR_JUST_STRING, STR_TOOLTIP_CATCHMENT), SetFill(0, 0),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_SV_LOCATION), SetAspect(WidgetDimensions::ASPECT_LOCATION), SetSpriteTip(SPR_GOTO_LOCATION, STR_STATION_VIEW_CENTER_TOOLTIP),
		NWidget(NWID_SPACER), SetFill(1, 0),
	EndContainer(),
};

struct MiniStationWindow : Window {
	enum Tab : uint8_t {
		TAB_STATUS,
		TAB_INFO,
		TAB_INDUSTRY,
		TAB_VEHICLES,
	};

	uint8_t tab = TAB_STATUS;

	/* Jump targets of the drawn body rows, top to bottom; INVALID_TILE and
	 * VehicleID::Invalid() mark rows without one. */
	mutable std::vector<TileIndex> industry_rows;
	mutable std::vector<VehicleID> vehicle_rows;

	MiniStationWindow(WindowDesc &desc, WindowNumber number) : Window(desc)
	{
		this->InitNested(number);
		this->owner = Station::Get(this->window_number)->owner;
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		SetViewportCatchmentStation(Station::Get(this->window_number), false);
		this->Window::Close();
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_SV_CAPTION: return GetString(STR_STATION_NAME, this->window_number);
			case WID_MS_TAB_STATUS: return "상태";
			case WID_MS_TAB_INFO: return "정보";
			case WID_MS_TAB_INDUSTRY: return "산업";
			case WID_MS_TAB_VEHICLES: return "차량";
			case WID_SV_CATCHMENT: return std::string();
			default: return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void DrawStatusRows(const Rect &ir, const Station *st) const
	{
		int lh = GetCharacterHeight(FS_NORMAL);
		int y = ir.top;
		bool any = false;

		for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
			const GoodsEntry &ge = st->goods[cs->Index()];
			if (!ge.HasRating()) continue;
			if (y + lh - 1 > ir.bottom) break;
			any = true;
			uint pct = ToPercent8(ge.rating);
			TextColour tc = pct < 25 ? TC_RED : pct < 50 ? TC_YELLOW : TC_BLACK;
			DrawString(ir.left, ir.right, y, fmt::format("{}  대기 {} · 등급 {}%", GetString(cs->name), ge.TotalCount(), pct), tc);
			y += lh;
		}
		if (!any) DrawString(ir.left, ir.right, y, "대기 화물 없음", TC_GREY);
	}

	void DrawInfoRows(const Rect &ir, const Station *st) const
	{
		int lh = GetCharacterHeight(FS_NORMAL);
		int y = ir.top;

		DrawString(ir.left, ir.right, y, fmt::format("설립  {}", GetString(STR_JUST_DATE_LONG, st->build_date)), TC_BLACK);
		y += lh;

		if (st->facilities.Test(StationFacility::Train) && st->train_station.tile != INVALID_TILE) {
			uint longest = 0;
			for (TileIndex t : st->train_station) {
				if (!st->TileBelongsToRailStation(t)) continue;
				longest = std::max(longest, st->GetPlatformLength(t));
			}
			DrawString(ir.left, ir.right, y, fmt::format("최대 승강장  {}칸", longest), TC_BLACK);
			y += lh;
		}

		std::string accepts;
		for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
			if (!st->goods[cs->Index()].status.Test(GoodsEntry::State::Acceptance)) continue;
			if (!accepts.empty()) accepts += ", ";
			accepts += GetString(cs->name);
		}
		DrawString(ir.left, ir.right, y, accepts.empty() ? "수용 화물 없음" : fmt::format("수용  {}", accepts), accepts.empty() ? TC_GREY : TC_BLACK);
	}

	void DrawIndustryRows(const Rect &ir, const Station *st) const
	{
		int lh = GetCharacterHeight(FS_NORMAL);
		int y = ir.top;

		this->industry_rows.clear();
		auto put = [&](std::string_view text, TextColour tc, TileIndex jump) {
			if (y + lh - 1 > ir.bottom) return false;
			DrawString(ir.left, ir.right, y, text, tc);
			this->industry_rows.push_back(jump);
			y += lh;
			return true;
		};

		std::vector<const Industry *> supply;
		for (const Industry *i : Industry::Iterate()) {
			if (i->stations_near.find(const_cast<Station *>(st)) == i->stations_near.end()) continue;
			if (std::none_of(std::begin(i->produced), std::end(i->produced), [](const auto &p) { return IsValidCargoType(p.cargo); })) continue;
			supply.push_back(i);
		}
		if (!supply.empty()) {
			if (!put("공급처", TC_GREY, INVALID_TILE)) return;
			for (const Industry *i : supply) {
				if (!put(GetString(STR_INDUSTRY_NAME, i->index), TC_BLACK, i->location.tile)) return;
			}
		}

		if (!st->industries_near.empty()) {
			if (!put("납품처", TC_GREY, INVALID_TILE)) return;
			for (const IndustryListEntry &e : st->industries_near) {
				if (!put(GetString(STR_INDUSTRY_NAME, e.industry->index), TC_BLACK, e.industry->location.tile)) return;
			}
		}

		if (this->industry_rows.empty()) DrawString(ir.left, ir.right, y, "주변 산업 없음", TC_GREY);
	}

	void DrawVehicleRows(const Rect &ir, const Station *st) const
	{
		int lh = GetCharacterHeight(FS_NORMAL);
		int y = ir.top;

		this->vehicle_rows.clear();
		for (VehicleType vt : {VEH_TRAIN, VEH_ROAD, VEH_SHIP, VEH_AIRCRAFT}) {
			VehicleList list;
			if (!GenerateVehicleSortList(&list, VehicleListIdentifier(VL_STATION_LIST, vt, _local_company, st->index))) continue;
			for (const Vehicle *v : list) {
				if (y + lh - 1 > ir.bottom) return;
				bool here = v->current_order.IsType(OT_GOTO_STATION) && v->current_order.GetDestination().ToStationID() == st->index;
				DrawString(ir.left, ir.right, y, GetString(STR_VEHICLE_NAME, v->index), here ? TC_ORANGE : TC_BLACK);
				this->vehicle_rows.push_back(v->index);
				y += lh;
			}
		}
		if (this->vehicle_rows.empty()) DrawString(ir.left, ir.right, y, "이 역에 오는 차량 없음", TC_GREY);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		const Station *st = Station::Get(this->window_number);
		switch (widget) {
			case WID_MS_DESC: {
				Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
				std::string desc;
				static constexpr std::pair<StationFacility, std::string_view> parts[] = {
					{StationFacility::Train, "철도"},
					{StationFacility::TruckStop, "트럭"},
					{StationFacility::BusStop, "버스"},
					{StationFacility::Airport, "공항"},
					{StationFacility::Dock, "부두"},
				};
				for (const auto &[fac, label] : parts) {
					if (!st->facilities.Test(fac)) continue;
					if (!desc.empty()) desc += " + ";
					desc += label;
				}
				DrawString(ir.left, ir.right, ir.top, desc.empty() ? "시설 없음" : desc, TC_BLACK);
				break;
			}
			case WID_MS_BODY: {
				Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
				switch (this->tab) {
					case TAB_STATUS: this->DrawStatusRows(ir, st); break;
					case TAB_INFO: this->DrawInfoRows(ir, st); break;
					case TAB_INDUSTRY: this->DrawIndustryRows(ir, st); break;
					case TAB_VEHICLES: this->DrawVehicleRows(ir, st); break;
				}
				break;
			}
			case WID_SV_CATCHMENT:
				MiniUiDrawCoverageGlyph(r, COLOUR_GREY);
				break;
		}
	}

	void OnPaint() override
	{
		extern const Station *_viewport_highlight_station;
		const Station *st = Station::Get(this->window_number);
		this->SetWidgetLoweredState(WID_MS_TAB_STATUS, this->tab == TAB_STATUS);
		this->SetWidgetLoweredState(WID_MS_TAB_INFO, this->tab == TAB_INFO);
		this->SetWidgetLoweredState(WID_MS_TAB_INDUSTRY, this->tab == TAB_INDUSTRY);
		this->SetWidgetLoweredState(WID_MS_TAB_VEHICLES, this->tab == TAB_VEHICLES);
		this->SetWidgetDisabledState(WID_SV_RENAME, st->owner != _local_company);
		this->SetWidgetDisabledState(WID_SV_CATCHMENT, st->facilities.None());
		this->SetWidgetLoweredState(WID_SV_CATCHMENT, _viewport_highlight_station == st);
		this->DrawWidgets();
	}

	void OnGameTick() override
	{
		this->SetDirty();
	}

	int BodyRow(int click_y) const
	{
		const NWidgetBase *wid = this->GetWidget<NWidgetBase>(WID_MS_BODY);
		return (click_y - (int)wid->pos_y - WidgetDimensions::scaled.framerect.top) / GetCharacterHeight(FS_NORMAL);
	}

	void OnClick(Point pt, WidgetID widget, int) override
	{
		switch (widget) {
			case WID_MS_TAB_STATUS:
			case WID_MS_TAB_INFO:
			case WID_MS_TAB_INDUSTRY:
			case WID_MS_TAB_VEHICLES:
				this->tab = (uint8_t)(widget - WID_MS_TAB_STATUS);
				this->SetDirty();
				break;

			case WID_MS_BODY: {
				int row = this->BodyRow(pt.y);
				if (row < 0) break;
				if (this->tab == TAB_INDUSTRY && row < (int)this->industry_rows.size()) {
					TileIndex t = this->industry_rows[row];
					if (t != INVALID_TILE) MiniUiScrollTo(TileX(t) * TILE_SIZE, TileY(t) * TILE_SIZE);
				} else if (this->tab == TAB_VEHICLES && row < (int)this->vehicle_rows.size()) {
					const Vehicle *v = Vehicle::GetIfValid(this->vehicle_rows[row]);
					if (v != nullptr) ShowVehicleViewWindow(v);
				}
				break;
			}

			case WID_SV_RENAME:
				ShowQueryString(GetString(STR_STATION_NAME, this->window_number), STR_STATION_VIEW_EDIT_STATION_SIGN, MAX_LENGTH_STATION_NAME_CHARS,
						this, CS_ALPHANUMERAL, {QueryStringFlag::EnableDefault, QueryStringFlag::LengthIsInChars});
				break;

			case WID_SV_CATCHMENT:
				SetViewportCatchmentStation(Station::Get(this->window_number), !this->IsWidgetLowered(WID_SV_CATCHMENT));
				break;

			case WID_SV_LOCATION: {
				TileIndex t = Station::Get(this->window_number)->xy;
				MiniUiScrollTo(TileX(t) * TILE_SIZE, TileY(t) * TILE_SIZE);
				break;
			}
		}
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value()) return;
		Command<CMD_RENAME_STATION>::Post(STR_ERROR_CAN_T_RENAME_STATION, static_cast<StationID>(this->window_number), *str);
	}
};

static WindowDesc _mini_station_desc(
	WDP_AUTO, "mini_station_view", 0, 0,
	WC_STATION_VIEW, WC_NONE,
	{},
	_nested_mini_station_widgets
);

bool ShowMiniStationWindow(StationID station)
{
	if (!MiniUiActive()) return false;
	if (!Station::IsValidID(station)) return false;
	AllocateWindowDescFront<MiniStationWindow>(_mini_station_desc, station);
	return true;
}
