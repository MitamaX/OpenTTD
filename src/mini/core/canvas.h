/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file canvas.h Shapes and text the mini UI records into the raylib command buffer. */

#ifndef MINI_CORE_CANVAS_H
#define MINI_CORE_CANVAS_H

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "../../gfx_type.h"
#include "../../mini_atlas.h"

struct CanvasText {
	int tex;
	int w;
	int h;
	int pad;
	uint64_t last_use;
};

constexpr uint32_t TextTint(TextColour colour)
{
	return colour == TC_BLACK ? 0xFF14181CU : 0xFFE6E1D3U;
}

class Canvas {
public:
	void BeginFrame();
	void SetGrey(bool grey) { this->grey = grey; }
	uint32_t Greyed(uint32_t c) const;
	uint32_t Tone(uint32_t c) const;

	void FillRect(int x0, int y0, int x1, int y1, uint32_t c);
	void BlendRect(int x0, int y0, int x1, int y1, uint32_t c, uint alpha);
	void ThickLine(int x0, int y0, int x1, int y1, int width, uint32_t c);
	void FillCircle(int cx, int cy, int r, uint32_t c);
	void FillDiamond(int cx, int cy, int r, uint32_t c);
	void FillTriangle(int cx, int cy, int r, uint32_t c);
	void FillShapeRot(MiniSprite s, int cx, int cy, int r, int angle, uint32_t c);

	const CanvasText *Text(std::string_view text);
	void DrawText(const CanvasText &text, int x, int y, uint32_t tint);
	void DrawText(std::string_view text, int x, int y, uint32_t tint);

private:
	std::optional<CanvasText> Render(std::string_view text) const;
	void PruneText();

	std::unordered_map<std::string, CanvasText> texts;
	uint64_t frame = 0;
	bool grey = false;
};

extern Canvas _canvas;

#endif /* MINI_CORE_CANVAS_H */
