/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file preview_scene.h One screen of the mini UI as a scene file describes it: the documents, their data and the screen around them. */

#ifndef MINI_PREVIEW_PREVIEW_SCENE_H
#define MINI_PREVIEW_PREVIEW_SCENE_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <RmlUi/Core/Types.h>

#include "../../3rdparty/nlohmann/json.hpp"

struct SceneDocument {
	std::string path;
	std::string model;
	std::optional<Rml::Vector2f> position;
};

struct Scene {
	std::filesystem::path file;
	Rml::Vector2i size{1920, 1080};
	float dp = 2.0f;
	Rml::Colourb backdrop{110, 150, 90, 255};
	std::optional<Rml::Vector2f> pointer;
	std::vector<SceneDocument> documents;
	nlohmann::json models;
	nlohmann::json styles;
};

std::optional<Scene> LoadScene(const std::filesystem::path &file);
std::vector<std::filesystem::path> SceneFiles(const std::filesystem::path &directory);

#endif /* MINI_PREVIEW_PREVIEW_SCENE_H */
