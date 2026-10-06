/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file data_texture.cpp Data laid out per texel for a shader to read. */

#include "../../stdafx.h"
#include "data_texture.h"

#include <cstddef>

#include "gl_api.h"

#include "../../safeguards.h"

struct TexelLayout {
	GLint internal_format;
	GLenum pixel_format;
	size_t bytes;
	bool filtered;
};

static TexelLayout LayoutOf(TexelFormat format)
{
	switch (format) {
		case TexelFormat::FilteredRgba: return {GL_RGBA8, GL_RGBA, 4, true};
		default: return {GL_RGBA8UI, GL_RGBA_INTEGER, 4, false};
	}
}

/* Integer textures are incomplete unless they are sampled nearest and without mipmaps. */
void DataTexture::Allocate(TexelFormat format, Dimension size, const void *texels)
{
	this->Release();
	this->format = format;
	this->size = size;

	TexelLayout layout = LayoutOf(format);
	ResetPixelUnpack();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glGenTextures(1, &this->name);
	glBindTexture(GL_TEXTURE_2D, this->name);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, layout.filtered ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, layout.filtered ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	if (!layout.filtered) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, layout.internal_format, static_cast<GLsizei>(size.width), static_cast<GLsizei>(size.height), 0, layout.pixel_format, GL_UNSIGNED_BYTE, texels);
	ResetPixelUnpack();
	this->Refilter();
}

/* The texels are the whole map's, laid out row by row; only the area goes up. */
void DataTexture::Update(const Rect &area, const void *texels) const
{
	TexelLayout layout = LayoutOf(this->format);
	size_t first = (static_cast<size_t>(area.top) * this->size.width + area.left) * layout.bytes;
	glBindTexture(GL_TEXTURE_2D, this->name);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(this->size.width));
	glTexSubImage2D(GL_TEXTURE_2D, 0, area.left, area.top, area.Width(), area.Height(), layout.pixel_format, GL_UNSIGNED_BYTE, static_cast<const std::byte *>(texels) + first);
	ResetPixelUnpack();
}

void DataTexture::Refilter() const
{
	if (!LayoutOf(this->format).filtered) return;
	glBindTexture(GL_TEXTURE_2D, this->name);
	glGenerateMipmap(GL_TEXTURE_2D);
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
}

/* RmlUi binds its own textures on unit zero and expects that unit active. */
void DataTexture::Unbind(uint unit)
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
}
