/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file build_dock.cpp The bottom-left corner: the build categories, the tools of the open one and the choices of the tool in hand. */

#include "../../stdafx.h"
#include "build_dock.h"

#include <RmlUi/Core.h>

#include "../input/input_mode.h"
#include "../tools/build_tool.h"
#include "../tools/tool_choices.h"
#include "build_catalog.h"

#include "../../safeguards.h"

static constexpr const char CHOICE_LIST_ID[] = "build-choices";

MenuShelf _build_shelf;

BuildDock::BuildDock() : HudPart("build")
{
}

void BuildDock::Bind(Rml::DataModelConstructor &model)
{
	if (Rml::StructHandle<FacingCell> cell = model.RegisterStruct<FacingCell>()) {
		cell.RegisterMember("value", &FacingCell::value);
		cell.RegisterMember("turn", &FacingCell::turn);
		cell.RegisterMember("active", &FacingCell::active);
	}
	model.RegisterArray<Rml::Vector<FacingCell>>();
	if (Rml::StructHandle<BuildRow> row = model.RegisterStruct<BuildRow>()) {
		row.RegisterMember("text", &BuildRow::text);
		row.RegisterMember("option", &BuildRow::option);
		row.RegisterMember("value", &BuildRow::value);
		row.RegisterMember("active", &BuildRow::active);
		row.RegisterMember("head", &BuildRow::head);
		row.RegisterMember("mark", &BuildRow::mark);
		row.RegisterMember("cells", &BuildRow::cells);
	}
	model.RegisterArray<Rml::Vector<BuildRow>>();

	model.Bind("categories", &this->categories);
	model.Bind("shelf", &this->shelf);
	model.Bind("tools", &this->tools);
	model.Bind("title", &this->title);
	model.Bind("rows", &this->rows);
	model.BindEventCallback("toggle", &BuildDock::Toggle, this);
	model.BindEventCallback("pick", &BuildDock::Pick, this);
	model.BindEventCallback("apply", &BuildDock::Apply, this);
}

void BuildDock::Collect()
{
	std::span<const MiniMenuCategory> categories = BuildCategories();
	this->categories.clear();
	for (int i = 0; i < static_cast<int>(categories.size()); i++) this->categories.push_back(CategoryTile(categories[i], i == _build_shelf.Open()));
	this->shelf = _build_shelf.IsOpen();
	this->CollectTools();
	this->CollectChoices();
}

void BuildDock::CollectTools()
{
	this->tools.clear();
	if (!_build_shelf.IsOpen()) return;
	for (const MiniMenuItem &item : BuildCategories()[_build_shelf.Open()].items) this->tools.push_back(ToolTile(item));
}

void BuildDock::CollectChoices()
{
	MiniTool kind = _tool.Kind();
	this->title = kind == MiniTool::None ? Rml::String() : ToolLabel(kind);
	this->rows = CollectBuildRows(kind);
	if (kind != this->listed) this->ScrollChoicesHome();
	this->listed = kind;
}

/* A new tool lists other choices, so the list starts again from its first row. */
void BuildDock::ScrollChoicesHome()
{
	Rml::Element *root = this->Root();
	Rml::Element *list = root == nullptr ? nullptr : root->GetElementById(CHOICE_LIST_ID);
	if (list != nullptr) list->SetScrollTop(0.0f);
}

/* The shelf is where a tool is picked, so pushing it back puts the tool down as well. */
void BuildDock::Toggle(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	int index = ArgumentSlot(BuildCategories(), arguments);
	if (index >= 0 && !_build_shelf.Toggle(index)) _mode.Idle();
}

void BuildDock::Pick(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	if (!_build_shelf.IsOpen()) return;
	const MiniMenuItem *item = ArgumentItem(BuildCategories()[_build_shelf.Open()].items, arguments);
	if (item != nullptr) _mode.ToggleBuild(item->tool);
}

void BuildDock::Apply(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	if (arguments.size() < 2) return;
	_choices.Apply(static_cast<ToolOption>(arguments[0].Get<int>()), arguments[1].Get<int>());
}
