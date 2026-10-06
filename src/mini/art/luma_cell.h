/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file luma_cell.h A grey texture cell holding luminance and alpha per texel, read as one period of a repeating pattern. */

#ifndef MINI_ART_LUMA_CELL_H
#define MINI_ART_LUMA_CELL_H

#include <cstdint>
#include <span>
#include <vector>

#include "../../core/geometry_type.hpp"

inline constexpr float OPAQUE_ALPHA = 1.0f;
inline constexpr float CLEAR_ALPHA = 0.0f;
inline constexpr float TEXEL_CENTRE = 0.5f;

struct TexelRect {
	int x;
	int y;
	int width;
	int height;
};

/* A texel by column and row, with the centre of it as a share of the cell across and down. */
struct CellTexel {
	int x;
	int y;
	float u;
	float v;
};

enum class GutterMode : uint8_t {
	Wrap,
	WrapAcross,
};

class LumaCell {
public:
	LumaCell(int width, int height, float luma, float alpha = OPAQUE_ALPHA);

	int Width() const { return this->width; }
	int Height() const { return this->height; }
	TexelRect Bounds() const { return {0, 0, this->width, this->height}; }
	float Luma(int x, int y) const { return this->luma[this->Index(x, y)]; }
	float Alpha(int x, int y) const { return this->alpha[this->Index(x, y)]; }

	void Set(int x, int y, float luma, float alpha = OPAQUE_ALPHA);
	void Blend(int x, int y, float luma, float alpha, float share);
	void Fill(const TexelRect &rect, float luma, float alpha = OPAQUE_ALPHA);
	void Scale(const TexelRect &rect, float factor);
	float MeanLuma() const { return this->MeanLuma(this->Bounds()); }
	float MeanLuma(const TexelRect &rect) const;
	void NormalizeMean(float target);

	template <typename Visit>
	void ForEachTexel(Visit &&visit) const
	{
		for (int y = 0; y < this->height; y++) {
			for (int x = 0; x < this->width; x++) visit(CellTexel{x, y, (x + TEXEL_CENTRE) / this->width, (y + TEXEL_CENTRE) / this->height});
		}
	}

	template <typename Shade>
	void Paint(Shade &&shade)
	{
		this->ForEachTexel([&](const CellTexel &texel) { this->luma[this->Index(texel.x, texel.y)] = shade(texel); });
	}

private:
	size_t Index(int x, int y) const;

	template <typename Visit>
	void ForEachIn(const TexelRect &rect, Visit &&visit) const
	{
		for (int y = rect.y; y < rect.y + rect.height; y++) {
			for (int x = rect.x; x < rect.x + rect.width; x++) visit(this->Index(x, y));
		}
	}

	int width;
	int height;
	std::vector<float> luma;
	std::vector<float> alpha;
};

void StoreCell(const LumaCell &cell, Point origin, GutterMode mode, std::span<uint32_t> atlas, int atlas_width);

#endif /* MINI_ART_LUMA_CELL_H */
