/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_labels.h Name plates over towns, stations, industries and signs. */

#ifndef MINI_MAP_MAP_LABELS_H
#define MINI_MAP_MAP_LABELS_H

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "../../core/geometry_type.hpp"
#include "../../gfx_type.h"
#include "../../industry_type.h"
#include "../../signs_type.h"
#include "../../station_type.h"
#include "../../town_type.h"
#include "../core/camera.h"

/* Listed in click priority: where plates overlap, the earlier kind wins. */
using LabelTarget = std::variant<SignID, StationID, IndustryID, TownID>;

/* Plates stand over their place in the world, show once a tile there spans enough pixels, give way to plates ranked above them,
 * hide behind hills, and fade in and out rather than pop. */
class MapLabels {
public:
	void Paint();
	std::optional<LabelTarget> HitAt(int x, int y) const;

private:
	/** Bands of rank, so every plate of one kind gives way to every plate of the kind before. */
	enum class LabelBand : uint8_t {
		Sign,
		Town,
		Station,
		Industry,
	};

	/* Plates that would overlap give way to the lower rank: the earlier band, then the lower order within it. */
	using LabelRank = std::pair<LabelBand, double>;

	/* A plate the world asks for. */
	struct Label {
		WorldPoint anchor;
		std::string text;
		uint32_t fill;
		bool transparent;
		LabelTarget target;
		double shown_from_pixels;
		double shown_until_pixels;
		LabelRank rank;
	};

	/* A plate drawn this frame, at the scale its distance gives it. */
	struct Shown {
		const Label *label;
		Rect area;
		double scale;
		double opacity;
	};

	struct Plate {
		Rect area;
		LabelTarget target;
	};

	std::vector<Label> Gather() const;
	std::optional<Rect> Area(const Label &label, double scale) const;
	void Draw(const Shown &shown);

	std::map<LabelTarget, double> opacities;
	std::vector<Plate> plates;
};

extern MapLabels _map_labels;

#endif /* MINI_MAP_MAP_LABELS_H */
