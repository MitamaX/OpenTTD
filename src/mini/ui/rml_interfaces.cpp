/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rml_interfaces.cpp The services RmlUi asks of its host: file access, system calls and text input focus. */

#include "../../stdafx.h"
#include "rml_interfaces.h"

#include <RmlUi/Core/StringUtilities.h>
#include <RmlUi/Core/TextInputContext.h>

#include "../../debug.h"
#include "../../fileio_func.h"
#include "../../video/video_driver.hpp"
#include "../../window_func.h"

#include "../../safeguards.h"

std::optional<std::string> GetClipboardContents();

static Rml::StringView RmlView(std::string_view text)
{
	return Rml::StringView(text.data(), text.data() + text.size());
}

Rml::FileHandle RmlFileInterface::Open(const Rml::String &path)
{
	std::optional<::FileHandle> file = FioFOpenFile(path, "rb", BASE_DIR);
	if (!file.has_value()) file = FioFOpenFile(path, "rb", NO_DIRECTORY);
	if (!file.has_value()) return 0;

	Rml::FileHandle handle = this->next_handle++;
	this->files.emplace(handle, std::move(*file));
	return handle;
}

void RmlFileInterface::Close(Rml::FileHandle file)
{
	this->files.erase(file);
}

size_t RmlFileInterface::Read(void *buffer, size_t size, Rml::FileHandle file)
{
	return fread(buffer, 1, size, this->Stream(file));
}

bool RmlFileInterface::Seek(Rml::FileHandle file, long offset, int origin)
{
	return fseek(this->Stream(file), offset, origin) == 0;
}

size_t RmlFileInterface::Tell(Rml::FileHandle file)
{
	return static_cast<size_t>(ftell(this->Stream(file)));
}

FILE *RmlFileInterface::Stream(Rml::FileHandle file)
{
	return this->files.at(file);
}

double RmlSystemInterface::GetElapsedTime()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now() - this->start).count();
}

bool RmlSystemInterface::LogMessage(Rml::Log::Type type, const Rml::String &message)
{
	Debug(misc, type <= Rml::Log::LT_WARNING ? 0 : 3, "[rmlui] {}", message);
	return true;
}

/* The game only reads the system clipboard, so pasting reaches it and copying stays inside RmlUi. */
void RmlSystemInterface::GetClipboardText(Rml::String &text)
{
	text = GetClipboardContents().value_or(Rml::String());
}

/* An input method's unfinished text sits in the field as a marked range that each update replaces. */
void RmlTextInputHandler::Compose(std::string_view text)
{
	if (this->active == nullptr) return;
	if (!this->composing) this->active->GetSelectionRange(this->composition_start, this->composition_end);

	this->active->SetText(RmlView(text), this->composition_start, this->composition_end);
	this->composition_end = this->composition_start + static_cast<int>(Rml::StringUtilities::LengthUTF8(RmlView(text)));
	this->composing = this->composition_end > this->composition_start;
	this->active->SetCompositionRange(this->composition_start, this->composition_end);
	this->active->SetCursorPosition(this->composition_end);
}

/* The driver only delivers composed and translated text while it believes an edit box has focus. */
void RmlTextInputHandler::OnActivate(Rml::TextInputContext *input_context)
{
	this->active = input_context;
	this->composing = false;
	VideoDriver::GetInstance()->EditBoxGainedFocus();
}

void RmlTextInputHandler::OnDeactivate(Rml::TextInputContext *input_context)
{
	this->Release(input_context);
}

void RmlTextInputHandler::OnDestroy(Rml::TextInputContext *input_context)
{
	this->Release(input_context);
}

void RmlTextInputHandler::Release(const Rml::TextInputContext *input_context)
{
	if (this->active != input_context) return;
	this->active = nullptr;
	this->composing = false;
	if (!EditBoxInGlobalFocus()) VideoDriver::GetInstance()->EditBoxLostFocus();
}
