/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file gpu_frame.h How a mini UI frame reaches the screen through the game's OpenGL back-end. */

#ifndef MINI_GPU_GPU_FRAME_H
#define MINI_GPU_GPU_FRAME_H

#include "screen_target.h"

/* The layer every frame is drawn in. It is detached while the GL context
 * still lives, so it can free what it holds. */
class ScreenLayer {
public:
	virtual ~ScreenLayer() = default;
	virtual void Render(Dimension screen) = 0;
	virtual void Detach() = 0;
};

class GpuFrame {
public:
	bool Available() const { return this->available; }
	bool BeginPaint(Dimension screen, bool capture);
	void Compose();
	void Release();

	void Attach(ScreenLayer *layer) { this->layer = layer; }
	GlImage Screen() const;

private:
	ScreenTarget target;
	ScreenLayer *layer = nullptr;
	bool available = false;
};

extern GpuFrame _gpu;

#endif /* MINI_GPU_GPU_FRAME_H */
