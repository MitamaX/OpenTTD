/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file street_walkers.h People strolling along town pavements, laid out from the map's own roads and moved every frame; they only ever look, never live. */

#ifndef MINI_WORLD_STREET_WALKERS_H
#define MINI_WORLD_STREET_WALKERS_H

#include <array>
#include <vector>

#include "figure_models.h"
#include "scatter.h"
#include "strewn_blocks.h"

/* A walker paces up and down a stretch of pavement, from its start along a heading for a length, or stands still where its pace is nothing. */
struct Walker {
	float x;
	float y;
	float along_x;
	float along_y;
	float length;
	float phase;
	float pace;
	float floor;
	std::array<uint8_t, 4> shirt;
	std::array<uint8_t, 4> trousers;
	uint8_t build;
};

/* A walker as it stands this frame: its copy, the pose and build it is drawn in and how many pixels a tile spans where it stands. */
struct Stroll {
	VehicleInstance instance;
	FigurePose pose;
	uint8_t build;
	double tile_pixels;
};

/* Walkers on town pavements and rail station platforms. */
class StreetWalkers final : public Scatter {
public:
	StreetWalkers();

	void Sync(const WorldChanges &changes) override;
	void Refresh(const SceneView &camera, double shown_pixels, double cast_pixels) override;
	/* Each walker is gathered in the bucket of its build and pose. */
	void Gather(const Sight &sight, double fewest_pixels, VehicleBatch &batch) override;
	void Release() override;

private:
	/* A block's walkers, and where they stood in the frame they were last moved in. */
	struct Pavement {
		std::vector<Walker> walkers;
		std::vector<Stroll> strolls;
		uint64_t moved = 0;
	};

	static void Build(const TileSpan &tiles, Pavement &pavement);
	void Move(const SceneView &view, Pavement &pavement) const;

	StrewnBlocks<Pavement> blocks;
	uint64_t frame = 0;
};

#endif /* MINI_WORLD_STREET_WALKERS_H */
