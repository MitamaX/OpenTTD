/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file view_host.h The RmlUi layer with everything drawn on it: the HUD below, the panels above. */

#ifndef MINI_UI_VIEW_HOST_H
#define MINI_UI_VIEW_HOST_H

#include "hud.h"
#include "panel_stack.h"
#include "rml_layer.h"

class ViewHost {
public:
	explicit ViewHost(std::vector<std::unique_ptr<HudPart>> hud_parts);

	void Frame(int width, int height, float dp_ratio, int dock_top);
	Panel *Show(std::unique_ptr<Panel> panel);
	bool CloseFront();
	void CloseAll();
	void ReloadDesign();

	void TrackPointer();
	bool CapturePointer();
	bool PointerOverLayer() const { return this->layer.Hovered() != nullptr; }
	bool PointerOverSlot() const;
	void ListNatives(std::vector<NativeKey> &natives) const { this->panels.ListNatives(natives); }
	const Panel *Front() const { return this->panels.Front(); }

	bool IsTyping() const { return this->layer.IsTyping(); }
	bool ProcessKey(uint keycode);
	bool ProcessText(char32_t character);

private:
	bool Attach();
	void Sync();

	RmlLayer layer;
	Rml::Context *context = nullptr;
	Hud hud;
	PanelStack panels;
};

#endif /* MINI_UI_VIEW_HOST_H */
