/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file raylib_wrap.h Interface to the translation unit that isolates raylib.h from OpenTTD headers. */

#ifndef VIDEO_RAYLIB_WRAP_H
#define VIDEO_RAYLIB_WRAP_H

#include <cstddef>
#include <cstdint>

struct RlwInput {
	int mouse_x = 0;
	int mouse_y = 0;
	bool left_down = false;
	bool right_down = false;
	bool middle_down = false;
	float wheel_x = 0.0f;
	float wheel_y = 0.0f;
	bool ctrl = false;
	bool shift = false;
	bool alt = false;
	bool dir_left = false;
	bool dir_up = false;
	bool dir_right = false;
	bool dir_down = false;
	bool key_w = false;
	bool key_a = false;
	bool key_s = false;
	bool key_d = false;
	bool tab = false;
	bool close_requested = false;
	bool resized = false;
	int width = 0;
	int height = 0;
};

struct RlwRectI {
	int x, y, w, h;
	/* Overlays normally draw above the ImGui layer; carrier viewports draw
	 * below it and reach the screen through an ImGui image instead. */
	bool under = false;
};

bool RlwInit(int w, int h, const char *title, bool vsync);
void RlwClose();
void RlwPoll(RlwInput &in);
uint32_t RlwNextKey(char32_t &character);
char32_t RlwNextChar();
void RlwPresent(const uint32_t *rgba, int w, int h);
void RlwSetSize(int w, int h);
void RlwSetVsync(bool on);
void RlwWarpMouse(int x, int y);
void RlwToggleBorderless();
bool RlwMonitorSize(int &w, int &h);

/* 2D command buffer: recorded during the game tick, replayed inside the
 * frame that RlwPresentMini draws. Colours are 0xAARRGGBB. */
void RlwCmdClear();
void RlwCmdRect(int x0, int y0, int x1, int y1, uint32_t argb);
void RlwCmdRoundRect(int x0, int y0, int x1, int y1, int radius, uint32_t argb);
void RlwCmdGradientRect(int x0, int y0, int x1, int y1, uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br);
void RlwCmdLine(int x0, int y0, int x1, int y1, int width, uint32_t argb);
void RlwCmdCircle(int cx, int cy, int r, uint32_t argb);
void RlwCmdDiamond(int cx, int cy, int r, uint32_t argb);
void RlwCmdTriangle(int cx, int cy, int r, uint32_t argb);
void RlwCmdTexQuad(int tex, int x, int y, uint32_t tint_argb);
void RlwCmdSprite(int tex, int sx, int sy, int sw, int sh, int dx0, int dy0, int dx1, int dy1, int angle_deg, uint32_t tint_argb);
int RlwCreateTexture(const uint32_t *rgba, int w, int h);
int RlwCreateAtlasTexture(const uint32_t *rgba, int w, int h);
int RlwCreateTileTexture(const uint32_t *rgba, int w, int h);
bool RlwLoadImageInto(const char *path, uint32_t *rgba, int w, int h);
void RlwFreeTexture(int tex);
void RlwPresentMini(const uint32_t *argb, int pitch, int w, int h, const RlwRectI *overlays, size_t count);

/* Dear ImGui rides on top of the command buffer: a frame opened during the
 * game tick is composited by RlwPresentMini, or dropped if the native screen
 * presents instead. */
void RlwImGuiInit();
void RlwImGuiShutdown();
void RlwImGuiNewFrame();

/* GL handle of the CPU screen texture, for sampling native-window regions
 * inside ImGui images. 0 while the texture does not exist yet. */
uintptr_t RlwScreenTexId();

#endif /* VIDEO_RAYLIB_WRAP_H */
