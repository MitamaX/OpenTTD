/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file water_pass.h The water's surface, drawn over a snapshot of the solid world it reflects the sky over and lets the ground show through. */

#ifndef MINI_WORLD_WATER_PASS_H
#define MINI_WORLD_WATER_PASS_H

#include "shader_program.h"
#include "terrain_field.h"
#include "world_pass.h"
#include "world_textures.h"

class WaterPass final : public WorldPass {
public:
	WaterPass(const WorldTextures &textures, TerrainField &field);

	void Reload() override;
	void Draw(const SceneView &view) override;
	void Release() override;
	WorldStage Stage() const override { return WorldStage::Surface; }

private:
	const WorldTextures &textures;
	TerrainField &field;
	ShaderProgram program;
};

#endif /* MINI_WORLD_WATER_PASS_H */
