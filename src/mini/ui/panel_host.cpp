/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file panel_host.cpp Opens, stacks, places and feeds the mini panels on the RmlUi layer. */

#include "../../stdafx.h"
#include "panel_host.h"

#include <RmlUi/Core.h>

#include "../../core/format.hpp"
#include "../../core/math_func.hpp"
#include "ledger.h"

#include "../../safeguards.h"

static constexpr float PANEL_MARGIN = 6.0f;
static constexpr float PANEL_CASCADE = 20.0f;
static constexpr const char PANEL_TYPES_MODEL[] = "panel_types";

static void RegisterPanelTypes(Rml::Context &context)
{
	Rml::DataModelConstructor types = context.CreateDataModel(PANEL_TYPES_MODEL);

	Rml::StructHandle<PanelCommand> command = types.RegisterStruct<PanelCommand>();
	command.RegisterMember("label", &PanelCommand::label);
	command.RegisterMember("enabled", &PanelCommand::enabled);
	types.RegisterArray<Rml::Vector<PanelCommand>>();

	Rml::StructHandle<LedgerLine> line = types.RegisterStruct<LedgerLine>();
	line.RegisterMember("label", &LedgerLine::label);
	line.RegisterMember("value", &LedgerLine::value);
	line.RegisterMember("tone", &LedgerLine::tone);
	line.RegisterMember("total", &LedgerLine::total);
	types.RegisterArray<Rml::Vector<LedgerLine>>();

	Rml::StructHandle<LedgerSection> section = types.RegisterStruct<LedgerSection>();
	section.RegisterMember("title", &LedgerSection::title);
	section.RegisterMember("lines", &LedgerSection::lines);
	types.RegisterArray<Rml::Vector<LedgerSection>>();

	types.RegisterArray<Rml::Vector<Rml::String>>();

	context.RemoveDataModel(PANEL_TYPES_MODEL);
}

void PanelHost::Frame(int width, int height, float dp_ratio, int dock_top)
{
	this->screen = Rml::Vector2f(static_cast<float>(width), static_cast<float>(height));
	this->dp_ratio = dp_ratio;
	this->dock_top = static_cast<float>(dock_top);
	if (!this->Attach()) return;

	for (const auto &panel : this->panels) {
		if (panel->IsOpen() && !panel->IsAlive()) panel->Close();
		if (panel->IsOpen()) panel->Refresh();
	}
	this->layer.Update(width, height, dp_ratio);
	for (const auto &panel : this->panels) {
		if (panel->IsOpen()) this->Confine(*panel);
	}
	this->Retire();
}

void PanelHost::Show(std::unique_ptr<Panel> panel)
{
	if (Panel *open = this->Find(panel->Key()); open != nullptr) {
		open->Raise();
		return;
	}
	if (!this->Attach() || !panel->IsAlive()) return;
	if (!panel->Open(*this->context, fmt::format("panel{}", ++this->serial))) return;

	this->Place(*panel);
	this->panels.push_back(std::move(panel));
}

bool PanelHost::CloseFront()
{
	this->Sync();
	if (this->context == nullptr) return false;

	Panel *front = this->Front();
	if (front == nullptr) return false;
	front->Close();
	return true;
}

void PanelHost::CloseAll()
{
	this->Sync();
	for (const auto &panel : this->panels) panel->Close();
}

void PanelHost::ReloadDesign()
{
	this->Sync();
	if (this->context == nullptr) return;
	Rml::Factory::ClearStyleSheetCache();
	Rml::Factory::ClearTemplateCache();
}

void PanelHost::TrackPointer()
{
	this->layer.TrackPointer(RmlPointer::Current());
}

bool PanelHost::CapturePointer()
{
	return this->layer.CapturePointer(RmlPointer::Current());
}

bool PanelHost::ProcessKey(uint keycode)
{
	if (!this->IsTyping()) return false;
	this->layer.ProcessKey(RmlKey::FromKeycode(keycode));
	return true;
}

bool PanelHost::ProcessText(char32_t character)
{
	if (!this->IsTyping()) return false;
	this->layer.ProcessText(character);
	return true;
}

bool PanelHost::Attach()
{
	this->layer.Acquire();
	this->Sync();
	return this->context != nullptr;
}

/* Panels die with the context they were opened in; the layer may have been detached under them. */
void PanelHost::Sync()
{
	Rml::Context *current = this->layer.Current();
	if (current == this->context) return;

	this->panels.clear();
	this->context = current;
	if (current != nullptr) RegisterPanelTypes(*current);
}

Panel *PanelHost::Find(const std::string &key) const
{
	for (const auto &panel : this->panels) {
		if (panel->IsOpen() && panel->Key() == key) return panel.get();
	}
	return nullptr;
}

Panel *PanelHost::Front() const
{
	for (int i = this->context->GetNumDocuments(); i-- > 0;) {
		const Rml::ElementDocument *document = this->context->GetDocument(i);
		for (const auto &panel : this->panels) {
			if (panel->Owns(document)) return panel.get();
		}
	}
	return nullptr;
}

size_t PanelHost::CountOpen() const
{
	return std::ranges::count_if(this->panels, [](const auto &panel) { return panel->IsOpen(); });
}

void PanelHost::Place(Panel &panel) const
{
	float margin = PANEL_MARGIN * this->dp_ratio;
	float cascade = PANEL_CASCADE * this->dp_ratio * this->CountOpen();
	Rml::Vector2f size = panel.Size();
	Rml::Vector2f docked(this->screen.x - margin - size.x - cascade, this->dock_top + margin + cascade);
	panel.MoveTo(this->Confined(size, docked));
}

void PanelHost::Confine(Panel &panel) const
{
	Rml::Vector2f position = panel.Position();
	Rml::Vector2f confined = this->Confined(panel.Size(), position);
	if (confined != position) panel.MoveTo(confined);
}

Rml::Vector2f PanelHost::Confined(Rml::Vector2f size, Rml::Vector2f position) const
{
	return Rml::Vector2f(
		Clamp(position.x, 0.0f, std::max(0.0f, this->screen.x - size.x)),
		Clamp(position.y, 0.0f, std::max(0.0f, this->screen.y - size.y)));
}

/* A closed document is only released by the next context update; its data model must outlive it until then. */
void PanelHost::Retire()
{
	for (const auto &panel : this->panels) {
		if (!panel->IsOpen()) this->context->RemoveDataModel(panel->ModelName());
	}
	std::erase_if(this->panels, [](const auto &panel) { return !panel->IsOpen(); });
}
