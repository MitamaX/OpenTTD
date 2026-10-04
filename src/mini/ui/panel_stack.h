/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file panel_stack.h Opens, stacks, places and feeds the mini panels. */

#ifndef MINI_UI_PANEL_STACK_H
#define MINI_UI_PANEL_STACK_H

#include <memory>
#include <vector>

#include "panel.h"

struct PanelBounds {
	Rml::Vector2f screen;
	float dp_ratio = 1.0f;
	float dock_top = 0.0f;
};

class PanelStack {
public:
	void Reset(Rml::Context *context);
	void SetBounds(const PanelBounds &bounds) { this->bounds = bounds; }

	void Show(std::unique_ptr<Panel> panel);
	bool CloseFront();
	void CloseAll();

	void Refresh();
	void Confine();
	void Retire();

private:
	Panel *Find(const std::string &key) const;
	Panel *Front() const;
	size_t CountOpen() const;
	void Place(Panel &panel) const;
	Rml::Vector2f Confined(Rml::Vector2f size, Rml::Vector2f position) const;

	Rml::Context *context = nullptr;
	std::vector<std::unique_ptr<Panel>> panels;
	uint32_t serial = 0;
	PanelBounds bounds;
};

#endif /* MINI_UI_PANEL_STACK_H */
