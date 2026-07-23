/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file raylib_wrap.cpp All raylib calls live here; raylib.h declares symbols that clash with windows.h, so it must never meet platform headers in one translation unit. */

#include "../stdafx.h"
#include "../gfx_type.h"
#include <raylib.h>
#include "raylib_wrap.h"

#include "../safeguards.h"

static Texture2D _rlw_tex;
static bool _rlw_tex_ok = false;
static int _rlw_tex_w, _rlw_tex_h;

bool RlwInit(int w, int h, const char *title)
{
	SetTraceLogLevel(LOG_WARNING);
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	InitWindow(w, h, title);
	if (!IsWindowReady()) return false;
	SetExitKey(KEY_NULL);
	HideCursor();
	return true;
}

void RlwClose()
{
	if (_rlw_tex_ok) {
		UnloadTexture(_rlw_tex);
		_rlw_tex_ok = false;
	}
	CloseWindow();
}

void RlwPoll(RlwInput &in)
{
	in.mouse_x = GetMouseX();
	in.mouse_y = GetMouseY();
	in.left_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
	in.right_down = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
	in.middle_down = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
	Vector2 wheel = GetMouseWheelMoveV();
	in.wheel_x = wheel.x;
	in.wheel_y = wheel.y;
	in.ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
	in.shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
	in.alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
	in.dir_left = IsKeyDown(KEY_LEFT);
	in.dir_up = IsKeyDown(KEY_UP);
	in.dir_right = IsKeyDown(KEY_RIGHT);
	in.dir_down = IsKeyDown(KEY_DOWN);
	in.tab = IsKeyDown(KEY_TAB);
	in.close_requested = WindowShouldClose();
	in.resized = IsWindowResized();
	in.width = GetScreenWidth();
	in.height = GetScreenHeight();
}

struct RlwKeyMapping {
	int rl_from;
	int rl_count;
	uint16_t map_to;
	bool unprintable;

	constexpr RlwKeyMapping(int rl_first, int rl_last, uint16_t map_first, bool unprintable) :
		rl_from(rl_first), rl_count(rl_last - rl_first + 1), map_to(map_first), unprintable(unprintable) {}
};

static constexpr RlwKeyMapping _rlw_key_map[] = {
	{KEY_PAGE_UP, KEY_PAGE_UP, WKC_PAGEUP, true},
	{KEY_PAGE_DOWN, KEY_PAGE_DOWN, WKC_PAGEDOWN, true},
	{KEY_UP, KEY_UP, WKC_UP, true},
	{KEY_DOWN, KEY_DOWN, WKC_DOWN, true},
	{KEY_LEFT, KEY_LEFT, WKC_LEFT, true},
	{KEY_RIGHT, KEY_RIGHT, WKC_RIGHT, true},
	{KEY_HOME, KEY_HOME, WKC_HOME, true},
	{KEY_END, KEY_END, WKC_END, true},
	{KEY_INSERT, KEY_INSERT, WKC_INSERT, true},
	{KEY_DELETE, KEY_DELETE, WKC_DELETE, true},
	{KEY_A, KEY_Z, 'A', false},
	{KEY_ZERO, KEY_NINE, '0', false},
	{KEY_ESCAPE, KEY_ESCAPE, WKC_ESC, true},
	{KEY_PAUSE, KEY_PAUSE, WKC_PAUSE, true},
	{KEY_BACKSPACE, KEY_BACKSPACE, WKC_BACKSPACE, true},
	{KEY_SPACE, KEY_SPACE, WKC_SPACE, false},
	{KEY_ENTER, KEY_ENTER, WKC_RETURN, false},
	{KEY_TAB, KEY_TAB, WKC_TAB, false},
	{KEY_F1, KEY_F12, WKC_F1, true},
	{KEY_KP_0, KEY_KP_9, '0', false},
	{KEY_KP_DECIMAL, KEY_KP_DECIMAL, WKC_NUM_DECIMAL, false},
	{KEY_KP_DIVIDE, KEY_KP_DIVIDE, WKC_NUM_DIV, false},
	{KEY_KP_MULTIPLY, KEY_KP_MULTIPLY, WKC_NUM_MUL, false},
	{KEY_KP_SUBTRACT, KEY_KP_SUBTRACT, WKC_NUM_MINUS, false},
	{KEY_KP_ADD, KEY_KP_ADD, WKC_NUM_PLUS, false},
	{KEY_KP_ENTER, KEY_KP_ENTER, WKC_NUM_ENTER, false},
	{KEY_SLASH, KEY_SLASH, WKC_SLASH, false},
	{KEY_SEMICOLON, KEY_SEMICOLON, WKC_SEMICOLON, false},
	{KEY_EQUAL, KEY_EQUAL, WKC_EQUALS, false},
	{KEY_LEFT_BRACKET, KEY_LEFT_BRACKET, WKC_L_BRACKET, false},
	{KEY_BACKSLASH, KEY_BACKSLASH, WKC_BACKSLASH, false},
	{KEY_RIGHT_BRACKET, KEY_RIGHT_BRACKET, WKC_R_BRACKET, false},
	{KEY_APOSTROPHE, KEY_APOSTROPHE, WKC_SINGLEQUOTE, false},
	{KEY_COMMA, KEY_COMMA, WKC_COMMA, false},
	{KEY_MINUS, KEY_MINUS, WKC_MINUS, false},
	{KEY_PERIOD, KEY_PERIOD, WKC_PERIOD, false},
	{KEY_GRAVE, KEY_GRAVE, WKC_BACKQUOTE, false},
};

uint32_t RlwNextKey(char32_t &character)
{
	int rk = GetKeyPressed();
	character = WKC_NONE;
	if (rk == KEY_NULL) return 0;

	uint32_t key = 0;
	bool unprintable = true;
	for (const RlwKeyMapping &map : _rlw_key_map) {
		if (rk >= map.rl_from && rk < map.rl_from + map.rl_count) {
			key = map.map_to + rk - map.rl_from;
			unprintable = map.unprintable;
			break;
		}
	}
	if (key == 0) return RlwNextKey(character);

	bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
	bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
	bool meta = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
	if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) key |= WKC_SHIFT;
	if (ctrl) key |= WKC_CTRL;
	if (alt) key |= WKC_ALT;
	if (meta) key |= WKC_META;

	if (!unprintable && !ctrl && !alt && !meta) {
		/* raylib letter and punctuation key values are their ASCII codes. */
		character = (rk >= KEY_A && rk <= KEY_Z) ? rk + 32 : (rk >= KEY_KP_0 ? key & 0xFF : rk);
	}
	return key;
}

char32_t RlwNextChar()
{
	return (char32_t)GetCharPressed();
}

void RlwPresent(const uint32_t *rgba, int w, int h)
{
	if (!_rlw_tex_ok || _rlw_tex_w != w || _rlw_tex_h != h) {
		if (_rlw_tex_ok) UnloadTexture(_rlw_tex);
		Image img;
		img.data = const_cast<uint32_t *>(rgba);
		img.width = w;
		img.height = h;
		img.mipmaps = 1;
		img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
		_rlw_tex = LoadTextureFromImage(img);
		_rlw_tex_ok = true;
		_rlw_tex_w = w;
		_rlw_tex_h = h;
	} else {
		UpdateTexture(_rlw_tex, rgba);
	}

	BeginDrawing();
	ClearBackground(BLACK);
	DrawTexture(_rlw_tex, 0, 0, WHITE);
	EndDrawing();
}

void RlwSetSize(int w, int h)
{
	SetWindowSize(w, h);
}

void RlwWarpMouse(int x, int y)
{
	SetMousePosition(x, y);
}

void RlwToggleBorderless()
{
	ToggleBorderlessWindowed();
}

bool RlwMonitorSize(int &w, int &h)
{
	if (!IsWindowReady()) return false;
	int m = GetCurrentMonitor();
	w = GetMonitorWidth(m);
	h = GetMonitorHeight(m);
	return w > 0 && h > 0;
}
