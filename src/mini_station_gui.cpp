/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mini_station_gui.cpp Station window in the reference structure: a description strip, tabbed status sections and a command row at the bottom. */

#include "stdafx.h"

#include "cargotype.h"
#include "command_func.h"
#include "company_func.h"
#include "core/math_func.hpp"
#include "mini_ui.h"
#include "station_base.h"
#include "station_cmd.h"
#include "strings_func.h"
#include "textbuf_gui.h"
#include "viewport_func.h"
#include "window_gui.h"

#include "widgets/station_widget.h"

#include "table/strings.h"

#include "safeguards.h"

static constexpr WidgetID WID_MS_DESC = WID_SV_CATCHMENT + 1;
static constexpr WidgetID WID_MS_TAB_STATUS = WID_SV_CATCHMENT + 2;
static constexpr WidgetID WID_MS_TAB_ACCEPTS = WID_SV_CATCHMENT + 3;
static constexpr WidgetID WID_MS_BODY = WID_SV_CATCHMENT + 4;

static constexpr NWidgetPart _nested_mini_station_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_SV_CAPTION), SetStringTip(STR_JUST_STRING, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_MS_DESC), SetMinimalSize(240, 0), SetMinimalTextLines(1, WidgetDimensions::unscaled.framerect.Vertical()), SetFill(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_MS_TAB_STATUS), SetStringTip(STR_JUST_STRING), SetFill(1, 0),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_MS_TAB_ACCEPTS), SetStringTip(STR_JUST_STRING), SetFill(1, 0),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_MS_BODY), SetMinimalTextLines(8, WidgetDimensions::unscaled.framerect.Vertical()), SetFill(1, 1), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SV_RENAME), SetStringTip(STR_JUST_STRING), SetFill(1, 0),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_SV_CATCHMENT), SetStringTip(STR_JUST_STRING), SetFill(1, 0),
	EndContainer(),
};

struct MiniStationWindow : Window {
	enum Tab : uint8_t {
		TAB_STATUS,
		TAB_ACCEPTS,
	};

	uint8_t tab = TAB_STATUS;

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
			case WID_MS_TAB_STATUS: return "STATUS";
			case WID_MS_TAB_ACCEPTS: return "ACCEPTS";
			case WID_SV_RENAME: return "RENAME";
			case WID_SV_CATCHMENT: return "AREA";
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
			DrawString(ir.left, ir.right, y, fmt::format("{} {} / {}%", GetString(cs->name), ge.TotalCount(), pct), tc);
			y += lh;
		}
		if (!any) DrawString(ir.left, ir.right, y, "-", TC_GREY);
	}

	void DrawAcceptsRows(const Rect &ir, const Station *st) const
	{
		int lh = GetCharacterHeight(FS_NORMAL);
		int y = ir.top;
		bool any = false;

		for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
			if (!st->goods[cs->Index()].status.Test(GoodsEntry::State::Acceptance)) continue;
			if (y + lh - 1 > ir.bottom) break;
			any = true;
			DrawString(ir.left, ir.right, y, GetString(cs->name), TC_BLACK);
			y += lh;
		}
		if (!any) DrawString(ir.left, ir.right, y, "-", TC_GREY);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		const Station *st = Station::Get(this->window_number);
		switch (widget) {
			case WID_MS_DESC: {
				Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
				std::string desc;
				static constexpr std::pair<StationFacility, std::string_view> parts[] = {
					{StationFacility::Train, "RAIL"},
					{StationFacility::TruckStop, "TRUCK"},
					{StationFacility::BusStop, "BUS"},
					{StationFacility::Airport, "AIR"},
					{StationFacility::Dock, "DOCK"},
				};
				for (const auto &[fac, label] : parts) {
					if (!st->facilities.Test(fac)) continue;
					if (!desc.empty()) desc += " + ";
					desc += label;
				}
				DrawString(ir.left, ir.right, ir.top, desc.empty() ? "-" : desc, TC_BLACK);
				break;
			}
			case WID_MS_BODY: {
				Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
				switch (this->tab) {
					case TAB_STATUS: this->DrawStatusRows(ir, st); break;
					case TAB_ACCEPTS: this->DrawAcceptsRows(ir, st); break;
				}
				break;
			}
		}
	}

	void OnPaint() override
	{
		extern const Station *_viewport_highlight_station;
		const Station *st = Station::Get(this->window_number);
		this->SetWidgetLoweredState(WID_MS_TAB_STATUS, this->tab == TAB_STATUS);
		this->SetWidgetLoweredState(WID_MS_TAB_ACCEPTS, this->tab == TAB_ACCEPTS);
		this->SetWidgetDisabledState(WID_SV_RENAME, st->owner != _local_company);
		this->SetWidgetDisabledState(WID_SV_CATCHMENT, st->facilities.None());
		this->SetWidgetLoweredState(WID_SV_CATCHMENT, _viewport_highlight_station == st);
		this->DrawWidgets();
	}

	void OnGameTick() override
	{
		this->SetDirty();
	}

	void OnClick(Point, WidgetID widget, int) override
	{
		switch (widget) {
			case WID_MS_TAB_STATUS:
			case WID_MS_TAB_ACCEPTS:
				this->tab = (uint8_t)(widget - WID_MS_TAB_STATUS);
				this->SetDirty();
				break;

			case WID_SV_RENAME:
				ShowQueryString(GetString(STR_STATION_NAME, this->window_number), STR_STATION_VIEW_EDIT_STATION_SIGN, MAX_LENGTH_STATION_NAME_CHARS,
						this, CS_ALPHANUMERAL, {QueryStringFlag::EnableDefault, QueryStringFlag::LengthIsInChars});
				break;

			case WID_SV_CATCHMENT:
				SetViewportCatchmentStation(Station::Get(this->window_number), !this->IsWidgetLowered(WID_SV_CATCHMENT));
				break;
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
