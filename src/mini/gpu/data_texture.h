/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file data_texture.h Data laid out per texel for a shader to read. */

#ifndef MINI_GPU_DATA_TEXTURE_H
#define MINI_GPU_DATA_TEXTURE_H

#include "../../core/geometry_type.hpp"

enum class TexelFormat : uint8_t {
	ExactRgba,
	FilteredRgba,
};

class DataTexture {
public:
	void Allocate(TexelFormat format, Dimension size, const void *texels);
	void Update(const Rect &area, const void *texels) const;
	void Refilter() const;
	void Bind(uint unit) const;
	void Release();

	static void Unbind(uint unit);

	bool Allocated() const { return this->name != 0; }

private:
	uint32_t name = 0;
	TexelFormat format = TexelFormat::ExactRgba;
	Dimension size{};
};

#endif /* MINI_GPU_DATA_TEXTURE_H */
