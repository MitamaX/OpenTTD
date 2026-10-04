/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file view_host.cpp The RmlUi layer with everything drawn on it: the HUD below, the panels above. */

#include "../../stdafx.h"
#include "view_host.h"

#include <RmlUi/Core.h>

#include "ledger.h"

#include "../../safeguards.h"

static constexpr const char VIEW_TYPES_MODEL[] = "view_types";

static void RegisterViewTypes(Rml::Context &context)
{
	Rml::DataModelConstructor types = context.CreateDataModel(VIEW_TYPES_MODEL);

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

	context.RemoveDataModel(VIEW_TYPES_MODEL);
}

ViewHost::ViewHost(std::vector<std::unique_ptr<HudPart>> hud_parts) : hud(std::move(hud_parts))
{
}

void ViewHost::Frame(int width, int height, float dp_ratio, int dock_top)
{
	this->panels.SetBounds({Rml::Vector2f(static_cast<float>(width), static_cast<float>(height)), dp_ratio, static_cast<float>(dock_top)});
	if (!this->Attach()) return;

	this->hud.Refresh();
	this->panels.Refresh();
	this->layer.Update(width, height, dp_ratio);
	this->panels.Confine();
	this->panels.Retire();
}

void ViewHost::Show(std::unique_ptr<Panel> panel)
{
	if (this->Attach()) this->panels.Show(std::move(panel));
}

bool ViewHost::CloseFront()
{
	this->Sync();
	return this->panels.CloseFront();
}

void ViewHost::CloseAll()
{
	this->Sync();
	this->panels.CloseAll();
}

void ViewHost::ReloadDesign()
{
	this->Sync();
	if (this->context == nullptr) return;
	Rml::Factory::ClearStyleSheetCache();
	Rml::Factory::ClearTemplateCache();
	this->hud.ReloadStyleSheet();
}

void ViewHost::TrackPointer()
{
	this->layer.TrackPointer(RmlPointer::Current());
}

bool ViewHost::CapturePointer()
{
	return this->layer.CapturePointer(RmlPointer::Current());
}

bool ViewHost::ProcessKey(uint keycode)
{
	if (!this->IsTyping()) return false;
	this->layer.ProcessKey(RmlKey::FromKeycode(keycode));
	return true;
}

bool ViewHost::ProcessText(char32_t character)
{
	if (!this->IsTyping()) return false;
	this->layer.ProcessText(character);
	return true;
}

bool ViewHost::Attach()
{
	this->layer.Acquire();
	this->Sync();
	return this->context != nullptr;
}

/* Every view dies with the context it was bound in; the layer may have been detached under them. */
void ViewHost::Sync()
{
	Rml::Context *current = this->layer.Current();
	if (current == this->context) return;

	this->context = current;
	if (current != nullptr) RegisterViewTypes(*current);
	this->panels.Reset(current);
	this->hud.Reset(current);
}
