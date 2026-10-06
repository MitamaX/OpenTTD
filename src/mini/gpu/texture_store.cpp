/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file texture_store.cpp Textures the mini UI draws with, kept as pixels so they outlive the GL context. */

#include "../../stdafx.h"
#include "texture_store.h"

#include <array>
#include <bit>

#include "draw_list.h"
#include "gl_api.h"

#include "../../safeguards.h"

using Texel = std::array<uint8_t, 4>;

static constexpr size_t ALPHA = 3;

TextureStore _textures;

static uint32_t Premultiplied(uint32_t pixel)
{
	Texel texel = std::bit_cast<Texel>(pixel);
	for (size_t c = 0; c < ALPHA; c++) texel[c] = static_cast<uint8_t>(texel[c] * texel[ALPHA] / CHANNEL_MAX);
	return std::bit_cast<uint32_t>(texel);
}

TextureId TextureStore::Add(std::span<const uint32_t> rgba, Dimension size, TextureFilter filter, TextureWrap wrap, int max_mip_level)
{
	TextureId id = this->next++;
	Entry &entry = this->entries.emplace(id, Entry{std::vector<uint32_t>(rgba.size()), size, filter, wrap, max_mip_level}).first->second;
	std::ranges::transform(rgba, entry.rgba.begin(), Premultiplied);
	return id;
}

/* A target has no pixels of its own; whatever renders into it fills it. */
TextureId TextureStore::AddTarget(Dimension size)
{
	return this->Add({}, size);
}

void TextureStore::Remove(TextureId id)
{
	auto it = this->entries.find(id);
	if (it == this->entries.end()) return;
	if (it->second.name != 0) glDeleteTextures(1, &it->second.name);
	this->entries.erase(it);
}

uint32_t TextureStore::Name(TextureId id)
{
	return this->Image(id).name;
}

/* Uploads on first use, so textures made between frames reach the GPU inside one. */
GlImage TextureStore::Image(TextureId id)
{
	auto it = this->entries.find(id);
	if (it == this->entries.end()) return {};
	if (it->second.name == 0) Upload(it->second);
	return {it->second.name, it->second.size};
}

/* The pixels stay, so every texture comes back on the next context. */
void TextureStore::Release()
{
	for (auto &[id, entry] : this->entries) {
		if (entry.name != 0) glDeleteTextures(1, &entry.name);
		entry.name = 0;
	}
}

void TextureStore::Upload(Entry &entry)
{
	bool mipmapped = entry.filter == TextureFilter::Mipmapped;
	GLint wrap = entry.wrap == TextureWrap::Repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;

	ResetPixelUnpack();
	glGenTextures(1, &entry.name);
	glBindTexture(GL_TEXTURE_2D, entry.name);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmapped ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mipmapped ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(entry.size.width), static_cast<GLsizei>(entry.size.height), 0, GL_RGBA, GL_UNSIGNED_BYTE, entry.rgba.empty() ? nullptr : entry.rgba.data());
	if (!mipmapped) return;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, entry.max_mip_level);
	glGenerateMipmap(GL_TEXTURE_2D);
}
