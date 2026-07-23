/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file raylib_wrap.h Interface to the translation unit that isolates raylib.h from OpenTTD headers. */

#ifndef VIDEO_RAYLIB_WRAP_H
#define VIDEO_RAYLIB_WRAP_H

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
	bool tab = false;
	bool close_requested = false;
	bool resized = false;
	int width = 0;
	int height = 0;
};

bool RlwInit(int w, int h, const char *title);
void RlwClose();
void RlwPoll(RlwInput &in);
uint32_t RlwNextKey(char32_t &character);
char32_t RlwNextChar();
void RlwPresent(const uint32_t *rgba, int w, int h);
void RlwSetSize(int w, int h);
void RlwWarpMouse(int x, int y);
void RlwToggleBorderless();
bool RlwMonitorSize(int &w, int &h);

#endif /* VIDEO_RAYLIB_WRAP_H */
