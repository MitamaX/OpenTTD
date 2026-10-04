/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file json_model.cpp A data model whose variables are read straight out of JSON, so a scene file stands in for the game. */

#include "json_model.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/DataVariable.h>

using Json = nlohmann::json;

static Rml::DataVariable JsonVariable(Json &node);

class JsonScalar final : public Rml::VariableDefinition {
public:
	JsonScalar() : VariableDefinition(Rml::DataVariableType::Scalar) {}

	bool Get(void *ptr, Rml::Variant &variant) override
	{
		const Json &node = *static_cast<const Json *>(ptr);
		if (node.is_boolean()) {
			variant = node.get<bool>();
		} else if (node.is_number_integer()) {
			variant = node.get<int>();
		} else if (node.is_number_float()) {
			variant = node.get<double>();
		} else if (node.is_string()) {
			variant = node.get<Rml::String>();
		} else {
			variant = Rml::String();
		}
		return true;
	}

	/* An assignment in the document keeps the type the scene gave the value. */
	bool Set(void *ptr, const Rml::Variant &variant) override
	{
		Json &node = *static_cast<Json *>(ptr);
		if (node.is_boolean()) {
			node = variant.Get<bool>();
		} else if (node.is_number_integer()) {
			node = variant.Get<int>();
		} else if (node.is_number_float()) {
			node = variant.Get<double>();
		} else {
			node = variant.Get<Rml::String>();
		}
		return true;
	}
};

class JsonArray final : public Rml::VariableDefinition {
public:
	JsonArray() : VariableDefinition(Rml::DataVariableType::Array) {}

	int Size(void *ptr) override
	{
		return static_cast<int>(static_cast<const Json *>(ptr)->size());
	}

	Rml::DataVariable Child(void *ptr, const Rml::DataAddressEntry &address) override
	{
		Json &node = *static_cast<Json *>(ptr);
		int size = static_cast<int>(node.size());
		if (address.index >= 0 && address.index < size) return JsonVariable(node[address.index]);
		if (address.name == "size") return Rml::MakeLiteralIntVariable(size);
		return {};
	}
};

class JsonObject final : public Rml::VariableDefinition {
public:
	JsonObject() : VariableDefinition(Rml::DataVariableType::Struct) {}

	Rml::DataVariable Child(void *ptr, const Rml::DataAddressEntry &address) override
	{
		Json &node = *static_cast<Json *>(ptr);
		auto member = node.find(address.name);
		return member == node.end() ? Rml::DataVariable() : JsonVariable(*member);
	}
};

static Rml::DataVariable JsonVariable(Json &node)
{
	static JsonScalar scalar;
	static JsonArray array;
	static JsonObject object;

	if (node.is_array()) return {&array, &node};
	if (node.is_object()) return {&object, &node};
	return {&scalar, &node};
}

bool BindJsonModel(Rml::Context &context, const Rml::String &name, Json &values)
{
	Rml::DataModelConstructor model = context.CreateDataModel(name);
	if (!model) return false;

	for (auto member = values.begin(); member != values.end(); ++member) model.BindCustomDataVariable(member.key(), JsonVariable(member.value()));
	return true;
}
