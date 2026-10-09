/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file world_pass.h One stage of drawing the 3D world, run in the order the world painter holds its passes. */

#ifndef MINI_WORLD_WORLD_PASS_H
#define MINI_WORLD_WORLD_PASS_H

#include <string_view>
#include <vector>

#include "../map/world_tiles.h"
#include "scene_view.h"
#include "shader_program.h"

/** When a pass draws: solid passes first, then surface passes over a snapshot of what the solid ones drew, and translucent passes last, over the open claddings. */
enum class WorldStage : uint8_t {
	Solid,
	Surface,
	Translucent,
};

/* A pass draws into the bound world target with depth testing on and the scene and shadow blocks bound. */
class WorldPass {
public:
	virtual ~WorldPass() = default;

	virtual std::string_view Name() const = 0;
	/* Every program and, by Load, every model and texture a pass draws with is built before the first frame, so none is built in the frame it first comes into view. */
	virtual std::vector<ShaderProgram *> Programs() = 0;
	virtual void Load() {}
	/* Runs while the game's state holds still, the only place a pass reads the game's map. */
	virtual void Prepare([[maybe_unused]] const SceneView &view) {}
	virtual void Sync([[maybe_unused]] const WorldChanges &changes) {}
	/* Runs once the game goes on again after Sync, building what a pass builds from the world's tiles alone. */
	virtual void Refresh([[maybe_unused]] const SceneView &view) {}
	/* Draws depth only into a shadow cascade, through a program CasterProgram made from the pass's own vertex sources. */
	virtual void Cast([[maybe_unused]] const ShadowView &view) {}
	/* A pass whose shading is dear lays its depth before any pass draws, then draws its colour with depth tested equal or nearer, shading only what stays in sight. */
	virtual void Lay([[maybe_unused]] const SceneView &view) {}
	virtual void Draw(const SceneView &view) = 0;
	/* Open claddings, whose gaps screen noise cuts, draw once the occlusion is found from the depth, so their gaps never pock what it reads. */
	virtual void DrawOpen([[maybe_unused]] const SceneView &view) {}
	/* A program that draws only what is in sight draws once with nothing in sight, so the driver readies it for its target before it first shows anything. */
	virtual void WarmCast() {}
	virtual void Warm() {}
	virtual void Release() = 0;
	virtual WorldStage Stage() const { return WorldStage::Solid; }
};

#endif /* MINI_WORLD_WORLD_PASS_H */
