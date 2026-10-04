/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file native_window.cpp Moving, sizing and reading the native windows the mini UI docks. */

#include "../../stdafx.h"
#include "native_window.h"

#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../viewport_type.h"
#include "../../window_func.h"
#include "../../window_gui.h"
#include "../ui/ui_text.h"
#include "carrier.h"

#include "../../table/strings.h"

#include "../../safeguards.h"

NativeSizing SizingOf(const Window *w)
{
	bool sizable = w->nested_root->GetWidgetOfType(WWT_RESIZEBOX) != nullptr;
	return {
		.fix_w = !sizable || w->nested_root->resize_x == 0,
		.fix_h = !sizable || w->nested_root->resize_y == 0,
		.step_w = std::max(1, (int)w->nested_root->resize_x),
		.step_h = std::max(1, (int)w->nested_root->resize_y),
		.min_w = (int)w->nested_root->smallest_x,
		.min_h = (int)w->nested_root->smallest_y - CaptionCrop(w),
	};
}

/* The caption row never reaches the screen: sampling starts below it so the
 * mini title bar stays the only title. */
int CaptionCrop(const Window *w)
{
	if (w->nested_root == nullptr) return 0;
	const NWidgetBase *cap = w->nested_root->GetWidgetOfType(WWT_CAPTION);
	return cap == nullptr ? 0 : cap->pos_y + (int)cap->current_y;
}

/* ResizeWindow asserts the delta lands on a whole resize step, so anything
 * finer than the step is dropped. A pinned axis is left alone. */
void ResizeInSteps(Window *w, int want_w, int want_h, const NativeSizing &sizing)
{
	int sx = (int)w->nested_root->resize_x;
	int sy = (int)w->nested_root->resize_y;
	int dx = (sizing.fix_w || sx == 0) ? 0 : want_w - w->width;
	int dy = (sizing.fix_h || sy == 0) ? 0 : want_h - w->height;
	if (sx != 0) dx -= dx % sx;
	if (sy != 0) dy -= dy % sy;
	if (dx != 0 || dy != 0) ResizeWindow(w, dx, dy, false);
}

/* The vacated region must repaint too, or its pixels linger in the screen
 * buffer and smear through other overlay rects. */
void MoveNativeWindow(Window *w, int x, int y)
{
	if (w->left == x && w->top == y) return;

	w->SetDirty();
	if (w->viewport != nullptr) {
		w->viewport->left += x - w->left;
		w->viewport->top += y - w->top;
	}
	w->left = x;
	w->top = y;
	w->SetDirty();
}

/* Popups glued to something else keep their own placement and frame; anything
 * that carries a caption is a standalone window and gets mini chrome. */
bool NativeWrappable(const Window *w)
{
	switch (w->window_class) {
		case WC_MAIN_WINDOW:
		case WC_MAIN_TOOLBAR:
		case WC_STATUS_BAR:
		case WC_DROPDOWN_MENU:
		case WC_TOOLTIPS:
		case WC_OSK:
		case WC_CONSOLE:
		case WC_MODAL_PROGRESS:
		case WC_HIGHSCORE:
		case WC_ENDSCREEN:
			return false;
		default:
			break;
	}
	if (IsCarrier(w)) return false;
	return w->nested_root != nullptr && w->nested_root->GetWidgetOfType(WWT_CAPTION) != nullptr;
}

std::string NativeCaption(Window *w)
{
	if (w->nested_root == nullptr) return {};
	const NWidgetCore *cap = dynamic_cast<const NWidgetCore *>(w->nested_root->GetWidgetOfType(WWT_CAPTION));
	if (cap == nullptr) return {};
	StringID sid = cap->GetString();
	if (cap->GetIndex() < 0) return sid == STR_NULL ? std::string() : GameText(sid);
	return StrMakeValid(w->GetWidgetString(cap->GetIndex(), sid), {});
}
