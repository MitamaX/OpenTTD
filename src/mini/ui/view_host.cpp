/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file view_host.cpp The RmlUi layer with everything drawn on it: the HUD with the map at its foot, the panels above, the floating native windows on top. */

#include "../../stdafx.h"
#include "view_host.h"

#include <RmlUi/Core.h>

#include "../../core/utf8.hpp"
#include "../../string_func.h"
#include "ledger.h"
#include "map_view.h"
#include "menu_tile.h"
#include "native_slot.h"

#include "../../safeguards.h"

static constexpr const char VIEW_TYPES_MODEL[] = "view_types";

static void RegisterViewTypes(Rml::Context &context)
{
	Rml::DataModelConstructor types = context.CreateDataModel(VIEW_TYPES_MODEL);

	Rml::StructHandle<PanelCommand> command = types.RegisterStruct<PanelCommand>();
	command.RegisterMember("label", &PanelCommand::label);
	command.RegisterMember("enabled", &PanelCommand::enabled);
	command.RegisterMember("active", &PanelCommand::active);
	types.RegisterArray<Rml::Vector<PanelCommand>>();

	Rml::StructHandle<StripBlock> block = types.RegisterStruct<StripBlock>();
	block.RegisterMember("width", &StripBlock::width);
	block.RegisterMember("colour", &StripBlock::colour);
	block.RegisterMember("engine", &StripBlock::engine);
	block.RegisterMember("active", &StripBlock::active);
	types.RegisterArray<Rml::Vector<StripBlock>>();

	Rml::StructHandle<LedgerLine> line = types.RegisterStruct<LedgerLine>();
	line.RegisterMember("label", &LedgerLine::label);
	line.RegisterMember("value", &LedgerLine::value);
	line.RegisterMember("tone", &LedgerLine::tone);
	line.RegisterMember("label_tone", &LedgerLine::label_tone);
	line.RegisterMember("key", &LedgerLine::key);
	line.RegisterMember("total", &LedgerLine::total);
	line.RegisterMember("link", &LedgerLine::link);
	line.RegisterMember("active", &LedgerLine::active);
	line.RegisterMember("blocks", &LedgerLine::blocks);
	types.RegisterArray<Rml::Vector<LedgerLine>>();

	Rml::StructHandle<LedgerSection> section = types.RegisterStruct<LedgerSection>();
	section.RegisterMember("title", &LedgerSection::title);
	section.RegisterMember("lines", &LedgerSection::lines);
	types.RegisterArray<Rml::Vector<LedgerSection>>();

	Rml::StructHandle<LedgerColumn> column = types.RegisterStruct<LedgerColumn>();
	column.RegisterMember("sections", &LedgerColumn::sections);
	types.RegisterArray<Rml::Vector<LedgerColumn>>();

	types.RegisterArray<Rml::Vector<Rml::String>>();

	Rml::StructHandle<MenuTile> tile = types.RegisterStruct<MenuTile>();
	tile.RegisterMember("label", &MenuTile::label);
	tile.RegisterMember("icon", &MenuTile::icon);
	tile.RegisterMember("active", &MenuTile::active);
	types.RegisterArray<Rml::Vector<MenuTile>>();

	context.RemoveDataModel(VIEW_TYPES_MODEL);
}

ViewHost::ViewHost(std::vector<std::unique_ptr<HudPart>> hud_parts) : hud(std::move(hud_parts))
{
}

void ViewHost::Frame(int width, int height, float dp_ratio, int dock_top, std::span<const Rect> floating)
{
	this->panels.SetBounds({Rml::Vector2f(static_cast<float>(width), static_cast<float>(height)), dp_ratio, static_cast<float>(dock_top)});
	if (!this->Attach()) return;

	this->hud.Refresh();
	this->panels.Refresh();
	this->floats.Show(floating);
	this->layer.Update(width, height, dp_ratio);
	this->panels.Settle();
	this->panels.Retire();
}

Panel *ViewHost::Show(std::unique_ptr<Panel> panel)
{
	return this->Attach() ? this->panels.Show(std::move(panel)) : nullptr;
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

/* RmlUi alone says what lies under the pointer. A native slot hands it to the
 * official window it shows; a slot that only shows a camera takes no pointer,
 * so the panel around it keeps the click. */
PointerLayer ViewHost::LayerAt(int x, int y) const
{
	const Rml::Element *element = this->layer.ElementAt(x, y);
	if (element == nullptr || element->GetTagName() == MapView::TAG) return PointerLayer::Map;
	return element->GetTagName() == NativeSlot::TAG ? PointerLayer::Native : PointerLayer::Panel;
}

/* Asked outside the frame too, when a lost GL context may already have taken the panels' documents with it. */
void ViewHost::ListNatives(std::vector<NativeKey> &natives)
{
	this->Sync();
	this->panels.ListNatives(natives);
}

const Panel *ViewHost::Front()
{
	this->Sync();
	return this->panels.Front();
}

void ViewHost::FeedPointer()
{
	this->layer.Feed(RmlPointer::Current());
}

void ViewHost::LeavePointer(bool pressed)
{
	this->layer.Leave(pressed);
}

/* Drivers that translate key presses hand the character over with the key; the others send it as text. */
bool ViewHost::ProcessKey(uint keycode, char32_t character)
{
	if (!this->IsTyping()) return false;
	this->layer.ProcessKey(RmlKey::FromKeycode(keycode));
	if (IsValidChar(character, CS_ALPHANUMERAL)) {
		auto [buffer, length] = EncodeUtf8(character);
		this->layer.ProcessText(std::string_view(buffer, length));
	}
	return true;
}

bool ViewHost::ProcessText(std::string_view text, bool marked)
{
	if (!this->IsTyping()) return false;
	if (marked) {
		this->layer.Compose(text);
	} else {
		this->layer.ProcessText(text);
	}
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
	this->floats.Reset(current);
}
