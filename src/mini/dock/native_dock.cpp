/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file native_dock.cpp Native windows pinned under mini window bodies. */

#include "../../stdafx.h"
#include "native_dock.h"

#include "../../window_func.h"
#include "../../window_gui.h"
#include "carrier.h"

#include "../../safeguards.h"

NativeDock _dock;

static NativeKey KeyOf(const Window *w)
{
	return {w->window_class, w->window_number};
}

template <typename Entries>
static auto FindEntry(Entries &entries, NativeKey key) -> decltype(entries.data())
{
	for (auto &d : entries) {
		if (d.key == key) return &d;
	}
	return nullptr;
}

/* A released window keeps the position its slot forced on it, caption and all
 * above the screen edge; the no-op resize runs the visibility clamp. */
static void Undock(const DockedWindow &d)
{
	if (d.owned) {
		CloseWindowById(d.key.wc, d.key.num);
		return;
	}
	Window *w = FindWindowById(d.key.wc, d.key.num);
	if (w != nullptr) ResizeWindow(w, 0, 0, true);
}

const DockedWindow *NativeDock::Find(const Window *w) const
{
	return FindEntry(this->windows, KeyOf(w));
}

bool NativeDock::Docks(const Window *w) const
{
	return this->Find(w) != nullptr || IsCarrier(w);
}

Window *NativeDock::Open(const DockSpec &spec, WindowNumber num) const
{
	Window *w = FindWindowById(spec.wc, num);
	if (w != nullptr || spec.open == nullptr) return w;
	spec.open(num);
	return FindWindowById(spec.wc, num);
}

/* The native is sized in whole steps to the slot and moved so its caption
 * sits just above it; the part that shows is what the slot can sample. A slot
 * not laid out yet only holds the window, so nothing adopts it meanwhile. */
Rect NativeDock::Pin(Window *w, bool owned, const Rect &slot, const NativeSizing &sizing, int grip)
{
	DockedWindow &d = this->Hold(KeyOf(w), owned);
	if (slot.Width() <= 0 || slot.Height() <= 0) return d.vis;

	int crop = CaptionCrop(w);
	ResizeInSteps(w, slot.Width(), slot.Height() + crop, sizing);
	MoveNativeWindow(w, slot.left, slot.top - crop);

	d.vis = {slot.left, slot.top, slot.left + std::min(w->width, slot.Width()) - 1, slot.top + std::min(w->height - crop, slot.Height()) - 1};
	d.grip = grip;
	return d.vis;
}

/* A carrier has no caption to crop and no resize box; it takes the slot's size outright. */
void NativeDock::Carry(Window *w, const Rect &slot)
{
	FitCarrier(w, slot.left, slot.top, slot.Width(), slot.Height());
	DockedWindow &d = this->Hold(KeyOf(w), true);
	d.vis = slot;
	d.grip = 0;
}

void NativeDock::Unmark()
{
	for (DockedWindow &d : this->windows) d.used = false;
}

void NativeDock::Mark(NativeKey key)
{
	if (DockedWindow *d = FindEntry(this->windows, key); d != nullptr) d->used = true;
}

void NativeDock::Sweep()
{
	for (size_t i = this->windows.size(); i-- > 0;) {
		if (this->windows[i].used) continue;
		Undock(this->windows[i]);
		this->windows.erase(this->windows.begin() + (ptrdiff_t)i);
	}
}

void NativeDock::CloseAll()
{
	for (const DockedWindow &d : this->windows) Undock(d);
	this->windows.clear();
}

/* Where two slots overlap, the native paint and the click both go to whichever
 * native window is on top, so native z-order has to follow mini window order.
 * Re-fronting is only worth doing when the two already disagree. */
void NativeDock::Stack(const std::vector<NativeKey> &want)
{
	if (want.size() <= 1) return;
	std::vector<NativeKey> have = this->DockedOrder();
	if (have == want) return;

	/* Some classes outrank others in the native z-order, so the wanted order is
	 * not always reachable. Re-applying is driven by change on either side:
	 * a new focus order, or the order drifting away from what was achieved. */
	if (this->applied == want && have == this->settled) return;
	this->applied = want;

	for (const NativeKey &key : want) {
		Window *w = BringWindowToFrontById(key.wc, key.num);
		/* The activation flash would blink inside the slot on every reorder. */
		if (w != nullptr) w->flags.Reset(WindowFlag::WhiteBorder);
	}
	this->settled = this->DockedOrder();
}

DockedWindow &NativeDock::Hold(NativeKey key, bool owned)
{
	DockedWindow *d = FindEntry(this->windows, key);
	if (d == nullptr) d = &this->windows.emplace_back(DockedWindow{key, {}, 0, false, owned});
	d->used = true;
	return *d;
}

std::vector<NativeKey> NativeDock::DockedOrder() const
{
	std::vector<NativeKey> order;
	for (const Window *w : Window::IterateFromBack()) {
		if (this->Docks(w)) order.push_back(KeyOf(w));
	}
	return order;
}
