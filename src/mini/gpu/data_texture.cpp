/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file data_texture.cpp Data laid out per texel for a shader to read. */

#include "../../stdafx.h"
#include "data_texture.h"

#include <algorithm>
#include <cstddef>

#include "frame_profile.h"
#include "gl_api.h"

#include "../../safeguards.h"

struct TexelLayout {
	GLint internal_format;
	GLenum pixel_format;
	size_t bytes;
	bool filtered;
	GLint wrap;
};

static TexelLayout LayoutOf(TexelFormat format)
{
	switch (format) {
		case TexelFormat::FilteredRgba: return {GL_RGBA8, GL_RGBA, 4, true, GL_CLAMP_TO_EDGE};
		case TexelFormat::FilteredRed: return {GL_R8, GL_RED, 1, true, GL_CLAMP_TO_EDGE};
		case TexelFormat::TiledRgba: return {GL_RGBA8, GL_RGBA, 4, true, GL_REPEAT};
		default: return {GL_RGBA8UI, GL_RGBA_INTEGER, 4, false, GL_CLAMP_TO_EDGE};
	}
}

static Dimension Halved(Dimension size)
{
	return {std::max(size.width / 2, 1U), std::max(size.height / 2, 1U)};
}

/* Each texel of the level below over the area blends the two by two texels above it, a pair along each row and then the two rows, rounding at each step as a driver's bilinear filter does. */
static void Halve(const std::byte *above, Dimension above_size, std::byte *below, Dimension below_size, const Rect &area, size_t bytes)
{
	auto blend = [](uint a, uint b) { return (a + b + 1) / 2; };
	for (int y = area.top; y <= area.bottom; y++) {
		const std::byte *row0 = above + static_cast<size_t>(2 * y) * above_size.width * bytes;
		const std::byte *row1 = above + static_cast<size_t>(std::min<uint>(2 * y + 1, above_size.height - 1)) * above_size.width * bytes;
		for (int x = area.left; x <= area.right; x++) {
			size_t column0 = static_cast<size_t>(2 * x) * bytes;
			size_t column1 = static_cast<size_t>(std::min<uint>(2 * x + 1, above_size.width - 1)) * bytes;
			for (size_t channel = 0; channel < bytes; channel++) {
				auto at = [&](const std::byte *row, size_t column) { return std::to_integer<uint>(row[column + channel]); };
				uint texel = blend(blend(at(row0, column0), at(row0, column1)), blend(at(row1, column0), at(row1, column1)));
				below[(static_cast<size_t>(y) * below_size.width + x) * bytes + channel] = static_cast<std::byte>(texel);
			}
		}
	}
}

/* Integer textures are incomplete unless they are sampled nearest and without mipmaps. */
void DataTexture::Allocate(TexelFormat format, Dimension size, const void *texels)
{
	this->Release();
	this->format = format;
	this->size = size;

	TexelLayout layout = LayoutOf(format);
	_frame_profile.Count("texture_allocs");
	_frame_profile.Count("texture_bytes", static_cast<size_t>(size.width) * size.height * layout.bytes);
	ResetPixelUnpack();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glGenTextures(1, &this->name);
	glBindTexture(GL_TEXTURE_2D, this->name);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, layout.filtered ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, layout.filtered ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, layout.wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, layout.wrap);
	if (layout.filtered) {
		FilterAnisotropically();
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	}
	glTexImage2D(GL_TEXTURE_2D, 0, layout.internal_format, static_cast<GLsizei>(size.width), static_cast<GLsizei>(size.height), 0, layout.pixel_format, GL_UNSIGNED_BYTE, texels);
	ResetPixelUnpack();
	if (!layout.filtered) return;

	for (Dimension level_size = size; level_size.width > 1 || level_size.height > 1;) {
		level_size = Halved(level_size);
		this->levels.emplace_back(static_cast<size_t>(level_size.width) * level_size.height * layout.bytes);
		glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(this->levels.size()), layout.internal_format, static_cast<GLsizei>(level_size.width), static_cast<GLsizei>(level_size.height), 0, layout.pixel_format, GL_UNSIGNED_BYTE, nullptr);
	}
	this->Refilter({0, 0, static_cast<int>(size.width) - 1, static_cast<int>(size.height) - 1}, static_cast<const std::byte *>(texels));
}

/* The texels are the whole map's, laid out row by row; only the area goes up, with the area of each smaller level it covers. */
void DataTexture::Update(const Rect &area, const void *texels)
{
	this->Upload(0, this->size, area, static_cast<const std::byte *>(texels));
	if (!this->levels.empty()) this->Refilter(area, static_cast<const std::byte *>(texels));
}

void DataTexture::Upload(int level, Dimension level_size, const Rect &area, const std::byte *texels) const
{
	TexelLayout layout = LayoutOf(this->format);
	size_t first = (static_cast<size_t>(area.top) * level_size.width + area.left) * layout.bytes;
	_frame_profile.Count("texture_bytes", static_cast<size_t>(area.Width()) * area.Height() * layout.bytes);
	glBindTexture(GL_TEXTURE_2D, this->name);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(level_size.width));
	glTexSubImage2D(GL_TEXTURE_2D, level, area.left, area.top, area.Width(), area.Height(), layout.pixel_format, GL_UNSIGNED_BYTE, texels + first);
	ResetPixelUnpack();
}

void DataTexture::Refilter(const Rect &area, const std::byte *texels)
{
	_frame_profile.Count("mipmap_builds");
	size_t bytes = LayoutOf(this->format).bytes;
	const std::byte *above = texels;
	Dimension above_size = this->size;
	Rect covered = area;
	for (size_t level = 0; level < this->levels.size(); level++) {
		Dimension level_size = Halved(above_size);
		covered = {covered.left / 2, covered.top / 2, covered.right / 2, covered.bottom / 2};
		Halve(above, above_size, this->levels[level].data(), level_size, covered, bytes);
		this->Upload(static_cast<int>(level + 1), level_size, covered, this->levels[level].data());
		above = this->levels[level].data();
		above_size = level_size;
	}
}

void DataTexture::Bind(uint unit) const
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, this->name);
}

void DataTexture::Release()
{
	if (this->name != 0) glDeleteTextures(1, &this->name);
	this->name = 0;
	this->levels.clear();
}

/* RmlUi binds its own textures on unit zero and expects that unit active. */
void DataTexture::Unbind(uint unit)
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
}
