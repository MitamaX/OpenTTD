/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file texture_store.h Textures the mini UI draws with, kept as pixels so they outlive the GL context. */

#ifndef MINI_GPU_TEXTURE_STORE_H
#define MINI_GPU_TEXTURE_STORE_H

#include <span>
#include <unordered_map>
#include <vector>

#include "../../core/geometry_type.hpp"

using TextureId = uint32_t;
static constexpr TextureId NO_TEXTURE = 0;
inline constexpr int UNLIMITED_MIP_LEVEL = 1000;

enum class TextureFilter : uint8_t {
	Nearest,
	Mipmapped,
};

enum class TextureWrap : uint8_t {
	Clamp,
	Repeat,
};

/* A texture as the GL context holds it. */
struct GlImage {
	uint32_t name = 0;
	Dimension size{};

	bool operator==(const GlImage &) const = default;
};

/* Pixels come in as RGBA bytes in memory order and are kept premultiplied, the way RmlUi blends them. */
class TextureStore {
public:
	TextureId Add(std::span<const uint32_t> rgba, Dimension size, TextureFilter filter = TextureFilter::Nearest, TextureWrap wrap = TextureWrap::Clamp, int max_mip_level = UNLIMITED_MIP_LEVEL);
	TextureId AddTarget(Dimension size);
	void Remove(TextureId id);

	uint32_t Name(TextureId id);
	GlImage Image(TextureId id);

	void Release();

private:
	struct Entry {
		std::vector<uint32_t> rgba;
		Dimension size;
		TextureFilter filter;
		TextureWrap wrap;
		int max_mip_level;
		uint32_t name = 0;
	};

	static void Upload(Entry &entry);

	std::unordered_map<TextureId, Entry> entries;
	TextureId next = NO_TEXTURE + 1;
};

extern TextureStore _textures;

#endif /* MINI_GPU_TEXTURE_STORE_H */
