/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file view.cpp Game state bound to RmlUi through a data model of its own. */

#include "../../stdafx.h"
#include "view.h"

#include <RmlUi/Core.h>

#include "../../safeguards.h"

int ArgumentIndex(const Rml::VariantList &arguments, size_t position)
{
	return position < arguments.size() ? arguments[position].Get<int>(-1) : -1;
}

void View::Refresh()
{
	this->Collect();
	for (Exposed &variable : this->exposed) {
		if (variable.changed()) this->model.DirtyVariable(variable.name);
	}
}

void View::RemoveModel(Rml::Context &context)
{
	context.RemoveDataModel(this->model_name);
	this->model = {};
	this->exposed.clear();
}

bool View::CreateModel(Rml::Context &context, Rml::String model_name)
{
	Rml::DataModelConstructor constructor = context.CreateDataModel(model_name);
	if (!constructor) return false;

	this->exposed.clear();
	this->Bind(constructor);
	this->model = constructor.GetModelHandle();
	this->model_name = std::move(model_name);
	this->Refresh();
	return true;
}
