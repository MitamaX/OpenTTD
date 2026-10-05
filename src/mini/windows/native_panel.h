/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file native_panel.h A panel that adopts a standalone official window, so it wears mini chrome instead of its own frame. */

#ifndef MINI_WINDOWS_NATIVE_PANEL_H
#define MINI_WINDOWS_NATIVE_PANEL_H

#include "../../window_type.h"
#include "window_panel.h"

class ViewHost;

class NativePanel final : public WindowPanel {
public:
	NativePanel(WindowClass window_class, WindowNumber number);

	static void AdoptAll(ViewHost &views);

	bool IsAlive() const override;

private:
	std::optional<EmbedTarget> Embed() const override;
	void Fill() override;
	void OnDismiss() override;

	const WindowClass window_class;
	const WindowNumber number;
};

#endif /* MINI_WINDOWS_NATIVE_PANEL_H */
