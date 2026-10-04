/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_painter.h Vehicles on the top-down map: consists, silhouettes, headings and cargo. */

#ifndef MINI_MAP_VEHICLE_PAINTER_H
#define MINI_MAP_VEHICLE_PAINTER_H

#include "../../vehicle_type.h"
#include "map_overlay.h"
#include "zoom_detail.h"

struct Vehicle;

class VehiclePainter {
public:
	void Paint(int ppt, MiniLayer filter);

private:
	struct Tones {
		uint32_t fill;
		uint32_t ink;
		bool dim;
	};

	bool InLayer(VehicleType vt) const;
	Tones TonesOf(const Vehicle *v) const;
	void PaintConsist(const Vehicle *head, int ppt);
	void PaintUnit(const Vehicle *v, int ppt);

	ZoomDetail detail{};
	MiniLayer filter = MiniLayer::None;
};

extern VehiclePainter _vehicle_painter;

#endif /* MINI_MAP_VEHICLE_PAINTER_H */
