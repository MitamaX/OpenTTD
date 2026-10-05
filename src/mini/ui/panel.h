/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file panel.h A mini window: an RML document bound to a data model of its own. */

#ifndef MINI_UI_PANEL_H
#define MINI_UI_PANEL_H

#include <functional>
#include <string>
#include <vector>

#include "view.h"

namespace Rml { class ElementDocument; }
struct NativeKey;

struct PanelCommand {
	Rml::String label;
	bool enabled = false;
	std::function<void()> action;
	bool active = false;
};

class Panel : public View {
public:
	const std::string &Key() const { return this->key; }
	bool IsOpen() const { return this->document != nullptr; }
	bool Owns(const Rml::ElementDocument *document) const { return this->document == document; }
	virtual bool IsAlive() const { return true; }
	virtual void ListNatives(std::vector<NativeKey> &natives) const;

	bool Open(Rml::Context &context, Rml::String model_name);
	void Close();
	void Dismiss();
	void Raise();
	void Settle();

	void SelectTab(int tab) { this->tab = tab; }
	void BeginEdit(Rml::String key, Rml::String text);

	Rml::Vector2f Size() const;
	Rml::Vector2f Position() const;
	void MoveTo(Rml::Vector2f position);

protected:
	static constexpr const char TITLE_KEY[] = "title";

	Panel(std::string key, std::string document_path, Rml::String title, Rml::Vector<Rml::String> tabs);

	void Bind(Rml::DataModelConstructor &model) final;
	virtual void BindSheet(Rml::DataModelConstructor &model);
	virtual void AfterLayout();
	virtual void OnDismiss();

	virtual bool Renamable() const { return false; }
	virtual void Rename(std::string name);
	virtual void Apply(const Rml::String &key, std::string text);

	Rml::ElementDocument *Document() const { return this->document; }

	Rml::String title;
	int tab = 0;
	Rml::Vector<PanelCommand> commands;
	bool embedded = false;
	bool sizable = false;
	bool wide = false;

private:
	void Run(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);
	void DismissClicked(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);
	void EditTitle(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);
	void Commit(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);
	void Cancel(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);
	void FocusEdit();

	const std::string key;
	const std::string document_path;
	Rml::Vector<Rml::String> tabs;
	Rml::ElementDocument *document = nullptr;
	Rml::String editing;
	Rml::String draft;
	bool focus_pending = false;
};

#endif /* MINI_UI_PANEL_H */
