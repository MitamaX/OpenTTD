/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file window_bar.cpp The top-right corner: the window categories, the windows of the open one and the map overlay toggles. */

#include "../../stdafx.h"
#include "window_bar.h"

#include <cmath>

#include <RmlUi/Core.h>

#include "../core/tuning.h"
#include "../map/map_overlay.h"

#include "../../safeguards.h"

struct OverlayToggle {
	MiniLayer layer;
	std::string_view icon;
};

static constexpr OverlayToggle OVERLAY_TOGGLES[] = {
	{MiniLayer::Rail, "rail"},
	{MiniLayer::Road, "road"},
};

static constexpr const char BASE_ID[] = "window-base";

MenuShelf _window_shelf;
static int _base_bottom = 0;

int WindowBarBottom()
{
	return _base_bottom;
}

WindowBar::WindowBar(Opener open) : HudPart("windows"), open(std::move(open))
{
}

void WindowBar::Bind(Rml::DataModelConstructor &model)
{
	this->Expose(model, "categories", &this->categories);
	this->Expose(model, "shelf", &this->shelf);
	this->Expose(model, "windows", &this->windows);
	this->Expose(model, "overlays", &this->overlays);
	model.BindEventCallback("toggle", &WindowBar::Toggle, this);
	model.BindEventCallback("pick", &WindowBar::Pick, this);
	model.BindEventCallback("overlay", &WindowBar::Overlay, this);
}

void WindowBar::Collect()
{
	std::span<const MiniWinCategory> categories = WindowCategories();
	this->categories.clear();
	for (int i = 0; i < static_cast<int>(categories.size()); i++) {
		const MiniWinCategory &category = categories[i];
		this->categories.push_back(MakeMenuTile(category.str, category.fallback, category.icon, i == _window_shelf.Open()));
	}
	this->shelf = _window_shelf.IsOpen();
	this->CollectWindows();
	this->CollectOverlays();
	this->MeasureBase();
}

void WindowBar::CollectWindows()
{
	this->windows.clear();
	if (!_window_shelf.IsOpen()) return;
	for (const MiniWinItem &item : WindowCategories()[_window_shelf.Open()].items) {
		this->windows.push_back(MakeMenuTile(item.str, item.fallback, item.icon, false));
	}
}

/* Without the greyed base there is nothing for an overlay to stand out from, so the strip goes with it. */
void WindowBar::CollectOverlays()
{
	this->overlays.clear();
	if (_tuning.filter_alpha <= 0) return;
	for (const OverlayToggle &toggle : OVERLAY_TOGGLES) this->overlays.push_back({Rml::String(), IconPath(toggle.icon), _overlay.Shown() == toggle.layer});
}

/* Panels and native windows stack below the bar and the strip, wherever the last layout left their bottom edge. */
void WindowBar::MeasureBase()
{
	Rml::Element *root = this->Root();
	Rml::Element *base = root == nullptr ? nullptr : root->GetElementById(BASE_ID);
	if (base == nullptr) return;

	float bottom = base->GetAbsoluteOffset(Rml::BoxArea::Border).y + base->GetBox().GetSize(Rml::BoxArea::Border).y;
	_base_bottom = static_cast<int>(std::ceil(bottom));
}

void WindowBar::Toggle(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	if (int index = ArgumentSlot(WindowCategories(), arguments); index >= 0) _window_shelf.Toggle(index);
}

void WindowBar::Pick(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	if (!_window_shelf.IsOpen()) return;
	const MiniWinItem *item = ArgumentItem(WindowCategories()[_window_shelf.Open()].items, arguments);
	if (item == nullptr) return;

	_window_shelf.Close();
	this->open(item->win);
}

void WindowBar::Overlay(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	const OverlayToggle *toggle = ArgumentItem(std::span<const OverlayToggle>(OVERLAY_TOGGLES), arguments);
	if (toggle != nullptr) _overlay.Toggle(toggle->layer);
}
