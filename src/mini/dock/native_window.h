/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file native_window.h Moving, sizing and reading the native windows the mini UI docks. */

#ifndef MINI_DOCK_NATIVE_WINDOW_H
#define MINI_DOCK_NATIVE_WINDOW_H

#include <string>

struct Window;

/* How far a native window lets a slot size it. Resize steps are no test: a
 * caption puts a horizontal one on every window. A resize box is what the
 * player can actually drag. */
struct NativeSizing {
	bool fix_w;
	bool fix_h;
	int step_w;
	int step_h;
	int min_w;
	int min_h;

	bool Pinned() const { return this->fix_w && this->fix_h; }
};

NativeSizing SizingOf(const Window *w);
int CaptionCrop(const Window *w);
void ResizeInSteps(Window *w, int want_w, int want_h, const NativeSizing &sizing);
void MoveNativeWindow(Window *w, int x, int y);
bool NativeWrappable(const Window *w);
std::string NativeCaption(Window *w);

#endif /* MINI_DOCK_NATIVE_WINDOW_H */
