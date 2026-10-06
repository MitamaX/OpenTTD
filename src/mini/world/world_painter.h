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
#include <optional>
#include <vector>

#include "../../vehicle_type.h"
#include "../ui/shader_painter.h"
#include "post_chain.h"
#include "scene_view.h"
#include "shadow_map.h"
#include "terrain_field.h"
#include "world_pass.h"
#include "world_target.h"
#include "world_textures.h"

class StructurePass;
class VehiclePass;

class WorldPainter final : public ShaderPainter {
public:
	static constexpr const char NAME[] = "world";

	WorldPainter();

	void Reload();
	void Prepare();
	void Paint(const ShaderArea &area) override;
	void Release() override;
	/* The tile of the building a sight line from the eye meets before it meets the ground. */
	std::optional<TileIndex> BuildingAt(const Vec3 &origin, const Vec3 &direction) const;
	/* The vehicle a sight line from the eye meets before it meets the ground; roofs and walls do not hide it from a click. */
	std::optional<VehicleID> VehicleAt(const Vec3 &origin, const Vec3 &direction) const;

private:
	bool Ready();
	void Render(const SceneView &view);
	void DrawStage(WorldStage stage, const SceneView &view);

	WorldTarget target;
	WorldTextures textures;
	TerrainField field;
	ShadowMap shadows;
	SceneUniforms scene;
	PostChain post;
	std::vector<std::unique_ptr<WorldPass>> passes;
	const StructurePass *structures = nullptr;
	const VehiclePass *vehicles = nullptr;
};

extern WorldPainter _world_painter;

#endif /* MINI_WORLD_WORLD_PAINTER_H */
