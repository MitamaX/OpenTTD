/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file panel_host.h Opens, stacks, places and feeds the mini panels on the RmlUi layer. */

#ifndef MINI_UI_PANEL_HOST_H
#define MINI_UI_PANEL_HOST_H

#include <memory>
#include <vector>

#include "panel.h"
#include "rml_layer.h"

class PanelHost {
public:
	void Frame(int width, int height, float dp_ratio, int dock_top);
	void Show(std::unique_ptr<Panel> panel);
	bool CloseFront();
	void CloseAll();
	void ReloadDesign();

	void TrackPointer();
	bool CapturePointer();

	bool IsTyping() const { return this->layer.IsTyping(); }
	bool ProcessKey(uint keycode);
	bool ProcessText(char32_t character);

private:
	bool Attach();
	void Sync();
	Panel *Find(const std::string &key) const;
	Panel *Front() const;
	size_t CountOpen() const;
	void Place(Panel &panel) const;
	void Confine(Panel &panel) const;
	Rml::Vector2f Confined(Rml::Vector2f size, Rml::Vector2f position) const;
	void Retire();

	RmlLayer layer;
	Rml::Context *context = nullptr;
	std::vector<std::unique_ptr<Panel>> panels;
	uint32_t serial = 0;
	Rml::Vector2f screen;
	float dp_ratio = 1.0f;
	float dock_top = 0.0f;
};

#endif /* MINI_UI_PANEL_HOST_H */
