/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file model_tag.h Naming a document's data model in its body tag. */

#ifndef MINI_UI_MODEL_TAG_H
#define MINI_UI_MODEL_TAG_H

#include <string_view>

#include <RmlUi/Core/Types.h>

/* RmlUi binds a document to its data model while parsing, so the model name has to be in the body tag beforehand. */
inline Rml::String BindBodyToModel(Rml::String rml, const Rml::String &model_name)
{
	static constexpr std::string_view BODY_TAG = "<body";

	size_t body = rml.find(BODY_TAG);
	if (body != Rml::String::npos) rml.insert(body + BODY_TAG.size(), " data-model=\"" + model_name + "\"");
	return rml;
}

#endif /* MINI_UI_MODEL_TAG_H */
