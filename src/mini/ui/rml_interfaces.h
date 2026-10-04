/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rml_interfaces.h The services RmlUi asks of its host: file access, system calls and text input focus. */

#ifndef MINI_UI_RML_INTERFACES_H
#define MINI_UI_RML_INTERFACES_H

#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/TextInputHandler.h>

#include <chrono>
#include <unordered_map>

#include "../../fileio_type.h"

class RmlFileInterface final : public Rml::FileInterface {
public:
	Rml::FileHandle Open(const Rml::String &path) override;
	void Close(Rml::FileHandle file) override;
	size_t Read(void *buffer, size_t size, Rml::FileHandle file) override;
	bool Seek(Rml::FileHandle file, long offset, int origin) override;
	size_t Tell(Rml::FileHandle file) override;

private:
	FILE *Stream(Rml::FileHandle file);

	std::unordered_map<Rml::FileHandle, ::FileHandle> files;
	Rml::FileHandle next_handle = 1;
};

class RmlSystemInterface final : public Rml::SystemInterface {
public:
	double GetElapsedTime() override;
	bool LogMessage(Rml::Log::Type type, const Rml::String &message) override;
	void SetClipboardText(const Rml::String &text) override;
	void GetClipboardText(Rml::String &text) override;

private:
	const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
};

class RmlTextInputHandler final : public Rml::TextInputHandler {
public:
	bool IsActive() const { return this->active != nullptr; }

	void OnActivate(Rml::TextInputContext *input_context) override;
	void OnDeactivate(Rml::TextInputContext *input_context) override;
	void OnDestroy(Rml::TextInputContext *input_context) override;

private:
	void Release(const Rml::TextInputContext *input_context);

	const Rml::TextInputContext *active = nullptr;
};

#endif /* MINI_UI_RML_INTERFACES_H */
