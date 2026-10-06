/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file world_painter.h The map's 3D world: its passes drawn into a target of its own, then laid into RmlUi's layer under the map element. */

#ifndef MINI_WORLD_WORLD_PAINTER_H
#define MINI_WORLD_WORLD_PAINTER_H

#include <memory>
#include <vector>

#include "../ui/shader_painter.h"
#include "scene_view.h"
#include "shader_program.h"
#include "shadow_map.h"
#include "terrain_field.h"
#include "world_pass.h"
#include "world_target.h"
#include "world_textures.h"

class WorldPainter final : public ShaderPainter {
public:
	static constexpr const char NAME[] = "world";

	WorldPainter();

	void Reload();
	void Paint(const ShaderArea &area) override;
	void Release() override;

private:
	bool Ready();
	void Render(const SceneView &view);
	void DrawStage(WorldStage stage, const SceneView &view);
	void Composite(const ShaderArea &area);

	WorldTarget target;
	WorldTextures textures;
	TerrainField field;
	ShadowMap shadows;
	SceneUniforms scene;
	ShaderProgram composite;
	std::vector<std::unique_ptr<WorldPass>> passes;
	uint32_t quad = 0;
};

extern WorldPainter _world_painter;

#endif /* MINI_WORLD_WORLD_PAINTER_H */
