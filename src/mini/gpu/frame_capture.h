/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file frame_capture.h Composed mini UI frames written to PNG files, one per shot of a shot list the environment names. */

#ifndef MINI_GPU_FRAME_CAPTURE_H
#define MINI_GPU_FRAME_CAPTURE_H

#include <optional>
#include <string>
#include <vector>

#include "../../core/geometry_type.hpp"
#include "../core/camera.h"

struct CaptureShot {
	std::string path;
	ViewAim aim;
	uint wait_frames;
	std::optional<ViewAim> from = std::nullopt;
};

class FrameCapture {
public:
	static constexpr const char SHOT_LIST_VARIABLE[] = "OTTD_MINI_CAPTURE";
	static constexpr const char MAP_ONLY_VARIABLE[] = "OTTD_MINI_CAPTURE_NOHUD";
	static constexpr const char CLEAN_VARIABLE[] = "OTTD_MINI_CAPTURE_CLEAN";
	static constexpr const char SPEED_VARIABLE[] = "OTTD_MINI_CAPTURE_SPEED";

	FrameCapture();

	bool Active() const { return this->next < this->shots.size(); }
	bool HidesHud() const { return this->map_only || this->clean; }
	bool HidesLabels() const { return this->clean; }
	bool HidesGuides() const { return this->clean; }
	std::optional<ViewAim> Aim() const;
	std::optional<uint16_t> GameSpeed() const { return this->Active() ? this->speed : std::nullopt; }
	void Grab(Dimension screen);

private:
	void Write(const CaptureShot &shot, Dimension screen) const;

	std::vector<CaptureShot> shots;
	size_t next = 0;
	uint frames = 0;
	bool map_only = false;
	bool clean = false;
	std::optional<uint16_t> speed;
};

extern FrameCapture _frame_capture;

#endif /* MINI_GPU_FRAME_CAPTURE_H */
