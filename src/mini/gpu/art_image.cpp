/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file art_image.cpp Picture files from the player's art folder, decoded to RGBA bytes at the size a slot asks for. */

#include "../../stdafx.h"
#include "art_image.h"

#include <array>
#include <bit>
#include <optional>
#include <vector>

#include "../../fileio_type.h"

#ifdef WITH_PNG
#include <png.h>
#endif /* WITH_PNG */

#include "../../safeguards.h"

using Texel = std::array<uint8_t, 4>;

static constexpr size_t ALPHA = 3;
static constexpr size_t COLOUR_CHANNELS = 3;

struct Picture {
	std::vector<uint32_t> rgba;
	int width;
	int height;

	Texel At(int x, int y) const { return std::bit_cast<Texel>(this->rgba[static_cast<size_t>(y) * this->width + x]); }
};

#ifdef WITH_PNG
static std::optional<Picture> DecodePng(const std::string &path)
{
	std::optional<FileHandle> file = FileHandle::Open(path, "rb");
	if (!file.has_value()) return std::nullopt;

	png_image image{};
	image.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_stdio(&image, static_cast<FILE *>(*file))) return std::nullopt;

	image.format = PNG_FORMAT_RGBA;
	Picture picture{std::vector<uint32_t>(static_cast<size_t>(image.width) * image.height), static_cast<int>(image.width), static_cast<int>(image.height)};
	bool decoded = png_image_finish_read(&image, nullptr, picture.rgba.data(), 0, nullptr) != 0;
	png_image_free(&image);
	if (!decoded) return std::nullopt;
	return picture;
}
#else
static std::optional<Picture> DecodePng(const std::string &)
{
	return std::nullopt;
}
#endif /* WITH_PNG */

/* Each target texel averages the source texels its area covers, weighted by
 * their alpha so transparent neighbours do not darken the edges. A fully
 * transparent area keeps its plain colour average for the filtering around
 * it. Enlarging covers a single texel, which is nearest sampling. */
static Texel Average(const Picture &from, int x0, int y0, int x1, int y1)
{
	std::array<uint32_t, COLOUR_CHANNELS> plain{};
	std::array<uint32_t, COLOUR_CHANNELS> weighted{};
	uint32_t alpha = 0;
	uint32_t count = 0;
	for (int y = y0; y < y1; y++) {
		for (int x = x0; x < x1; x++) {
			Texel texel = from.At(x, y);
			for (size_t c = 0; c < COLOUR_CHANNELS; c++) {
				plain[c] += texel[c];
				weighted[c] += texel[c] * texel[ALPHA];
			}
			alpha += texel[ALPHA];
			count++;
		}
	}

	Texel result{};
	for (size_t c = 0; c < COLOUR_CHANNELS; c++) result[c] = static_cast<uint8_t>(alpha == 0 ? plain[c] / count : weighted[c] / alpha);
	result[ALPHA] = static_cast<uint8_t>(alpha / count);
	return result;
}

static void Resample(const Picture &from, std::span<uint32_t> to, int width, int height)
{
	for (int y = 0; y < height; y++) {
		int y0 = y * from.height / height;
		int y1 = std::max(y0 + 1, (y + 1) * from.height / height);
		for (int x = 0; x < width; x++) {
			int x0 = x * from.width / width;
			int x1 = std::max(x0 + 1, (x + 1) * from.width / width);
			to[static_cast<size_t>(y) * width + x] = std::bit_cast<uint32_t>(Average(from, x0, y0, x1, y1));
		}
	}
}

bool LoadArtImage(const std::string &path, std::span<uint32_t> rgba, int width, int height)
{
	std::optional<Picture> picture = DecodePng(path);
	if (!picture.has_value() || picture->width <= 0 || picture->height <= 0) return false;
	Resample(*picture, rgba, width, height);
	return true;
}
