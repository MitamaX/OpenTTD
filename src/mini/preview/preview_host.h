/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file preview_host.h RmlUi set up the way the game sets it up, showing one scene at a time. */

#ifndef MINI_PREVIEW_PREVIEW_HOST_H
#define MINI_PREVIEW_PREVIEW_HOST_H

#include <filesystem>
#include <memory>

#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/SystemInterface.h>

#include "preview_scene.h"

namespace Rml { class Context; }
class RenderInterface_GL3;

class PreviewFiles final : public Rml::FileInterface {
public:
	explicit PreviewFiles(std::filesystem::path root) : root(std::move(root)) {}

	Rml::FileHandle Open(const Rml::String &path) override;
	void Close(Rml::FileHandle file) override;
	size_t Read(void *buffer, size_t size, Rml::FileHandle file) override;
	bool Seek(Rml::FileHandle file, long offset, int origin) override;
	size_t Tell(Rml::FileHandle file) override;

private:
	const std::filesystem::path root;
};

class PreviewLog final : public Rml::SystemInterface {
public:
	bool LogMessage(Rml::Log::Type type, const Rml::String &message) override;
};

class PreviewHost {
public:
	explicit PreviewHost(std::filesystem::path root);
	~PreviewHost();

	bool Start();
	void Show(Scene &scene);
	Rml::Context *Context() const { return this->context; }
	void Render(Rml::Vector2i size);

private:
	void Open(const SceneDocument &entry, const nlohmann::json &styles);

	PreviewFiles files;
	PreviewLog log;
	std::unique_ptr<RenderInterface_GL3> renderer;
	Rml::Context *context = nullptr;
};

#endif /* MINI_PREVIEW_PREVIEW_HOST_H */
