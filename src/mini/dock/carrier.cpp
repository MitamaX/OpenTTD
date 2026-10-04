/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file carrier.cpp Frameless native viewports that paint under a mini window's view slot. */

#include "../../stdafx.h"
#include "carrier.h"

#include "../../viewport_func.h"
#include "../../widget_type.h"
#include "../../window_func.h"
#include "../../window_gui.h"
#include "../../zoom_type.h"
#include "native_window.h"

#include "../../safeguards.h"

static constexpr int CARRIER_NUM_BASE = 0x40000;
/* Kinds get separate number ranges; a vehicle and a station sharing one id
 * must not resolve to the same carrier window. */
static constexpr int CARRIER_KIND_STRIDE = 0x1000000;

static constexpr NWidgetPart _nested_carrier_widgets[] = {
	NWidget(NWID_VIEWPORT, INVALID_COLOUR, 0), SetResize(1, 1), SetFill(1, 1), SetMinimalSize(64, 48),
};

static WindowDesc _carrier_desc(
	WDP_MANUAL, {}, 0, 0,
	WC_EXTRA_VIEWPORT, WC_NONE,
	{},
	_nested_carrier_widgets
);

struct CarrierWindow : Window {
	TileIndex focus_tile = INVALID_TILE;

	CarrierWindow(WindowDesc &desc, WindowNumber num, CarrierFocus focus) : Window(desc)
	{
		if (std::holds_alternative<TileIndex>(focus)) this->focus_tile = std::get<TileIndex>(focus);
		this->InitNested(num);
		this->GetWidget<NWidgetViewport>(0)->InitializeViewport(this, focus, ZoomLevel::Viewport);
	}

	/* The viewport scroll target is its top-left corner, so growing the
	 * window from its minimal size would drift the view right and down. */
	void OnResize() override
	{
		if (this->viewport == nullptr) return;
		this->GetWidget<NWidgetViewport>(0)->UpdateViewportCoordinates(this);
		if (this->focus_tile != INVALID_TILE) ScrollWindowToTile(this->focus_tile, this, true);
	}
};

WindowNumber CarrierNumber(int kind, int id)
{
	return CARRIER_NUM_BASE + kind * CARRIER_KIND_STRIDE + id;
}

bool IsCarrier(const Window *w)
{
	return w->window_class == WC_EXTRA_VIEWPORT && w->window_number >= CARRIER_NUM_BASE;
}

Window *FindCarrier(WindowNumber num)
{
	return FindWindowById(WC_EXTRA_VIEWPORT, num);
}

Window *OpenCarrier(WindowNumber num, CarrierFocus focus)
{
	return new CarrierWindow(_carrier_desc, num, focus);
}

void FitCarrier(Window *w, int x, int y, int width, int height)
{
	if (w->width != width || w->height != height) ResizeWindow(w, width - w->width, height - w->height, false);
	MoveNativeWindow(w, x, y);
}

void CloseCarrier(WindowNumber num)
{
	CloseWindowById(WC_EXTRA_VIEWPORT, num);
}
