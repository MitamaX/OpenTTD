/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file zoom_detail.h What the map shows at each zoom tier. */

#ifndef MINI_MAP_ZOOM_DETAIL_H
#define MINI_MAP_ZOOM_DETAIL_H

/* Zoom tiers: below 8 ppt the map is a terrain overview, mid zoom shows
 * infrastructure, close zoom adds per-unit detail. */
inline constexpr int INFRASTRUCTURE_PPT = 8;
inline constexpr int UNIT_DETAIL_PPT = 16;

struct ZoomDetail {
	bool tree_dots;
	bool block_borders;
	bool signals;
	bool oneway;
	bool cargo_dots;
	bool vehicle_shapes;
	bool station_names;
	bool all_town_names;

	static constexpr ZoomDetail For(int ppt)
	{
		bool infrastructure = ppt >= INFRASTRUCTURE_PPT;
		return {
			.tree_dots = infrastructure,
			.block_borders = infrastructure,
			.signals = infrastructure,
			.oneway = infrastructure,
			.cargo_dots = ppt >= UNIT_DETAIL_PPT,
			.vehicle_shapes = infrastructure,
			.station_names = infrastructure,
			.all_town_names = infrastructure,
		};
	}
};

#endif /* MINI_MAP_ZOOM_DETAIL_H */
