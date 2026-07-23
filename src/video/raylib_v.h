/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file raylib_v.h The raylib video driver: raylib owns the window and presents the game screen as a texture. */

#ifndef VIDEO_RAYLIB_H
#define VIDEO_RAYLIB_H

#include "video_driver.hpp"

class VideoDriver_Raylib : public VideoDriver {
public:
	VideoDriver_Raylib() : VideoDriver(true) {}

	std::optional<std::string_view> Start(const StringList &param) override;

	void Stop() override;

	void MakeDirty(int left, int top, int width, int height) override;

	void MainLoop() override;

	bool ChangeResolution(int w, int h) override;

	bool ToggleFullscreen(bool fullscreen) override;

	bool AfterBlitterChange() override;

	void EditBoxGainedFocus() override { this->edit_box_focused = true; }

	void EditBoxLostFocus() override { this->edit_box_focused = false; }

	std::string_view GetName() const override { return "raylib"; }

protected:
	Dimension GetScreenSize() const override;
	void InputLoop() override;
	void Paint() override;
	void CheckPaletteAnim() override;

private:
	void ClientSizeChanged(int w, int h, bool force);

	std::vector<uint32_t> vid_buf;
	std::vector<uint32_t> present_buf;
	Palette local_palette{};
	Rect dirty_rect{};
	bool edit_box_focused = false;
	bool exit_requested = false;
	bool prev_right_down = false;
	float wheel_accum = 0.0f;
};

class FVideoDriver_Raylib : public DriverFactoryBase {
public:
	FVideoDriver_Raylib() : DriverFactoryBase(Driver::DT_VIDEO, 10, "raylib", "raylib Video Driver") {}
	std::unique_ptr<Driver> CreateInstance() const override { return std::make_unique<VideoDriver_Raylib>(); }
};

#endif /* VIDEO_RAYLIB_H */
