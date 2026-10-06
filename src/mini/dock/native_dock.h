/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file native_dock.h Native windows pinned under mini window bodies. */

#ifndef MINI_DOCK_NATIVE_DOCK_H
#define MINI_DOCK_NATIVE_DOCK_H

#include <vector>

#include "../../core/geometry_type.hpp"
#include "../../window_type.h"
#include "native_window.h"

/* Which native window a slot shows, and how to open it when it is not up. */
struct DockSpec {
	WindowClass wc;
	void (*open)(WindowNumber num);
};

struct NativeKey {
	WindowClass wc;
	WindowNumber num;

	bool operator==(const NativeKey &) const = default;
};

struct DockedWindow {
	NativeKey key;
	bool used;
	/* Windows the mini UI opened itself go away with their slot; windows it
	 * merely adopted are handed back where the player can still reach them. */
	bool owned;
};

/* The native paint lands in the CPU screen buffer below the mini layer and
 * the body samples the region beneath the native caption, so official content
 * arrives inside mini chrome without a frame of its own. */
class NativeDock {
public:
	const DockedWindow *Find(const Window *w) const;
	bool Docks(const Window *w) const;

	Window *Open(const DockSpec &spec, WindowNumber num);
	void Pin(Window *w, const Rect &slot, const NativeSizing &sizing);
	void Carry(Window *w, const Rect &slot);

	void Unmark();
	void Mark(NativeKey key);
	void Sweep();
	void CloseAll();
	void Stack(const std::vector<NativeKey> &want);

private:
	void Hold(NativeKey key, bool owned);
	std::vector<NativeKey> DockedOrder() const;

	std::vector<DockedWindow> windows;
	std::vector<NativeKey> applied;
	std::vector<NativeKey> settled;
};

extern NativeDock _dock;

#endif /* MINI_DOCK_NATIVE_DOCK_H */
