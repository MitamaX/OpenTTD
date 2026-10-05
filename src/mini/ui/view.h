/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file view.h Game state bound to RmlUi through a data model of its own. */

#ifndef MINI_UI_VIEW_H
#define MINI_UI_VIEW_H

#include <span>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

int ArgumentIndex(const Rml::VariantList &arguments, size_t position = 0);

template <typename T>
int ArgumentSlot(std::span<const T> items, const Rml::VariantList &arguments)
{
	int index = ArgumentIndex(arguments);
	return index >= 0 && static_cast<size_t>(index) < items.size() ? index : -1;
}

template <typename T>
const T *ArgumentItem(std::span<const T> items, const Rml::VariantList &arguments)
{
	int slot = ArgumentSlot(items, arguments);
	return slot < 0 ? nullptr : &items[slot];
}

class View {
public:
	virtual ~View() = default;

	const Rml::String &ModelName() const { return this->model_name; }

	void Refresh();
	void RemoveModel(Rml::Context &context);

protected:
	bool CreateModel(Rml::Context &context, Rml::String model_name);

	virtual void Bind(Rml::DataModelConstructor &model) = 0;
	virtual void Collect() = 0;

private:
	Rml::String model_name;
	Rml::DataModelHandle model;
};

#endif /* MINI_UI_VIEW_H */
