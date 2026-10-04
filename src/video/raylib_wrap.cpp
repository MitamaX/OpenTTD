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
#include <rlgl.h>
#include <unordered_map>
#include <vector>
#include "imgui.h"
#include "../3rdparty/rlimgui/rlImGui.h"
#include "../3rdparty/rlimgui/imgui_impl_raylib.h"
#include "raylib_wrap.h"

#include "../safeguards.h"

static Texture2D _rlw_tex;
static bool _rlw_tex_ok = false;
static int _rlw_tex_w, _rlw_tex_h;

enum class RlwCmdType : uint8_t {
	Rect,
	RoundRect,
	GradientRect,
	Line,
	Circle,
	Diamond,
	Triangle,
	TexQuad,
	Sprite,
};

struct RlwCmd {
	RlwCmdType type;
	int a, b, c, d, e;
	uint32_t col;
	uint32_t col_tr, col_bl, col_br;
};

static std::vector<RlwCmd> _rlw_cmds;
static std::unordered_map<int, Texture2D> _rlw_user_tex;
static int _rlw_next_tex = 1;

static Color RlwColour(uint32_t argb)
{
	return { (uint8_t)(argb >> 16), (uint8_t)(argb >> 8), (uint8_t)argb, (uint8_t)(argb >> 24) };
}

bool RlwInit(int w, int h, const char *title, bool vsync)
{
	SetTraceLogLevel(LOG_WARNING);
	SetConfigFlags(FLAG_WINDOW_RESIZABLE | (vsync ? FLAG_VSYNC_HINT : 0));
	InitWindow(w, h, title);
	if (!IsWindowReady()) return false;
	SetExitKey(KEY_NULL);
	return true;
}

void RlwSetVsync(bool on)
{
	if (on) {
		SetWindowState(FLAG_VSYNC_HINT);
	} else {
		ClearWindowState(FLAG_VSYNC_HINT);
	}
}

static bool _rlw_imgui_ready = false;
static bool _rlw_imgui_frame = false;
static bool _rlw_imgui_drawn = false;

void RlwImGuiInit()
{
	rlImGuiSetup(true);
	ImGui::GetIO().IniFilename = nullptr;
	_rlw_imgui_ready = true;
}

void RlwImGuiShutdown()
{
	if (!_rlw_imgui_ready) return;
	if (_rlw_imgui_frame) ImGui::EndFrame();
	_rlw_imgui_frame = false;
	_rlw_imgui_drawn = false;
	rlImGuiShutdown();
	_rlw_imgui_ready = false;
}

void RlwImGuiNewFrame()
{
	if (!_rlw_imgui_ready) return;
	if (_rlw_imgui_frame) ImGui::EndFrame();
	rlImGuiBegin();
	_rlw_imgui_frame = true;
}

/* A present can arrive without a fresh ImGui frame, e.g. while a mode switch
 * suppresses window updates; re-submitting the last draw data keeps the layer
 * from blinking out on those frames. */
static void RlwImGuiRender()
{
	if (!_rlw_imgui_ready) return;
	if (_rlw_imgui_frame) {
		ImGui::Render();
		_rlw_imgui_frame = false;
		_rlw_imgui_drawn = true;
	}
	if (!_rlw_imgui_drawn) return;
	ImDrawData *dd = ImGui::GetDrawData();
	if (dd == nullptr) return;
	ImGui_ImplRaylib_RenderDrawData(dd);
}

static void RlwImGuiDropFrame()
{
	if (!_rlw_imgui_frame) return;
	ImGui::EndFrame();
	_rlw_imgui_frame = false;
}

uintptr_t RlwScreenTexId()
{
	return _rlw_tex_ok ? _rlw_tex.id : 0;
}

static RlwLayer *_rlw_layer = nullptr;

void RlwAttachLayer(RlwLayer *layer)
{
	_rlw_layer = layer;
}

static void RlwLayerRender(int w, int h)
{
	if (_rlw_layer == nullptr) return;
	rlDrawRenderBatchActive();
	_rlw_layer->Render(w, h);
}

static void RlwLayerDetach()
{
	if (_rlw_layer == nullptr) return;
	_rlw_layer->Detach();
	_rlw_layer = nullptr;
}

void RlwClose()
{
	RlwLayerDetach();
	RlwImGuiShutdown();
	if (_rlw_tex_ok) {
		UnloadTexture(_rlw_tex);
		_rlw_tex_ok = false;
	}
	for (auto &[id, tex] : _rlw_user_tex) UnloadTexture(tex);
	_rlw_user_tex.clear();
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
	in.key_w = IsKeyDown(KEY_W);
	in.key_a = IsKeyDown(KEY_A);
	in.key_s = IsKeyDown(KEY_S);
	in.key_d = IsKeyDown(KEY_D);
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

/* (Re)creates the screen texture at the given size; fresh textures start
 * from the passed pixels, matching ones are left untouched. */
static bool RlwEnsureScreenTexture(const uint32_t *rgba, int w, int h)
{
	if (_rlw_tex_ok && _rlw_tex_w == w && _rlw_tex_h == h) return false;
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
	return true;
}

void RlwPresent(const uint32_t *rgba, int w, int h)
{
	RlwImGuiDropFrame();
	if (!RlwEnsureScreenTexture(rgba, w, h)) UpdateTexture(_rlw_tex, rgba);

	BeginDrawing();
	ClearBackground(BLACK);
	DrawTexture(_rlw_tex, 0, 0, WHITE);
	EndDrawing();
}

void RlwCmdClear()
{
	_rlw_cmds.clear();
}

void RlwCmdRect(int x0, int y0, int x1, int y1, uint32_t argb)
{
	_rlw_cmds.push_back({RlwCmdType::Rect, x0, y0, x1, y1, 0, argb, 0, 0, 0});
}

void RlwCmdRoundRect(int x0, int y0, int x1, int y1, int radius, uint32_t argb)
{
	_rlw_cmds.push_back({RlwCmdType::RoundRect, x0, y0, x1, y1, radius, argb, 0, 0, 0});
}

void RlwCmdGradientRect(int x0, int y0, int x1, int y1, uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br)
{
	_rlw_cmds.push_back({RlwCmdType::GradientRect, x0, y0, x1, y1, 0, tl, tr, bl, br});
}

void RlwCmdLine(int x0, int y0, int x1, int y1, int width, uint32_t argb)
{
	_rlw_cmds.push_back({RlwCmdType::Line, x0, y0, x1, y1, width, argb});
}

void RlwCmdCircle(int cx, int cy, int r, uint32_t argb)
{
	_rlw_cmds.push_back({RlwCmdType::Circle, cx, cy, r, 0, 0, argb});
}

void RlwCmdDiamond(int cx, int cy, int r, uint32_t argb)
{
	_rlw_cmds.push_back({RlwCmdType::Diamond, cx, cy, r, 0, 0, argb});
}

void RlwCmdTriangle(int cx, int cy, int r, uint32_t argb)
{
	_rlw_cmds.push_back({RlwCmdType::Triangle, cx, cy, r, 0, 0, argb});
}

void RlwCmdTexQuad(int tex, int x, int y, uint32_t tint_argb)
{
	_rlw_cmds.push_back({RlwCmdType::TexQuad, tex, x, y, 0, 0, tint_argb});
}

/* The atlas source rectangle and rotation ride in the spare colour words. */
void RlwCmdSprite(int tex, int sx, int sy, int sw, int sh, int dx0, int dy0, int dx1, int dy1, int angle_deg, uint32_t tint_argb)
{
	_rlw_cmds.push_back({RlwCmdType::Sprite, tex, dx0, dy0, dx1, dy1, tint_argb, ((uint32_t)sx << 16) | (uint32_t)sy, ((uint32_t)sw << 16) | (uint32_t)sh, (uint32_t)(int32_t)angle_deg});
}

int RlwCreateTexture(const uint32_t *rgba, int w, int h)
{
	Image img;
	img.data = const_cast<uint32_t *>(rgba);
	img.width = w;
	img.height = h;
	img.mipmaps = 1;
	img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
	int id = _rlw_next_tex++;
	_rlw_user_tex.emplace(id, LoadTextureFromImage(img));
	return id;
}

/* Atlas sprites are drawn at many scales, so they get mipmaps and smooth
 * filtering; plain textures stay point-sampled for 1:1 blits. */
int RlwCreateAtlasTexture(const uint32_t *rgba, int w, int h)
{
	int id = RlwCreateTexture(rgba, w, h);
	Texture2D &tex = _rlw_user_tex.at(id);
	GenTextureMipmaps(&tex);
	SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
	return id;
}

/* Ground art repeats along merged tile runs, so its wrap mode must tile. */
int RlwCreateTileTexture(const uint32_t *rgba, int w, int h)
{
	int id = RlwCreateTexture(rgba, w, h);
	Texture2D &tex = _rlw_user_tex.at(id);
	GenTextureMipmaps(&tex);
	SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
	SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
	return id;
}

/* Decodes an image file and delivers it as exactly w x h RGBA pixels. */
bool RlwLoadImageInto(const char *path, uint32_t *rgba, int w, int h)
{
	if (!FileExists(path)) return false;
	Image img = LoadImage(path);
	if (img.data == nullptr) return false;
	ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
	if (img.width != w || img.height != h) ImageResize(&img, w, h);
	std::copy_n(static_cast<const uint32_t *>(img.data), (size_t)w * h, rgba);
	UnloadImage(img);
	return true;
}

void RlwFreeTexture(int tex)
{
	auto it = _rlw_user_tex.find(tex);
	if (it == _rlw_user_tex.end()) return;
	UnloadTexture(it->second);
	_rlw_user_tex.erase(it);
}

static void RlwReplayCommands()
{
	for (const RlwCmd &c : _rlw_cmds) {
		switch (c.type) {
			case RlwCmdType::Rect:
				DrawRectangle(c.a, c.b, c.c - c.a + 1, c.d - c.b + 1, RlwColour(c.col));
				break;
			case RlwCmdType::RoundRect: {
				float w = (float)(c.c - c.a + 1);
				float h = (float)(c.d - c.b + 1);
				float m = w < h ? w : h;
				float round = m > 0.0f ? 2.0f * c.e / m : 0.0f;
				if (round > 1.0f) round = 1.0f;
				DrawRectangleRounded({(float)c.a, (float)c.b, w, h}, round, 6, RlwColour(c.col));
				break;
			}
			case RlwCmdType::GradientRect:
				DrawRectangleGradientEx({(float)c.a, (float)c.b, (float)(c.c - c.a + 1), (float)(c.d - c.b + 1)}, RlwColour(c.col), RlwColour(c.col_bl), RlwColour(c.col_br), RlwColour(c.col_tr));
				break;
			case RlwCmdType::Line:
				DrawLineEx({(float)c.a, (float)c.b}, {(float)c.c, (float)c.d}, (float)c.e, RlwColour(c.col));
				break;
			case RlwCmdType::Circle:
				/* DrawCircle always tessellates 36 segments; thousands of tiny
				 * dots per frame want far fewer vertices than that. */
				if (c.c <= 1) {
					DrawRectangle(c.a - c.c, c.b - c.c, 2 * c.c + 1, 2 * c.c + 1, RlwColour(c.col));
				} else {
					DrawPoly({(float)c.a, (float)c.b}, c.c <= 4 ? 8 : 20, (float)c.c, 0.0f, RlwColour(c.col));
				}
				break;
			case RlwCmdType::Diamond:
				DrawPoly({(float)c.a, (float)c.b}, 4, (float)c.c, 0.0f, RlwColour(c.col));
				break;
			case RlwCmdType::Triangle:
				DrawTriangle({(float)c.a, (float)(c.b - c.c)}, {(float)(c.a - c.c), (float)(c.b + c.c)}, {(float)(c.a + c.c), (float)(c.b + c.c)}, RlwColour(c.col));
				break;
			case RlwCmdType::TexQuad:
				if (auto it = _rlw_user_tex.find(c.a); it != _rlw_user_tex.end()) {
					DrawTexture(it->second, c.b, c.c, RlwColour(c.col));
				}
				break;
			case RlwCmdType::Sprite:
				if (auto it = _rlw_user_tex.find(c.a); it != _rlw_user_tex.end()) {
					Rectangle src = {(float)(c.col_tr >> 16), (float)(c.col_tr & 0xFFFF), (float)(c.col_bl >> 16), (float)(c.col_bl & 0xFFFF)};
					float w = (float)(c.d - c.b + 1);
					float h = (float)(c.e - c.c + 1);
					/* The quad rotates about its centre; the origin offset puts the
					 * centre back on the destination rectangle, so angle 0 lands
					 * pixel-exact on the unrotated position. */
					Rectangle dst = {c.b + w * 0.5f, c.c + h * 0.5f, w, h};
					DrawTexturePro(it->second, src, dst, {w * 0.5f, h * 0.5f}, (float)(int32_t)c.col_br, RlwColour(c.col));
				}
				break;
		}
	}
}

void RlwPresentMini(const uint32_t *argb, int pitch, int w, int h, const RlwRectI *overlays, size_t count)
{
	if (!_rlw_tex_ok || _rlw_tex_w != w || _rlw_tex_h != h) {
		std::vector<uint32_t> black((size_t)w * h, 0xFF000000U);
		RlwEnsureScreenTexture(black.data(), w, h);
	}

	/* The screen texture only stays fresh under the native windows; the rest
	 * of the frame comes from the command buffer. */
	static std::vector<uint32_t> stage;
	for (size_t i = 0; i < count; i++) {
		RlwRectI r = overlays[i];
		if (r.x < 0) { r.w += r.x; r.x = 0; }
		if (r.y < 0) { r.h += r.y; r.y = 0; }
		if (r.x + r.w > w) r.w = w - r.x;
		if (r.y + r.h > h) r.h = h - r.y;
		if (r.w <= 0 || r.h <= 0) continue;
		stage.resize((size_t)r.w * r.h);
		for (int y = 0; y < r.h; y++) {
			const uint32_t *src = argb + (size_t)(r.y + y) * pitch + r.x;
			uint32_t *dst = stage.data() + (size_t)y * r.w;
			for (int x = 0; x < r.w; x++) {
				uint32_t c = src[x];
				dst[x] = 0xFF000000 | (c & 0x0000FF00) | ((c >> 16) & 0xFF) | ((c & 0xFF) << 16);
			}
		}
		UpdateTextureRec(_rlw_tex, {(float)r.x, (float)r.y, (float)r.w, (float)r.h}, stage.data());
	}

	BeginDrawing();
	ClearBackground(BLACK);
	RlwReplayCommands();
	/* Two passes around the ImGui layer: carrier viewports below it, every
	 * other native window above it. */
	for (int pass = 0; pass < 2; pass++) {
		if (pass == 1) {
			RlwImGuiRender();
			RlwLayerRender(w, h);
		}
		for (size_t i = 0; i < count; i++) {
			RlwRectI r = overlays[i];
			if (r.under != (pass == 0)) continue;
			if (r.sample_only) continue;
			if (r.x < 0) { r.w += r.x; r.x = 0; }
			if (r.y < 0) { r.h += r.y; r.y = 0; }
			if (r.x + r.w > w) r.w = w - r.x;
			if (r.y + r.h > h) r.h = h - r.y;
			if (r.w <= 0 || r.h <= 0) continue;
			DrawTextureRec(_rlw_tex, {(float)r.x, (float)r.y, (float)r.w, (float)r.h}, {(float)r.x, (float)r.y}, WHITE);
		}
	}
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
