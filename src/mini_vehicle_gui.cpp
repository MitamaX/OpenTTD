/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file mini_vehicle_gui.cpp Vehicle window in the reference structure: a description strip, tabbed status sections and a command row at the bottom. */

#include "stdafx.h"

#include "cargotype.h"
#include "command_func.h"
#include "gui.h"
#include "mini_ui.h"
#include "order_base.h"
#include "order_cmd.h"
#include "strings_func.h"
#include "vehicle_base.h"
#include "vehicle_cmd.h"
#include "vehicle_func.h"
#include "vehicle_gui.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "widgets/vehicle_widget.h"

#include "table/strings.h"

#include "safeguards.h"

static constexpr WidgetID WID_MV_DESC = WID_VV_HONK_HORN + 1;
static constexpr WidgetID WID_MV_TAB_STATUS = WID_VV_HONK_HORN + 2;
static constexpr WidgetID WID_MV_TAB_CARGO = WID_VV_HONK_HORN + 3;
static constexpr WidgetID WID_MV_TAB_ORDERS = WID_VV_HONK_HORN + 4;
static constexpr WidgetID WID_MV_STOP = WID_VV_HONK_HORN + 5;

static constexpr NWidgetPart _nested_mini_vehicle_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_VV_CAPTION), SetStringTip(STR_JUST_STRING, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_MV_DESC), SetMinimalSize(240, 0), SetMinimalTextLines(1, WidgetDimensions::unscaled.framerect.Vertical()), SetFill(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_MV_TAB_STATUS), SetStringTip(STR_JUST_STRING), SetFill(1, 0),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_MV_TAB_CARGO), SetStringTip(STR_JUST_STRING), SetFill(1, 0),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_MV_TAB_ORDERS), SetStringTip(STR_JUST_STRING), SetFill(1, 0),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_VV_START_STOP), SetMinimalTextLines(8, WidgetDimensions::unscaled.framerect.Vertical()), SetFill(1, 1), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_MV_STOP), SetStringTip(STR_JUST_STRING), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VV_GOTO_DEPOT), SetStringTip(STR_JUST_STRING), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VV_REFIT), SetStringTip(STR_JUST_STRING), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_VV_SHOW_ORDERS), SetStringTip(STR_JUST_STRING), SetFill(1, 0),
	EndContainer(),
};

struct MiniVehicleWindow : Window {
	enum Tab : uint8_t {
		TAB_STATUS,
		TAB_CARGO,
		TAB_ORDERS,
	};

	uint8_t tab = TAB_STATUS;

	MiniVehicleWindow(WindowDesc &desc, WindowNumber number) : Window(desc)
	{
		this->InitNested(number);
		this->owner = Vehicle::Get(number)->owner;
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		const Vehicle *v = Vehicle::Get(this->window_number);
		switch (widget) {
			case WID_VV_CAPTION: return GetString(STR_VEHICLE_NAME, v->index);
			case WID_MV_TAB_STATUS: return "STATUS";
			case WID_MV_TAB_CARGO: return "CARGO";
			case WID_MV_TAB_ORDERS: return "ORDERS";
			case WID_MV_STOP: return v->vehstatus.Test(VehState::Stopped) ? "GO" : "STOP";
			case WID_VV_GOTO_DEPOT: return "DEPOT";
			case WID_VV_REFIT: return "REFIT";
			case WID_VV_SHOW_ORDERS: return "ORDERS...";
			default: return this->Window::GetWidgetString(widget, stringid);
		}
	}

	/* One line per status fact; the reference keeps prose out and one value
	 * per row. */
	void DrawStatusRows(const Rect &ir, const Vehicle *v) const
	{
		int lh = GetCharacterHeight(FS_NORMAL);
		int y = ir.top;

		if (v->vehstatus.Test(VehState::Crashed)) {
			DrawString(ir.left, ir.right, y, GetString(STR_VEHICLE_STATUS_CRASHED), TC_RED);
		} else if (v->vehstatus.Test(VehState::Stopped)) {
			DrawString(ir.left, ir.right, y, GetString(STR_VEHICLE_STATUS_STOPPED), TC_RED);
		} else if (v->current_order.IsType(OT_GOTO_STATION)) {
			DrawString(ir.left, ir.right, y, GetString(STR_STATION_NAME, v->current_order.GetDestination().ToStationID()), TC_LIGHT_BLUE);
		} else if (v->current_order.IsType(OT_GOTO_DEPOT)) {
			DrawString(ir.left, ir.right, y, "DEPOT", TC_LIGHT_BLUE);
		} else {
			DrawString(ir.left, ir.right, y, "-", TC_GREY);
		}
		y += lh;

		DrawString(ir.left, ir.right, y, fmt::format("SPEED {} / {}", v->GetDisplaySpeed(), v->GetDisplayMaxSpeed()), TC_BLACK);
		y += lh;
		DrawString(ir.left, ir.right, y, GetString(STR_VEHICLE_INFO_RELIABILITY_BREAKDOWNS, v->reliability * 100 >> 16, v->breakdowns_since_last_service));
		y += lh;
		DrawString(ir.left, ir.right, y, fmt::format("AGE {}Y / {}Y", v->age.base() / 366, v->max_age.base() / 366), TC_BLACK);
		y += lh;
		DrawString(ir.left, ir.right, y, GetString(STR_VEHICLE_INFO_PROFIT_THIS_YEAR_LAST_YEAR, v->GetDisplayProfitThisYear(), v->GetDisplayProfitLastYear()));
	}

	void DrawCargoRows(const Rect &ir, const Vehicle *v) const
	{
		int lh = GetCharacterHeight(FS_NORMAL);
		int y = ir.top;

		static std::vector<std::tuple<CargoType, uint, uint>> cargo;
		cargo.clear();
		for (const Vehicle *u = v; u != nullptr; u = u->Next()) {
			if (u->cargo_cap == 0 || !IsValidCargoType(u->cargo_type)) continue;
			auto it = std::find_if(cargo.begin(), cargo.end(), [&](const auto &e) { return std::get<0>(e) == u->cargo_type; });
			if (it == cargo.end()) it = cargo.emplace(cargo.end(), u->cargo_type, 0, 0);
			std::get<1>(*it) += u->cargo_cap;
			std::get<2>(*it) += u->cargo.StoredCount();
		}
		if (cargo.empty()) {
			DrawString(ir.left, ir.right, y, "-", TC_GREY);
			return;
		}
		for (const auto &[ct, cap, stored] : cargo) {
			if (y + lh - 1 > ir.bottom) break;
			DrawString(ir.left, ir.right, y, fmt::format("{} {} / {}", GetString(CargoSpec::Get(ct)->name), stored, cap), TC_BLACK);
			y += lh;
		}
	}

	std::string OrderLabel(const Order &o) const
	{
		switch (o.GetType()) {
			case OT_GOTO_STATION: return GetString(STR_STATION_NAME, o.GetDestination().ToStationID());
			case OT_GOTO_WAYPOINT: return GetString(STR_WAYPOINT_NAME, o.GetDestination().ToStationID());
			case OT_GOTO_DEPOT: return "DEPOT";
			case OT_CONDITIONAL: return fmt::format("IF > {}", o.GetConditionSkipToOrder() + 1);
			default: return std::string();
		}
	}

	/* Real order indices of the drawn rows, top to bottom; used to map a
	 * click back to the order to skip to. */
	mutable std::vector<int> order_rows;

	void DrawOrderRows(const Rect &ir, const Vehicle *v) const
	{
		int lh = GetCharacterHeight(FS_NORMAL);
		int y = ir.top;
		int max_rows = ir.Height() / lh;

		this->order_rows.clear();
		if (v->GetNumOrders() == 0) {
			DrawString(ir.left, ir.right, y, "-", TC_GREY);
			return;
		}
		int oi = 0;
		for (const Order &o : v->Orders()) {
			std::string label = this->OrderLabel(o);
			if (!label.empty()) {
				if ((int)this->order_rows.size() >= max_rows) break;
				bool cur = oi == v->cur_real_order_index;
				DrawString(ir.left, ir.right, y, fmt::format("{}{}. {}", cur ? "> " : "", oi + 1, label), cur ? TC_ORANGE : TC_BLACK);
				this->order_rows.push_back(oi);
				y += lh;
			}
			oi++;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		const Vehicle *v = Vehicle::Get(this->window_number);
		switch (widget) {
			case WID_MV_DESC: {
				Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
				DrawString(ir.left, ir.right, ir.top, fmt::format("{} #{}", GetString((StringID)(STR_REPLACE_VEHICLE_TRAIN + v->type)), v->unitnumber), TC_BLACK);
				break;
			}
			case WID_VV_START_STOP: {
				Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
				switch (this->tab) {
					case TAB_STATUS: this->DrawStatusRows(ir, v); break;
					case TAB_CARGO: this->DrawCargoRows(ir, v); break;
					case TAB_ORDERS: this->DrawOrderRows(ir, v); break;
				}
				break;
			}
		}
	}

	void OnPaint() override
	{
		this->SetWidgetLoweredState(WID_MV_TAB_STATUS, this->tab == TAB_STATUS);
		this->SetWidgetLoweredState(WID_MV_TAB_CARGO, this->tab == TAB_CARGO);
		this->SetWidgetLoweredState(WID_MV_TAB_ORDERS, this->tab == TAB_ORDERS);
		const Vehicle *v = Vehicle::Get(this->window_number);
		this->SetWidgetDisabledState(WID_MV_STOP, v->owner != _local_company);
		this->SetWidgetDisabledState(WID_VV_GOTO_DEPOT, v->owner != _local_company);
		this->SetWidgetDisabledState(WID_VV_REFIT, v->owner != _local_company);
		this->DrawWidgets();
	}

	void OnClick(Point pt, WidgetID widget, int) override
	{
		const Vehicle *v = Vehicle::Get(this->window_number);
		switch (widget) {
			case WID_MV_TAB_STATUS:
			case WID_MV_TAB_CARGO:
			case WID_MV_TAB_ORDERS:
				this->tab = (uint8_t)(widget - WID_MV_TAB_STATUS);
				this->SetDirty();
				break;

			case WID_VV_START_STOP: {
				if (this->tab != TAB_ORDERS || v->owner != _local_company) break;
				const NWidgetBase *wid = this->GetWidget<NWidgetBase>(WID_VV_START_STOP);
				int row = (pt.y - (int)wid->pos_y - WidgetDimensions::scaled.framerect.top) / GetCharacterHeight(FS_NORMAL);
				if (row < 0 || row >= (int)this->order_rows.size()) break;
				Command<CMD_SKIP_TO_ORDER>::Post(STR_ERROR_CAN_T_SKIP_TO_ORDER, v->tile, v->index, (VehicleOrderID)this->order_rows[row]);
				break;
			}

			case WID_MV_STOP:
				Command<CMD_START_STOP_VEHICLE>::Post(STR_ERROR_CAN_T_STOP_START_TRAIN + v->type, v->tile, v->index, false);
				break;

			case WID_VV_GOTO_DEPOT:
				Command<CMD_SEND_VEHICLE_TO_DEPOT>::Post(GetCmdSendToDepotMsg(v), v->index, _ctrl_pressed ? DepotCommandFlag::Service : DepotCommandFlags{}, {});
				break;

			case WID_VV_REFIT:
				ShowVehicleRefitWindow(v, INVALID_VEH_ORDER_ID, this);
				break;

			case WID_VV_SHOW_ORDERS:
				ShowOrdersWindow(v);
				break;
		}
	}
};

static WindowDesc _mini_vehicle_desc(
	WDP_AUTO, "mini_vehicle_view", 0, 0,
	WC_VEHICLE_VIEW, WC_NONE,
	{},
	_nested_mini_vehicle_widgets
);

bool ShowMiniVehicleWindow(const Vehicle *v)
{
	if (!MiniUiActive()) return false;
	AllocateWindowDescFront<MiniVehicleWindow>(_mini_vehicle_desc, v->index);
	return true;
}
