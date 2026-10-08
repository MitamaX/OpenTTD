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

#include "model_tag.h"
#include "pixel_style.h"

#include "../../safeguards.h"

static constexpr const char EDIT_FIELD[] = ".editing .edit";
static constexpr const char WIDE_CLASS[] = "wide";
static constexpr std::chrono::milliseconds REFRESH_INTERVAL{250};
static constexpr Rml::EventId TOUCH_EVENTS[] = {
	Rml::EventId::Mousedown, Rml::EventId::Mouseup, Rml::EventId::Click, Rml::EventId::Dblclick, Rml::EventId::Mouseover, Rml::EventId::Mousescroll,
	Rml::EventId::Drag, Rml::EventId::Keydown, Rml::EventId::Textinput, Rml::EventId::Change, Rml::EventId::Blur,
};

static Rml::String KeyArgument(const Rml::VariantList &arguments)
{
	return arguments.empty() ? Rml::String() : arguments[0].Get<Rml::String>();
}

/* Only what RmlUi shows counts; the action is looked up by index when it fires. */
bool PanelCommand::operator==(const PanelCommand &other) const
{
	return this->label == other.label && this->enabled == other.enabled && this->active == other.active;
}

Panel::Panel(std::string key, std::string document_path, Rml::String title, Rml::Vector<Rml::String> tabs) :
	title(std::move(title)), key(std::move(key)), document_path(std::move(document_path)), tabs(std::move(tabs)), refresh_beat(REFRESH_INTERVAL)
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
	this->document->SetClass(WIDE_CLASS, this->wide);
	for (Rml::EventId event : TOUCH_EVENTS) this->document->AddEventListener(event, &this->touch, true);
	this->document->Show();
	return true;
}

void Panel::ListNatives(std::vector<NativeKey> &) const
{
}

void Panel::Close()
{
	if (this->document == nullptr) return;
	for (Rml::EventId event : TOUCH_EVENTS) this->document->RemoveEventListener(event, &this->touch, true);
	this->document->Close();
	this->document = nullptr;
}

/* The player sent the panel away, as opposed to the mini UI closing it. */
void Panel::Dismiss()
{
	this->OnDismiss();
	this->Close();
}

void Panel::Raise()
{
	if (this->document != nullptr) this->document->PullToFront();
}

void Panel::RefreshWhenDue()
{
	if (!this->touch.touched && !this->refresh_beat.Due()) return;
	this->touch.touched = false;
	this->Refresh();
}

/* A panel is laid out again at once when it changed shape, so it is drawn at the size its slots are pinned to. */
void Panel::Reshape()
{
	if (this->Shape()) this->document->UpdateDocument();
}

/* Runs once the document has been laid out, when element boxes are final for the frame. */
void Panel::Settle()
{
	if (this->focus_pending) this->FocusEdit();
	this->AfterLayout();
}

void Panel::SelectTab(int tab)
{
	this->tab = tab;
	this->touch.touched = true;
}

void Panel::BeginEdit(Rml::String key, Rml::String text)
{
	this->editing = std::move(key);
	this->draft = std::move(text);
	this->focus_pending = true;
	this->touch.touched = true;
}

Rml::Vector2f Panel::Size() const
{
	return this->document->GetBox().GetSize(Rml::BoxArea::Border);
}

Rml::Vector2f Panel::Position() const
{
	return this->document->GetAbsoluteOffset(Rml::BoxArea::Border);
}

/* A move is laid out at once, so the slots are pinned where the panel is drawn. */
void Panel::MoveTo(Rml::Vector2f position)
{
	bool moved = SetPixels(*this->document, Rml::PropertyId::Left, position.x);
	moved |= SetPixels(*this->document, Rml::PropertyId::Top, position.y);
	if (moved) this->document->UpdateDocument();
}

void Panel::Bind(Rml::DataModelConstructor &model)
{
	this->Expose(model, "title", &this->title);
	this->Expose(model, "tabs", &this->tabs);
	this->Expose(model, "tab", &this->tab);
	this->Expose(model, "commands", &this->commands);
	this->Expose(model, "editing", &this->editing);
	this->Expose(model, "draft", &this->draft);
	this->Expose(model, "embedded", &this->embedded);
	this->Expose(model, "sizable", &this->sizable);
	model.BindEventCallback("run", &Panel::Run, this);
	model.BindEventCallback("close", &Panel::DismissClicked, this);
	model.BindEventCallback("edit_title", &Panel::EditTitle, this);
	model.BindEventCallback("commit", &Panel::Commit, this);
	model.BindEventCallback("cancel", &Panel::Cancel, this);
	this->BindSheet(model);
}

void Panel::BindSheet(Rml::DataModelConstructor &)
{
}

bool Panel::Shape()
{
	return false;
}

void Panel::AfterLayout()
{
}

void Panel::OnDismiss()
{
}

void Panel::Rename(std::string)
{
}

void Panel::Apply(const Rml::String &key, std::string text)
{
	if (key == TITLE_KEY && this->Renamable() && !text.empty()) this->Rename(std::move(text));
}

void Panel::Run(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	int index = ArgumentIndex(arguments);
	if (index < 0 || static_cast<size_t>(index) >= this->commands.size()) return;

	const PanelCommand &command = this->commands[index];
	if (command.enabled) command.action();
}

void Panel::DismissClicked(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	this->Dismiss();
}

void Panel::EditTitle(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	if (this->Renamable()) this->BeginEdit(TITLE_KEY, this->title);
}

/* A text field reports every keystroke as a change; only the one Enter sends carries a line break. */
void Panel::Commit(Rml::DataModelHandle, Rml::Event &event, const Rml::VariantList &arguments)
{
	if (!event.GetParameter<bool>("linebreak", false)) return;

	Rml::String key = KeyArgument(arguments);
	if (key != this->editing) return;

	this->editing.clear();
	event.GetTargetElement()->Blur();
	this->Apply(key, event.GetParameter<Rml::String>("value", ""));
}

/* Leaving the field abandons the edit; a field left behind by a newer edit must not end that one. */
void Panel::Cancel(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
{
	if (KeyArgument(arguments) == this->editing) this->editing.clear();
}

/* The field only exists once the layout after BeginEdit has shown it. */
void Panel::FocusEdit()
{
	auto *field = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(this->document->QuerySelector(EDIT_FIELD));
	if (field == nullptr) return;

	field->Focus();
	field->Select();
	this->focus_pending = false;
}
