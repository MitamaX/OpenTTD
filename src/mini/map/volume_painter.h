/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file volume_painter.h Buildings and the steps between tiles drawn as lit, textured solids standing on the map. */

#ifndef MINI_MAP_VOLUME_PAINTER_H
#define MINI_MAP_VOLUME_PAINTER_H

#include <optional>
#include <vector>

#include "../../core/geometry_type.hpp"
#include "../../tile_type.h"
#include "../core/camera.h"
#include "../gpu/draw_list.h"
#include "building_form.h"
#include "tile_shapes.h"
#include "volume_light.h"

inline constexpr double STRUCTURE_RISE_LEVELS = MAX_STRUCTURE_TILES * RISE_SCALE / LEVEL_TILES;

enum class BuildingTier : uint8_t { Massing, Shaped, Textured };

struct VolumeStyle {
	std::optional<uint32_t> accent;
};

class VolumePainter {
public:
	void BeginFrame();
	bool MayShow(TileIndex tile) const;
	void Draw(const BuildingForm &form, TileIndex cell, const VolumeStyle &style);
	void DrawStep(TileIndex tile, const StepFace &step) const;
	std::optional<TileIndex> PickAt(Point screen) const;

private:
	class CoatPainter;
	class SolidPainter;

	struct Pick {
		Rect bounds;
		TileIndex tile;
	};

	bool FacesViewer(const Vec3 &unit_normal) const;
	SolidPlacement PlacementAt(const PlanRect &footprint, double floor) const;
	SolidPlacement PlacementOf(const BuildingForm &form) const;
	std::optional<Rect> PieceBounds(const BuildingForm &form, const PlanRect &cell) const;
	bool ShowsDetail(const VolumeStyle &style) const;
	void DrawSolid(const BuildingForm &form, const Solid &solid, const SolidPlacement &placement, const PlanRect &cell, const VolumeStyle &style) const;

	double ppt = MIN_PPT;
	BuildingTier tier = BuildingTier::Massing;
	Daylight daylight{};
	MapVector toward{};
	Vec3 view{};
	double glazing = 0.0;
	std::optional<uint> snow_line;
	SolidTexel atlas{};
	std::vector<Pick> picks;
};

extern VolumePainter _volume_painter;
TileSpan StructureSurvey(const TileSpan &visible);

#endif /* MINI_MAP_VOLUME_PAINTER_H */
