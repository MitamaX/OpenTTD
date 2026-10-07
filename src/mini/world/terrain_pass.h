/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file terrain_pass.h The map's ground, shaded from the world's texels and casting the sun's shadows. */

#ifndef MINI_WORLD_TERRAIN_PASS_H
#define MINI_WORLD_TERRAIN_PASS_H

#include "ground_detail.h"
#include "shader_program.h"
#include "terrain_field.h"
#include "world_pass.h"
#include "world_textures.h"

class TerrainPass final : public WorldPass {
public:
	TerrainPass(const WorldTextures &textures, TerrainField &field);

	void Reload() override;
	void Cast(const ShadowView &view) override;
	void Draw(const SceneView &view) override;
	void Release() override;

private:
	void Configure() const;

	const WorldTextures &textures;
	TerrainField &field;
	GroundDetail detail;
	ShaderProgram program;
	ShaderProgram caster;
};

#endif /* MINI_WORLD_TERRAIN_PASS_H */
