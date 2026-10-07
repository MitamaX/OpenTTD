/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file frame_capture.cpp Composed mini UI frames written to PNG files, one per shot of a shot list the environment names. */

#include "../../stdafx.h"
#include "frame_capture.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "../../core/string_consumer.hpp"
#include "../../debug.h"
#include "../../gfx_type.h"
#include "../../openttd.h"
#include "../../screenshot_type.h"
#include "../../string_func.h"
#include "frame_profile.h"
#include "gl_api.h"

#include "../../safeguards.h"

static constexpr uint DEFAULT_WAIT_FRAMES = 60;
static constexpr std::string_view WAIT_COMMAND = "wait";
static constexpr std::string_view GLIDE_COMMAND = "glide";
static constexpr std::string_view COMMENT_MARK = "#";
static constexpr std::string_view PNG_PROVIDER = "png";
static constexpr std::string_view PNG_EXTENSION = ".png";
static constexpr int CAPTURE_DEPTH = 32;

FrameCapture _frame_capture;

static std::filesystem::path FsPath(std::string_view path)
{
	return std::filesystem::path(OTTD2FS(path));
}

static std::optional<ViewAim> ReadAim(std::istream &fields)
{
	ViewAim aim{};
	if (!(fields >> aim.focus.first >> aim.focus.second >> aim.zoom >> aim.yaw >> aim.pitch)) return std::nullopt;
	return aim;
}

/* A shot path is taken from the shot list's own folder unless it is absolute. */
static std::string ShotPath(const std::filesystem::path &list, std::string_view name)
{
	std::filesystem::path path = list.parent_path() / FsPath(name);
	if (path.extension() != PNG_EXTENSION) path += PNG_EXTENSION;
	return FS2OTTD(path.native());
}

static std::vector<CaptureShot> ReadShotList(std::string_view list_path)
{
	std::filesystem::path list = FsPath(list_path);
	std::ifstream stream(list);
	std::vector<CaptureShot> shots;
	uint wait_frames = DEFAULT_WAIT_FRAMES;

	std::string line;
	while (std::getline(stream, line)) {
		std::istringstream fields(line);
		std::string name;
		if (!(fields >> name) || name.starts_with(COMMENT_MARK)) continue;

		if (name == WAIT_COMMAND) {
			uint frames;
			if (fields >> frames) wait_frames = std::max(1U, frames);
			continue;
		}

		if (name == GLIDE_COMMAND) {
			uint frames;
			if (!(fields >> frames)) continue;
			std::optional<ViewAim> aim = ReadAim(fields);
			if (!aim.has_value()) continue;
			ViewAim from = shots.empty() ? *aim : shots.back().aim;
			shots.push_back({{}, *aim, std::max(1U, frames), from});
			continue;
		}

		if (std::optional<ViewAim> aim = ReadAim(fields); aim.has_value()) shots.push_back({ShotPath(list, name), *aim, wait_frames});
	}
	return shots;
}

static const ScreenshotProvider *PngProvider()
{
	const auto &providers = ProviderManager<ScreenshotProvider>::GetProviders();
	auto it = std::ranges::find(providers, PNG_PROVIDER, &ScreenshotProvider::GetName);
	return it == std::end(providers) ? nullptr : *it;
}

FrameCapture::FrameCapture()
{
	std::optional<std::string_view> list = GetEnv(SHOT_LIST_VARIABLE);
	if (!list.has_value()) return;

	this->shots = ReadShotList(*list);
	this->map_only = GetEnv(MAP_ONLY_VARIABLE) == "1";
	this->clean = GetEnv(CLEAN_VARIABLE) == "1";
	if (std::optional<std::string_view> speed = GetEnv(SPEED_VARIABLE); speed.has_value()) this->speed = ParseInteger<uint16_t>(*speed);
}

std::optional<ViewAim> FrameCapture::Aim() const
{
	if (!this->Active()) return std::nullopt;
	const CaptureShot &shot = this->shots[this->next];
	if (!shot.from.has_value()) return shot.aim;

	double share = std::min(1.0, (this->frames + 1.0) / shot.wait_frames);
	const ViewAim &from = *shot.from;
	return ViewAim{
		{std::lerp(from.focus.first, shot.aim.focus.first, share), std::lerp(from.focus.second, shot.aim.focus.second, share)},
		std::lerp(from.zoom, shot.aim.zoom, share), std::lerp(from.yaw, shot.aim.yaw, share), std::lerp(from.pitch, shot.aim.pitch, share),
	};
}

/* Every composed frame counts toward the shot in hand; the game ends once the last shot is written. */
void FrameCapture::Grab(Dimension screen)
{
	if (!this->Active() || ++this->frames < this->shots[this->next].wait_frames) return;

	const CaptureShot &shot = this->shots[this->next];
	if (!shot.from.has_value()) {
		this->Write(shot, screen);
		_frame_profile.Report(shot.path);
	}
	this->frames = 0;
	if (++this->next == this->shots.size()) _exit_game = true;
}

/* The window's back buffer holds the composed frame bottom row first. */
void FrameCapture::Write(const CaptureShot &shot, Dimension screen) const
{
	std::vector<Colour> pixels(static_cast<size_t>(screen.width) * screen.height);
	GLint read_framebuffer = 0;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glReadBuffer(GL_BACK);
	glReadPixels(0, 0, static_cast<GLsizei>(screen.width), static_cast<GLsizei>(screen.height), GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels.data());
	glBindFramebuffer(GL_READ_FRAMEBUFFER, read_framebuffer);

	auto rows = [&](void *buffer, uint y, uint pitch, uint count) {
		Colour *out = static_cast<Colour *>(buffer);
		for (uint row = y; row < y + count; row++, out += pitch) {
			auto source = pixels.begin() + static_cast<ptrdiff_t>(screen.height - 1 - row) * screen.width;
			std::copy_n(source, screen.width, out);
		}
	};

	std::error_code ignored;
	std::filesystem::create_directories(FsPath(shot.path).parent_path(), ignored);
	const ScreenshotProvider *png = PngProvider();
	if (png == nullptr || !png->MakeImage(shot.path, rows, screen.width, screen.height, CAPTURE_DEPTH, nullptr)) {
		Debug(misc, 0, "Mini frame capture could not write {}", shot.path);
	}
}
