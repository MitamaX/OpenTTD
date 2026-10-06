/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tuning.h The mini UI knobs read from mini_ui.cfg in the personal directory. */

#ifndef MINI_CORE_TUNING_H
#define MINI_CORE_TUNING_H

struct MiniTuning {
	int start_active = 1;
	double pan_speed = 1600.0;
	double pan_speed_fast = 4000.0;
	double pan_smooth_ms = 60.0;
	double zoom_step = 1.25;
	double zoom_smooth_ms = 80.0;
	double turn_smooth_ms = 160.0;
	double height_scale = 1.5;
	int hud_scale = 2;
	int grid_alpha = 80;
	int contour_alpha = 120;
	int relief_strength = 22;
	int filter_alpha = 150;
	int edge_scroll = 0;
	int edge_margin = 24;
	double edge_scroll_speed = 1600.0;
	double jump_ppt = 32.0;
	double glide_ms = 250.0;

	void Load();
};

extern MiniTuning _tuning;

#endif /* MINI_CORE_TUNING_H */
