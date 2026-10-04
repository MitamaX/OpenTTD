/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file preview_scene.cpp One screen of the mini UI as a scene file describes it: the documents, their data and the screen around them. */

#include "preview_scene.h"

#include <algorithm>
#include <cstdio>
#include <fstream>

using Json = nlohmann::json;

static constexpr int HEX_BASE = 16;

static Rml::Vector2f ReadPoint(const Json &pair)
{
	return {pair.at(0).get<float>(), pair.at(1).get<float>()};
}

/* Colours are written the way the stylesheets write them, as #rrggbb. */
static Rml::Colourb ReadColour(const std::string &hex)
{
	unsigned long rgb = std::stoul(hex.substr(1), nullptr, HEX_BASE);
	return Rml::Colourb(static_cast<Rml::byte>(rgb >> 16), static_cast<Rml::byte>(rgb >> 8), static_cast<Rml::byte>(rgb), 255);
}

static SceneDocument ReadDocument(const Json &entry)
{
	SceneDocument document;
	document.path = entry.at("path").get<std::string>();
	document.model = entry.value("model", std::string());
	if (entry.contains("position")) document.position = ReadPoint(entry["position"]);
	return document;
}

static Scene ReadScene(const Json &root)
{
	Scene scene;
	if (root.contains("size")) {
		Rml::Vector2f size = ReadPoint(root["size"]);
		scene.size = Rml::Vector2i(static_cast<int>(size.x), static_cast<int>(size.y));
	}
	scene.dp = root.value("dp", scene.dp);
	if (root.contains("backdrop")) scene.backdrop = ReadColour(root["backdrop"].get<std::string>());
	if (root.contains("pointer")) scene.pointer = ReadPoint(root["pointer"]);
	for (const Json &entry : root.at("documents")) scene.documents.push_back(ReadDocument(entry));
	scene.models = root.value("models", Json::object());
	scene.styles = root.value("styles", Json::object());
	return scene;
}

std::optional<Scene> LoadScene(const std::filesystem::path &file)
{
	std::ifstream stream(file);
	Json root = Json::parse(stream, nullptr, false);
	if (root.is_discarded()) {
		std::fprintf(stderr, "%s: not valid JSON\n", file.string().c_str());
		return std::nullopt;
	}

	try {
		Scene scene = ReadScene(root);
		scene.file = file;
		return scene;
	} catch (const Json::exception &error) {
		std::fprintf(stderr, "%s: %s\n", file.string().c_str(), error.what());
		return std::nullopt;
	}
}

std::vector<std::filesystem::path> SceneFiles(const std::filesystem::path &directory)
{
	std::vector<std::filesystem::path> files;
	for (const auto &entry : std::filesystem::directory_iterator(directory)) {
		if (entry.path().extension() == ".json") files.push_back(entry.path());
	}
	std::ranges::sort(files);
	return files;
}
