/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tuning.cpp The mini UI knobs read from mini_ui.cfg in the personal directory. */

#include "../../stdafx.h"
#include "tuning.h"

#include <array>
#include <charconv>

#include "../../core/format.hpp"
#include "../../core/math_func.hpp"
#include "../../fileio_func.h"
#include "../../ini_type.h"
#include "camera.h"

#include "../../safeguards.h"

static constexpr std::string_view TUNING_FILE = "mini_ui.cfg";
static constexpr std::string_view TUNING_GROUP = "mini";
static constexpr std::array<std::string_view, 2> RETIRED_KEYS = {"height_scale", "relief_strength"};

MiniTuning _tuning;

template <typename T>
static void ReadIniNumber(IniGroup &group, std::string_view name, T &v)
{
	if (const IniItem *item = group.GetItem(name); item != nullptr && item->value.has_value()) {
		const std::string &s = *item->value;
		T parsed;
		if (std::from_chars(s.data(), s.data() + s.size(), parsed).ec == std::errc{}) v = parsed;
	} else {
		group.GetOrCreateItem(name).SetValue(fmt::format("{}", v));
	}
}

/* Re-read on every activation, so tuning only needs an F9 round trip. */
void MiniTuning::Load()
{
	*this = {};

	std::string path = _personal_dir + std::string(TUNING_FILE);
	IniFile ini;
	ini.LoadFromDisk(path, NO_DIRECTORY);
	IniGroup &group = ini.GetOrCreateGroup(TUNING_GROUP);
	for (std::string_view key : RETIRED_KEYS) group.RemoveItem(key);

	ReadIniNumber(group, "start_active", this->start_active);
	ReadIniNumber(group, "pan_speed", this->pan_speed);
	ReadIniNumber(group, "pan_speed_fast", this->pan_speed_fast);
	ReadIniNumber(group, "pan_smooth_ms", this->pan_smooth_ms);
	ReadIniNumber(group, "zoom_step", this->zoom_step);
	ReadIniNumber(group, "zoom_smooth_ms", this->zoom_smooth_ms);
	ReadIniNumber(group, "turn_smooth_ms", this->turn_smooth_ms);
	ReadIniNumber(group, "turn_speed", this->turn_speed);
	ReadIniNumber(group, "orbit_speed", this->orbit_speed);
	ReadIniNumber(group, "view_fov", this->view_fov);
	ReadIniNumber(group, "view_pitch", this->view_pitch);
	ReadIniNumber(group, "elevation_scale", this->elevation_scale);
	ReadIniNumber(group, "hud_scale", this->hud_scale);
	ReadIniNumber(group, "grid_alpha", this->grid_alpha);
	ReadIniNumber(group, "contour_alpha", this->contour_alpha);
	ReadIniNumber(group, "filter_alpha", this->filter_alpha);
	ReadIniNumber(group, "edge_scroll", this->edge_scroll);
	ReadIniNumber(group, "edge_margin", this->edge_margin);
	ReadIniNumber(group, "edge_scroll_speed", this->edge_scroll_speed);
	ReadIniNumber(group, "jump_ppt", this->jump_ppt);
	ReadIniNumber(group, "glide_ms", this->glide_ms);
	ReadIniNumber(group, "ambient_occlusion", this->ambient_occlusion);
	ReadIniNumber(group, "bloom", this->bloom);
	ReadIniNumber(group, "anti_aliasing", this->anti_aliasing);
	ReadIniNumber(group, "depth_of_field", this->depth_of_field);
	ReadIniNumber(group, "exposure", this->exposure);

	this->pan_speed = Clamp(this->pan_speed, 100.0, 10000.0);
	this->pan_speed_fast = Clamp(this->pan_speed_fast, 100.0, 20000.0);
	this->pan_smooth_ms = Clamp(this->pan_smooth_ms, 1.0, 500.0);
	this->zoom_step = Clamp(this->zoom_step, 1.05, 2.0);
	this->zoom_smooth_ms = Clamp(this->zoom_smooth_ms, 1.0, 500.0);
	this->turn_smooth_ms = Clamp(this->turn_smooth_ms, 1.0, 1000.0);
	this->turn_speed = Clamp(this->turn_speed, 10.0, 720.0);
	this->orbit_speed = Clamp(this->orbit_speed, 0.02, 2.0);
	this->view_fov = Clamp(this->view_fov, 20.0, 70.0);
	this->view_pitch = Clamp(this->view_pitch, MIN_PITCH, MAX_PITCH);
	this->elevation_scale = Clamp(this->elevation_scale, 0.25, 2.0);
	this->hud_scale = Clamp(this->hud_scale, 1, 4);
	this->grid_alpha = Clamp(this->grid_alpha, 0, 255);
	this->contour_alpha = Clamp(this->contour_alpha, 0, 255);
	this->filter_alpha = Clamp(this->filter_alpha, 0, 230);
	this->edge_margin = Clamp(this->edge_margin, 2, 200);
	this->edge_scroll_speed = Clamp(this->edge_scroll_speed, 100.0, 10000.0);
	this->jump_ppt = Clamp(this->jump_ppt, MIN_PPT, MAX_PPT);
	this->glide_ms = Clamp(this->glide_ms, 1.0, 2000.0);
	this->anti_aliasing = Clamp(this->anti_aliasing, 0, 2);
	this->exposure = Clamp(this->exposure, -4.0, 4.0);

	ini.SaveToDisk(path);
}
