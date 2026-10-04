/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file json_model.h A data model whose variables are read straight out of JSON, so a scene file stands in for the game. */

#ifndef MINI_PREVIEW_JSON_MODEL_H
#define MINI_PREVIEW_JSON_MODEL_H

#include <RmlUi/Core/Types.h>

#include "../../3rdparty/nlohmann/json.hpp"

namespace Rml { class Context; }

bool BindJsonModel(Rml::Context &context, const Rml::String &name, nlohmann::json &values);

#endif /* MINI_PREVIEW_JSON_MODEL_H */
