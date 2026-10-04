/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file panel.h A mini window: an RML document bound to a data model of its own. */

#ifndef MINI_UI_PANEL_H
#define MINI_UI_PANEL_H

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include <functional>
#include <string>

namespace Rml { class ElementDocument; }

struct PanelCommand {
	Rml::String label;
	bool enabled = false;
	std::function<void()> action;
};

class Panel {
public:
	virtual ~Panel() = default;

	const std::string &Key() const { return this->key; }
	const Rml::String &ModelName() const { return this->model_name; }
	bool IsOpen() const { return this->document != nullptr; }
	bool Owns(const Rml::ElementDocument *document) const { return this->document == document; }
	virtual bool IsAlive() const { return true; }

	bool Open(Rml::Context &context, Rml::String model_name);
	void Close();
	void Raise();
	void Refresh();

	Rml::Vector2f Size() const;
	Rml::Vector2f Position() const;
	void MoveTo(Rml::Vector2f position);

protected:
	Panel(std::string key, std::string document_path, Rml::String title, Rml::Vector<Rml::String> tabs);

	virtual void Collect() = 0;
	virtual void Bind(Rml::DataModelConstructor &model);

	Rml::String title;
	int tab = 0;
	Rml::Vector<PanelCommand> commands;

private:
	void Run(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);
	void Dismiss(Rml::DataModelHandle model, Rml::Event &event, const Rml::VariantList &arguments);

	const std::string key;
	const std::string document_path;
	Rml::Vector<Rml::String> tabs;
	Rml::String model_name;
	Rml::ElementDocument *document = nullptr;
	Rml::DataModelHandle model;
};

#endif /* MINI_UI_PANEL_H */
