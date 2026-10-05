/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file panel_stack.cpp Opens, stacks, places and feeds the mini panels. */

#include "../../stdafx.h"
#include "panel_stack.h"

#include <RmlUi/Core.h>

#include "../../core/format.hpp"
#include "../../core/math_func.hpp"

#include "../../safeguards.h"

static constexpr float PANEL_MARGIN = 6.0f;
static constexpr float PANEL_CASCADE = 20.0f;

/* Panels die with the context they were opened in. */
void PanelStack::Reset(Rml::Context *context)
{
	this->panels.clear();
	this->context = context;
}

Panel *PanelStack::Show(std::unique_ptr<Panel> panel)
{
	if (Panel *open = this->Find(panel->Key()); open != nullptr) {
		open->Raise();
		return open;
	}
	if (this->context == nullptr || !panel->IsAlive()) return nullptr;
	if (!panel->Open(*this->context, fmt::format("panel{}", ++this->serial))) return nullptr;

	this->Place(*panel);
	return this->panels.emplace_back(std::move(panel)).get();
}

bool PanelStack::CloseFront()
{
	if (this->context == nullptr) return false;

	Panel *front = this->Front();
	if (front == nullptr) return false;
	front->Close();
	return true;
}

void PanelStack::CloseAll()
{
	for (const auto &panel : this->panels) panel->Close();
}

void PanelStack::Refresh()
{
	for (const auto &panel : this->panels) {
		if (panel->IsOpen() && !panel->IsAlive()) panel->Close();
		if (panel->IsOpen()) panel->Refresh();
	}
}

void PanelStack::Confine()
{
	for (const auto &panel : this->panels) {
		if (!panel->IsOpen()) continue;
		Rml::Vector2f position = panel->Position();
		Rml::Vector2f confined = this->Confined(panel->Size(), position);
		if (confined != position) panel->MoveTo(confined);
	}
}

void PanelStack::Settle()
{
	for (const auto &panel : this->panels) {
		if (panel->IsOpen()) panel->Settle();
	}
}

/* A closed document is only released by the next context update; its data model must outlive it until then. */
void PanelStack::Retire()
{
	for (const auto &panel : this->panels) {
		if (!panel->IsOpen()) panel->RemoveModel(*this->context);
	}
	std::erase_if(this->panels, [](const auto &panel) { return !panel->IsOpen(); });
}

Panel *PanelStack::Find(const std::string &key) const
{
	for (const auto &panel : this->panels) {
		if (panel->IsOpen() && panel->Key() == key) return panel.get();
	}
	return nullptr;
}

Panel *PanelStack::Front() const
{
	for (int i = this->context->GetNumDocuments(); i-- > 0;) {
		const Rml::ElementDocument *document = this->context->GetDocument(i);
		for (const auto &panel : this->panels) {
			if (panel->Owns(document)) return panel.get();
		}
	}
	return nullptr;
}

size_t PanelStack::CountOpen() const
{
	return std::ranges::count_if(this->panels, [](const auto &panel) { return panel->IsOpen(); });
}

void PanelStack::Place(Panel &panel) const
{
	float margin = PANEL_MARGIN * this->bounds.dp_ratio;
	float cascade = PANEL_CASCADE * this->bounds.dp_ratio * this->CountOpen();
	Rml::Vector2f size = panel.Size();
	Rml::Vector2f docked(this->bounds.screen.x - margin - size.x - cascade, this->bounds.dock_top + margin + cascade);
	panel.MoveTo(this->Confined(size, docked));
}

Rml::Vector2f PanelStack::Confined(Rml::Vector2f size, Rml::Vector2f position) const
{
	return Rml::Vector2f(
		Clamp(position.x, 0.0f, std::max(0.0f, this->bounds.screen.x - size.x)),
		Clamp(position.y, 0.0f, std::max(0.0f, this->bounds.screen.y - size.y)));
}
