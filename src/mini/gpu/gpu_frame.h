/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file gpu_frame.h How a mini UI frame reaches the screen through the game's OpenGL back-end. */

#ifndef MINI_GPU_GPU_FRAME_H
#define MINI_GPU_GPU_FRAME_H

#include <span>

#include "draw_list.h"
#include "draw_pass.h"
#include "screen_target.h"

/* A layer drawn over the map and under the native popups. It is detached
 * while the GL context still lives, so it can free what it holds. */
class ScreenLayer {
public:
	virtual ~ScreenLayer() = default;
	virtual void Render(Dimension screen) = 0;
	virtual void Detach() = 0;
};

/* The game's screen as a texture; its rows run bottom up, as a framebuffer's do. */
struct ScreenImage {
	uint32_t name = 0;
	Dimension size{};

	bool operator==(const ScreenImage &) const = default;
};

class GpuFrame {
public:
	bool Available() const { return this->available; }
	bool BeginPaint(Dimension screen, bool capture);
	void Compose(const DrawList &map, std::span<const Rect> floating);
	void Release();

	void Attach(ScreenLayer *layer) { this->layer = layer; }
	ScreenImage Screen() const;

private:
	void DrawFloating(std::span<const Rect> floating);

	ScreenTarget target;
	DrawPass pass;
	DrawList floating_list;
	ScreenLayer *layer = nullptr;
	bool available = false;
};

extern GpuFrame _gpu;

#endif /* MINI_GPU_GPU_FRAME_H */
