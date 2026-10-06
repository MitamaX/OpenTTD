/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file luma_cell.cpp A grey texture cell holding luminance and alpha per texel, read as one period of a repeating pattern. */

#include "../../stdafx.h"
#include "luma_cell.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "../gpu/draw_list.h"
#include "material_atlas.h"
#include "pattern_noise.h"

#include "../../safeguards.h"

static constexpr int NORMALIZE_PASSES = 4;

LumaCell::LumaCell(int width, int height, float luma, float alpha) :
	width(width), height(height), luma(static_cast<size_t>(width) * height, luma), alpha(static_cast<size_t>(width) * height, alpha)
{
}

size_t LumaCell::Index(int x, int y) const
{
	return static_cast<size_t>(Wrapped(y, this->height)) * this->width + Wrapped(x, this->width);
}

void LumaCell::Set(int x, int y, float luma, float alpha)
{
	size_t index = this->Index(x, y);
	this->luma[index] = luma;
	this->alpha[index] = alpha;
}

/* Mixes premultiplied, so a clear texel lends no grey to the edge of whatever covers part of it. */
void LumaCell::Blend(int x, int y, float luma, float alpha, float share)
{
	size_t index = this->Index(x, y);
	float blended_alpha = std::lerp(this->alpha[index], alpha, share);
	float blended_light = std::lerp(this->luma[index] * this->alpha[index], luma * alpha, share);
	this->luma[index] = blended_alpha > CLEAR_ALPHA ? blended_light / blended_alpha : luma;
	this->alpha[index] = blended_alpha;
}

void LumaCell::Fill(const TexelRect &rect, float luma, float alpha)
{
	this->ForEachIn(rect, [&](size_t index) {
		this->luma[index] = luma;
		this->alpha[index] = alpha;
	});
}

void LumaCell::Scale(const TexelRect &rect, float factor)
{
	this->ForEachIn(rect, [&](size_t index) { this->luma[index] *= factor; });
}

/* Clear texels count for nothing, so a see-through pattern is judged by what shows. */
float LumaCell::MeanLuma(const TexelRect &rect) const
{
	double light = 0.0;
	double cover = 0.0;
	this->ForEachIn(rect, [&](size_t index) {
		light += this->luma[index] * this->alpha[index];
		cover += this->alpha[index];
	});
	return cover > 0.0 ? static_cast<float>(light / cover) : 0.0f;
}

/* Clipping at white pulls the mean back under the target, so the scaling repeats until it settles. */
void LumaCell::NormalizeMean(float target)
{
	for (int pass = 0; pass < NORMALIZE_PASSES; pass++) {
		float mean = this->MeanLuma();
		if (mean <= 0.0f) return;
		float factor = target / mean;
		for (float &luma : this->luma) luma = std::clamp(luma * factor, 0.0f, 1.0f);
	}
}

static uint Channel(float share)
{
	return FadedAlpha(share);
}

static uint32_t LumaTexel(float luma, float alpha)
{
	uint grey = Channel(luma);
	uint32_t texel = PackArgb(Channel(alpha), grey, grey, grey);
	assert(Red(texel) == Green(texel) && Green(texel) == Blue(texel));
	return texel;
}

void StoreCell(const LumaCell &cell, Point origin, GutterMode mode, std::span<uint32_t> atlas, int atlas_width)
{
	for (int y = -ATLAS_GUTTER; y < cell.Height() + ATLAS_GUTTER; y++) {
		int source_y = mode == GutterMode::Wrap ? y : std::clamp(y, 0, cell.Height() - 1);
		size_t row = static_cast<size_t>(origin.y + y) * atlas_width;
		for (int x = -ATLAS_GUTTER; x < cell.Width() + ATLAS_GUTTER; x++) {
			atlas[row + static_cast<size_t>(origin.x + x)] = LumaTexel(cell.Luma(x, source_y), cell.Alpha(x, source_y));
		}
	}
}
