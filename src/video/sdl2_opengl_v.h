/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sdl2_opengl_v.h OpenGL backend of the SDL2 video driver. */

#include "sdl2_v.h"
#include "../mini/core/system_pointer.h"

struct SDL_Cursor;

class SdlPointer : public SystemPointer {
public:
	~SdlPointer() override { this->Release(); }

protected:
	bool Adopt(const CursorPicture &picture) override;
	void Release() override;
	void Show(bool shown) override;

private:
	SDL_Cursor *cursor = nullptr;
};

/** The OpenGL video driver for windows. */
class VideoDriver_SDL_OpenGL : public VideoDriver_SDL_Base {
public:
	VideoDriver_SDL_OpenGL() : VideoDriver_SDL_Base(true), gl_context(nullptr), anim_buffer(nullptr) {}

	std::optional<std::string_view> Start(const StringList &param) override;

	void Stop() override;

	bool HasEfficient8Bpp() const override { return true; }

	bool UseSystemCursor() override { return true; }

	void ClearSystemSprites() override;

	void PopulateSystemSprites() override;

	bool HasAnimBuffer() override { return true; }
	uint8_t *GetAnimBuffer() override { return this->anim_buffer; }

	std::string_view GetName() const override { return "sdl-opengl"; }

protected:
	bool AllocateBackingStore(int w, int h, bool force = false) override;
	void *GetVideoPointer() override;
	void ReleaseVideoPointer() override;
	void CheckPaletteAnim() override;
	void Paint() override;
	bool CreateMainWindow(uint w, uint h, uint flags) override;
	bool SetSwapInterval(int interval) override;

private:
	void  *gl_context;  ///< OpenGL context.
	uint8_t *anim_buffer; ///< Animation buffer from OpenGL back-end.
	SdlPointer pointer; ///< Cursor the system draws in place of the game's own.

	std::optional<std::string_view> AllocateContext();
	void DestroyContext();
};

/** The factory for SDL' OpenGL video driver. */
class FVideoDriver_SDL_OpenGL : public DriverFactoryBase {
public:
	FVideoDriver_SDL_OpenGL() : DriverFactoryBase(Driver::DT_VIDEO, 8, "sdl-opengl", "SDL OpenGL Video Driver") {}
	/* virtual */ std::unique_ptr<Driver> CreateInstance() const override { return std::make_unique<VideoDriver_SDL_OpenGL>(); }

protected:
	bool UsesHardwareAcceleration() const override { return true; }
};
