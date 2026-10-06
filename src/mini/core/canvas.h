/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file canvas.h Shapes and text the mini UI records into the map draw list. */

#ifndef MINI_CORE_CANVAS_H
#define MINI_CORE_CANVAS_H

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

#include "../../core/geometry_type.hpp"
#include "../../gfx_type.h"
#include "../../mini_atlas.h"
#include "../gpu/draw_list.h"
#include "camera.h"

struct CanvasText {
	TextureId tex;
	int w;
	int h;
	int pad;
	uint64_t last_use;
};

constexpr uint32_t TextTint(TextColour colour)
{
	return colour == TC_BLACK ? 0xFF14181CU : 0xFFE6E1D3U;
}

inline constexpr uint LUMA_RED = 77;
inline constexpr uint LUMA_GREEN = 151;
inline constexpr uint LUMA_BLUE = 28;
inline constexpr uint LUMA_SHIFT = 8;
inline constexpr uint LIGHT_LUMINANCE = 140;
inline constexpr uint GREY_FLOOR = 12;

constexpr uint Luminance(uint32_t c)
{
	return (LUMA_RED * Red(c) + LUMA_GREEN * Green(c) + LUMA_BLUE * Blue(c)) >> LUMA_SHIFT;
}

constexpr bool IsLightTone(uint32_t c)
{
	return Luminance(c) >= LIGHT_LUMINANCE;
}

ScreenPoint ScreenPointOf(const WorldPoint &point);
std::array<ScreenPoint, 4> ScreenQuadOf(const std::array<WorldPoint, 4> &corners);
bool OnScreen(std::span<const ScreenPoint> outline);
float Winding(std::span<const ScreenPoint> outline);

class Canvas {
public:
	static constexpr uint OPAQUE_ALPHA = CHANNEL_MAX;

	void BeginFrame();

	void FillRect(int x0, int y0, int x1, int y1, uint32_t c);
	void BlendRect(int x0, int y0, int x1, int y1, uint32_t c, uint alpha);
	void Frame(const Rect &r, int width, uint32_t c, uint alpha = OPAQUE_ALPHA);
	void FillWorldQuad(const std::array<WorldPoint, 4> &corners, uint32_t c, uint alpha = OPAQUE_ALPHA);
	void FrameWorldQuad(const std::array<WorldPoint, 4> &corners, int width, uint32_t c, uint alpha = OPAQUE_ALPHA) { this->FrameWorldRing(corners, width, c, alpha); }
	void FrameWorldRing(std::span<const WorldPoint> ring, int width, uint32_t c, uint alpha = OPAQUE_ALPHA);
	void ThickLine(int x0, int y0, int x1, int y1, int width, uint32_t c);
	void FillCircle(int cx, int cy, int r, uint32_t c);
	void FillDiamond(int cx, int cy, int r, uint32_t c);
	void FillTriangle(int cx, int cy, int r, uint32_t c);

	const CanvasText *Text(std::string_view text);
	void DrawText(const CanvasText &text, int x, int y, uint32_t tint);
	void DrawText(std::string_view text, int x, int y, uint32_t tint);

private:
	uint32_t Blended(uint32_t c, uint alpha) const;
	std::optional<CanvasText> Render(std::string_view text) const;
	void PruneText();

	std::unordered_map<std::string, CanvasText> texts;
	uint64_t frame = 0;
};

extern Canvas _canvas;
extern DrawList _map_draw;

#endif /* MINI_CORE_CANVAS_H */
