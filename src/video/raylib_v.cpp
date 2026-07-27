/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file raylib_v.cpp Implementation of the raylib video driver. */

#include "../stdafx.h"
#include "../openttd.h"
#include "../gfx_func.h"
#include "../blitter/factory.hpp"
#include "../core/geometry_func.hpp"
#include "../core/math_func.hpp"
#include "../core/utf8.hpp"
#include "../framerate_type.h"
#include "../mini_atlas.h"
#include "../mini_ui.h"
#include "../progress.h"
#include "../string_func.h"
#include "../window_func.h"
#include "raylib_wrap.h"
#include "raylib_v.h"

#include "../safeguards.h"

static FVideoDriver_Raylib iFVideoDriver_Raylib;

static const Dimension _raylib_default_resolutions[] = {
	{ 1280,  720 },
	{ 1366,  768 },
	{ 1600,  900 },
	{ 1920, 1080 },
	{ 2560, 1440 },
	{ 3840, 2160 },
};

std::optional<std::string_view> VideoDriver_Raylib::Start(const StringList &param)
{
	if (BlitterFactory::GetCurrentBlitter()->GetScreenDepth() != 32) {
		if (BlitterFactory::SelectBlitter("32bpp-anim") == nullptr) return "Failed to select a 32bpp blitter";
	}

	this->UpdateAutoResolution();

	std::string caption = VideoDriver::GetCaption();
	if (!RlwInit(_cur_resolution.width, _cur_resolution.height, caption.c_str())) return "Failed to create raylib window";

	_resolutions.assign(std::begin(_raylib_default_resolutions), std::end(_raylib_default_resolutions));
	SortResolutions();

	this->ClientSizeChanged(_cur_resolution.width, _cur_resolution.height, true);

	MarkWholeScreenDirty();
	_cursor.in_window = true;

	this->is_game_threaded = !GetDriverParamBool(param, "no_threads") && !GetDriverParamBool(param, "no_thread");

	return std::nullopt;
}

void VideoDriver_Raylib::Stop()
{
	MiniAtlasReset();
	RlwClose();
}

void VideoDriver_Raylib::MakeDirty(int left, int top, int width, int height)
{
	Rect r = {left, top, left + width, top + height};
	this->dirty_rect = BoundingRect(this->dirty_rect, r);
}

void VideoDriver_Raylib::MainLoop()
{
	this->StartGameThread();

	while (!_exit_game) {
		this->Tick();
		this->SleepTillNextTick();
	}

	this->StopGameThread();
}

bool VideoDriver_Raylib::ChangeResolution(int w, int h)
{
	RlwSetSize(w, h);
	this->ClientSizeChanged(w, h, false);
	return true;
}

bool VideoDriver_Raylib::ToggleFullscreen(bool fullscreen)
{
	RlwToggleBorderless();
	_fullscreen = fullscreen;
	InvalidateWindowClassesData(WC_GAME_OPTIONS, 3);
	return true;
}

bool VideoDriver_Raylib::AfterBlitterChange()
{
	if (BlitterFactory::GetCurrentBlitter()->GetScreenDepth() != 32) return false;
	this->ClientSizeChanged(_screen.width, _screen.height, true);
	return true;
}

Dimension VideoDriver_Raylib::GetScreenSize() const
{
	int w, h;
	if (!RlwMonitorSize(w, h)) return VideoDriver::GetScreenSize();
	return { static_cast<uint>(w), static_cast<uint>(h) };
}

void VideoDriver_Raylib::ClientSizeChanged(int w, int h, bool force)
{
	if (!force && w == _screen.width && h == _screen.height) return;

	this->vid_buf.assign(static_cast<size_t>(w) * h, 0);
	this->present_buf.resize(static_cast<size_t>(w) * h);

	_screen.width = w;
	_screen.height = h;
	_screen.pitch = w;
	_screen.dst_ptr = this->vid_buf.data();

	this->dirty_rect = {};

	CopyPalette(this->local_palette, true);
	BlitterFactory::GetCurrentBlitter()->PostResize();
	GameSizeChanged();
}

static bool IsEditBoxControlKey(uint32_t keycode)
{
	switch (keycode) {
		case WKC_DELETE:
		case WKC_NUM_ENTER:
		case WKC_LEFT:
		case WKC_RIGHT:
		case WKC_UP:
		case WKC_DOWN:
		case WKC_HOME:
		case WKC_END:
			return true;
		default:
			return (keycode & (WKC_META | WKC_CTRL | WKC_ALT)) != 0 || (keycode >= WKC_F1 && keycode <= WKC_F12);
	}
}

void VideoDriver_Raylib::InputLoop()
{
	RlwInput in;
	RlwPoll(in);

	if (in.close_requested && !this->exit_requested) {
		this->exit_requested = true;
		HandleExitGameRequest();
	}

	if (in.resized) this->ClientSizeChanged(std::max(in.width, 64), std::max(in.height, 64), false);

	bool old_ctrl_pressed = _ctrl_pressed;
	_ctrl_pressed = in.ctrl;
	_shift_pressed = in.shift;
	this->fast_forward_key_pressed = in.tab && !in.alt;
	_dirkeys =
		(in.dir_left  ? 1 : 0) |
		(in.dir_up    ? 2 : 0) |
		(in.dir_right ? 4 : 0) |
		(in.dir_down  ? 8 : 0);
	if (MiniUiActive() && !EditBoxInGlobalFocus()) {
		_dirkeys |=
			(in.key_a ? 1 : 0) |
			(in.key_w ? 2 : 0) |
			(in.key_d ? 4 : 0) |
			(in.key_s ? 8 : 0);
	}
	if (old_ctrl_pressed != _ctrl_pressed) HandleCtrlChanged();

	if (_cursor.UpdateCursorPosition(in.mouse_x, in.mouse_y)) {
		RlwWarpMouse(_cursor.pos.x, _cursor.pos.y);
	}

	_left_button_down = in.left_down;
	if (!in.left_down) _left_button_clicked = false;
	if (in.right_down && !this->prev_right_down) _right_button_clicked = true;
	_right_button_down = in.right_down;
	this->prev_right_down = in.right_down;
	_middle_button_down = in.middle_down;

	if (in.wheel_y != 0.0f || in.wheel_x != 0.0f) {
		this->wheel_accum += in.wheel_y;
		while (this->wheel_accum >= 1.0f) {
			_cursor.wheel--;
			this->wheel_accum -= 1.0f;
		}
		while (this->wheel_accum <= -1.0f) {
			_cursor.wheel++;
			this->wheel_accum += 1.0f;
		}
		const float SCROLL_BUILTIN_MULTIPLIER = 14.0f;
		_cursor.v_wheel -= in.wheel_y * SCROLL_BUILTIN_MULTIPLIER * _settings_client.gui.scrollwheel_multiplier;
		_cursor.h_wheel += in.wheel_x * SCROLL_BUILTIN_MULTIPLIER * _settings_client.gui.scrollwheel_multiplier;
		_cursor.wheel_moved = true;
	}

	HandleMouseEvents();

	char32_t character;
	for (uint32_t keycode; (keycode = RlwNextKey(character)) != 0; ) {
		if (!this->edit_box_focused || IsEditBoxControlKey(keycode) || !IsValidChar(character, CS_ALPHANUMERAL)) {
			HandleKeypress(keycode, character);
		}
	}

	for (char32_t c; (c = RlwNextChar()) != 0; ) {
		if (!this->edit_box_focused) continue;
		auto [buf, len] = EncodeUtf8(c);
		HandleTextInput(std::string_view(buf, len));
	}
}

void VideoDriver_Raylib::CheckPaletteAnim()
{
	if (!CopyPalette(this->local_palette)) return;
	this->MakeDirty(0, 0, _screen.width, _screen.height);
}

void VideoDriver_Raylib::Paint()
{
	PerformanceMeasurer framerate(PFE_VIDEO);

	if (this->local_palette.count_dirty != 0) {
		Blitter *blitter = BlitterFactory::GetCurrentBlitter();
		if (blitter->UsePaletteAnimation() == Blitter::PaletteAnimation::Blitter) {
			blitter->PaletteAnimate(this->local_palette);
		}
		this->local_palette.count_dirty = 0;
	}

	if (MiniUiActive()) {
		/* The mini UI frame comes from the command buffer; the CPU screen is
		 * only sampled under the native windows. */
		static std::vector<RlwRectI> rects;
		rects.clear();
		MiniUiOverlayRects(rects);
		RlwPresentMini(this->vid_buf.data(), _screen.pitch, _screen.width, _screen.height, rects.data(), rects.size());
		this->dirty_rect = {};
		return;
	}

	/* The blitter writes 0xAARRGGBB; the texture wants RGBA bytes, so swap the
	 * red and blue channels and force full alpha. */
	const uint32_t *src = this->vid_buf.data();
	uint32_t *dst = this->present_buf.data();
	size_t n = this->vid_buf.size();
	for (size_t i = 0; i < n; i++) {
		uint32_t c = src[i];
		dst[i] = 0xFF000000 | (c & 0x0000FF00) | ((c >> 16) & 0xFF) | ((c & 0xFF) << 16);
	}

	RlwPresent(dst, _screen.width, _screen.height);
	this->dirty_rect = {};
}
