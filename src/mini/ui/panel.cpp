/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file panel.cpp A mini window: an RML document bound to a data model of its own. */

#include "../../stdafx.h"
#include "panel.h"

#include <RmlUi/Core.h>

#include "../../core/format.hpp"

#include "../../safeguards.h"

static constexpr std::string_view RML_BODY_TAG = "<body";

/* RmlUi binds a document to its data model while parsing, so the model name has to be in the body tag beforehand. */
static Rml::String BindBodyToModel(Rml::String rml, const Rml::String &model_name)
{
	size_t body = rml.find(RML_BODY_TAG);
	if (body != Rml::String::npos) rml.insert(body + RML_BODY_TAG.size(), fmt::format(" data-model=\"{}\"", model_name));
	return rml;
}

Panel::Panel(std::string key, std::string document_path, Rml::String title, Rml::Vector<Rml::String> tabs) :
	title(std::move(title)), key(std::move(key)), document_path(std::move(document_path)), tabs(std::move(tabs))
{
}

bool Panel::Open(Rml::Context &context, Rml::String model_name)
{
	Rml::String rml;
	if (!Rml::GetFileInterface()->LoadFile(this->document_path, rml)) return false;
	if (!this->CreateModel(context, std::move(model_name))) return false;

	this->document = context.LoadDocumentFromMemory(BindBodyToModel(std::move(rml), this->ModelName()), this->document_path);
	if (this->document == nullptr) {
		this->RemoveModel(context);
		return false;
	}
	this->document->Show();
	return true;
}

void Panel::Close()
{
	if (this->document == nullptr) return;
	this->document->Close();
	this->document = nullptr;
}

void Panel::Raise()
{
	if (this->document != nullptr) this->document->PullToFront();
}

Rml::Vector2f Panel::Size() const
{
	return this->document->GetBox().GetSize(Rml::BoxArea::Border);
}

Rml::Vector2f Panel::Position() const
{
	return this->document->GetAbsoluteOffset(Rml::BoxArea::Border);
}

void Panel::MoveTo(Rml::Vector2f position)
{
	this->document->SetProperty(Rml::PropertyId::Left, Rml::Property(position.x, Rml::Unit::PX));
	this->document->SetProperty(Rml::PropertyId::Top, Rml::Property(position.y, Rml::Unit::PX));
}

void Panel::Bind(Rml::DataModelConstructor &model)
{
	model.Bind("title", &this->title);
	model.Bind("tabs", &this->tabs);
	model.Bind("tab", &this->tab);
	model.Bind("commands", &this->commands);
	model.BindEventCallback("run", &Panel::Run, this);
	model.BindEventCallback("close", &Panel::Dismiss, this);
	this->BindSheet(model);
}

void Panel::BindSheet(Rml::DataModelConstructor &)
{
}

void Panel::Run(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	int index = ArgumentIndex(arguments);
	if (index < 0 || static_cast<size_t>(index) >= this->commands.size()) return;

	const PanelCommand &command = this->commands[index];
	if (command.enabled) command.action();
}

void Panel::Dismiss(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	this->Close();
}
