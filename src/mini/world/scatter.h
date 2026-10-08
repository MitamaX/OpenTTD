/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file scatter.h Small things strewn over the map near the eye, laid out from the map and drawn as copies of a few models. */

#ifndef MINI_WORLD_SCATTER_H
#define MINI_WORLD_SCATTER_H

#include "../map/world_tiles.h"
#include "scene_view.h"
#include "vehicle_models.h"

class Scatter {
public:
	virtual ~Scatter() = default;

	virtual void Sync(const WorldChanges &changes) = 0;
	/* Lays out afresh, within a slice of the frame, what the camera may show where a tile spans the shown pixels, or the shadows of where it spans the cast pixels,
	 * as far as the world changed there. */
	virtual void Prepare(const SceneView &camera, double shown_pixels, double cast_pixels) = 0;
	/* Adds the copies the view sees where a tile spans at least the fewest pixels, each in the bucket of the model it is drawn with. */
	virtual void Gather(const SceneView &view, const Frustum &frustum, double fewest_pixels, VehicleBatch &batch) = 0;
	virtual void Release() = 0;
};

#endif /* MINI_WORLD_SCATTER_H */
