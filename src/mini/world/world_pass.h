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

/** When a pass draws: solid passes first, then surface passes over a snapshot of what the solid ones drew. */
enum class WorldStage : uint8_t {
	Solid,
	Surface,
};

/* A pass draws into the bound world target with depth testing on; the scene and shadow blocks are bound and every pass gets each frame's world changes.
 * A pass whose shading is dear may lay its depth in Lay, before any pass draws, so it shades only what stays in sight; it then draws its colour with depth tested equal or nearer.
 * A pass that casts shadows draws its meshes depth only in Cast, through a program made by CasterProgram from its own vertex sources.
 * Prepare runs while the game's state holds still, before the frame is drawn; a pass that reads the game's map does so there and nowhere else.
 * Every program a pass draws with is built before the first frame, so none is built in the frame it first comes into view. */
class WorldPass {
public:
	virtual ~WorldPass() = default;

	virtual std::string_view Name() const = 0;
	virtual std::vector<ShaderProgram *> Programs() = 0;
	virtual void Prepare([[maybe_unused]] const SceneView &view) {}
	virtual void Sync([[maybe_unused]] const WorldChanges &changes) {}
	virtual void Cast([[maybe_unused]] const ShadowView &view) {}
	virtual void Lay([[maybe_unused]] const SceneView &view) {}
	virtual void Draw(const SceneView &view) = 0;
	virtual void Release() = 0;
	virtual WorldStage Stage() const { return WorldStage::Solid; }
};

#endif /* MINI_WORLD_WORLD_PASS_H */
