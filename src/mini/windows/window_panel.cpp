/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file window_panel.cpp A ledger panel that can carry a live camera on its first tab and show an official window in its body. */

#include "../../stdafx.h"
#include "window_panel.h"

#include <RmlUi/Core.h>

#include "../../core/geometry_type.hpp"
#include "../../window_gui.h"
#include "../dock/native_window.h"
#include "../ui/native_slot.h"
#include "../ui/pixel_style.h"

#include "../../safeguards.h"

static constexpr int CAMERA_TAB = 0;
static constexpr const char CAMERA_SLOT[] = "camera";
static constexpr const char EMBED_SLOT[] = "embed";

static constexpr Rml::PropertyId FIT_PROPERTIES[] = {
	Rml::PropertyId::Width, Rml::PropertyId::Height,
	Rml::PropertyId::MinWidth, Rml::PropertyId::MinHeight,
	Rml::PropertyId::MaxWidth, Rml::PropertyId::MaxHeight,
};

static NativeSlot *FindSlot(Rml::ElementDocument &document, const char *id)
{
	return dynamic_cast<NativeSlot *>(document.GetElementById(id));
}

static Rect ScreenRectOf(NativeSlot &slot)
{
	Rml::Rectanglei area = slot.ScreenRect();
	return {area.Left(), area.Top(), area.Right() - 1, area.Bottom() - 1};
}

static void SetCap(Rml::Element &element, Rml::PropertyId id, std::optional<float> pixels)
{
	if (pixels.has_value()) {
		SetPixels(element, id, *pixels);
	} else if (element.GetLocalProperty(id) != nullptr) {
		element.RemoveProperty(id);
	}
}

/* A native grows in whole resize steps, so the panel only takes sizes the native can fill exactly. */
static float SnapToSteps(float size, float base, int step)
{
	if (step <= 1) return std::max(size, base);
	return base + std::floor(std::max(0.0f, size - base) / step) * step;
}

std::optional<CameraShot> WindowPanel::Camera() const
{
	return std::nullopt;
}

std::optional<EmbedTarget> WindowPanel::Embed() const
{
	return std::nullopt;
}

void WindowPanel::ListNatives(std::vector<NativeKey> &natives) const
{
	if (this->shot.has_value()) natives.push_back({WC_EXTRA_VIEWPORT, CarrierNumber(to_underlying(this->shot->subject), this->shot->id)});
	if (this->target.has_value()) natives.push_back({this->target->spec.wc, this->target->number});
}

void WindowPanel::Collect()
{
	this->shot = this->tab == CAMERA_TAB ? this->Camera() : std::nullopt;
	this->target = this->Embed();
	this->camera = this->shot.has_value();
	this->embedded = this->target.has_value();
	LedgerPanel::Collect();
}

void WindowPanel::AfterLayout()
{
	if (this->shot.has_value()) this->ShowCamera(*this->shot);
	if (this->target.has_value()) {
		this->ShowEmbed(*this->target);
	} else {
		this->Unfit();
	}
}

/* The carrier paints into the screen buffer below the panel layer and the slot samples it back. */
void WindowPanel::ShowCamera(const CameraShot &shot) const
{
	NativeSlot *slot = FindSlot(*this->Document(), CAMERA_SLOT);
	if (slot == nullptr) return;
	Rect area = ScreenRectOf(*slot);
	if (area.Width() <= 0 || area.Height() <= 0) return;

	WindowNumber number = CarrierNumber(to_underlying(shot.subject), shot.id);
	Window *w = FindCarrier(number);
	if (w == nullptr) w = OpenCarrier(number, shot.focus);
	_dock.Carry(w, area);
}

void WindowPanel::ShowEmbed(const EmbedTarget &target)
{
	Rml::ElementDocument &document = *this->Document();
	NativeSlot *slot = FindSlot(document, EMBED_SLOT);
	Window *w = slot == nullptr ? nullptr : _dock.Open(target.spec, target.number);
	if (w == nullptr) return;

	NativeSizing sizing = SizingOf(w);
	Rml::Vector2f chrome = document.GetBox().GetSize(Rml::BoxArea::Border) - Rml::Vector2f(slot->ScreenRect().Size());
	this->Fit(sizing, chrome);
	this->sizable = !sizing.Pinned();
	_dock.Pin(w, target.spec.open != nullptr, ScreenRectOf(*slot), sizing, 0);
}

/* The panel never shrinks below the native's own minimum; an axis the native cannot resize is held at it. */
void WindowPanel::Fit(const NativeSizing &sizing, Rml::Vector2f chrome)
{
	Rml::ElementDocument &document = *this->Document();
	Rml::Vector2f base(sizing.min_w + chrome.x, sizing.min_h + chrome.y);
	Rml::Vector2f size = document.GetBox().GetSize(Rml::BoxArea::Border);
	size.x = sizing.fix_w ? base.x : SnapToSteps(size.x, base.x, sizing.step_w);
	size.y = sizing.fix_h ? base.y : SnapToSteps(size.y, base.y, sizing.step_h);

	SetPixels(document, Rml::PropertyId::MinWidth, base.x);
	SetPixels(document, Rml::PropertyId::MinHeight, base.y);
	SetCap(document, Rml::PropertyId::MaxWidth, sizing.fix_w ? std::optional(base.x) : std::nullopt);
	SetCap(document, Rml::PropertyId::MaxHeight, sizing.fix_h ? std::optional(base.y) : std::nullopt);
	SetPixels(document, Rml::PropertyId::Width, size.x);
	SetPixels(document, Rml::PropertyId::Height, size.y);
	this->fitted = true;
}

void WindowPanel::Unfit()
{
	this->sizable = false;
	if (!std::exchange(this->fitted, false)) return;
	for (Rml::PropertyId id : FIT_PROPERTIES) this->Document()->RemoveProperty(id);
}
