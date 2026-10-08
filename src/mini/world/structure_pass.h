/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file structure_pass.h The map's buildings in 3D: houses, industries, objects, depots and the buildings of stations, lit and casting shadows. */

#ifndef MINI_WORLD_STRUCTURE_PASS_H
#define MINI_WORLD_STRUCTURE_PASS_H

#include <optional>
#include <span>
#include <vector>

#include "shader_program.h"
#include "structure_field.h"
#include "world_pass.h"

class StructurePass final : public WorldPass {
public:
	StructurePass();

	std::string_view Name() const override { return "structures"; }
	std::vector<ShaderProgram *> Programs() override;
	void Prepare(const SceneView &view) override;
	void Sync(const WorldChanges &changes) override;
	void Cast(const ShadowView &view) override;
	void Draw(const SceneView &view) override;
	void Release() override;

	std::optional<StructureHit> Pick(const Vec3 &origin, const Vec3 &direction) const { return this->field.Pick(origin, direction); }
	/* The blocks the last frame drew. */
	std::span<const StructureChunk *const> Shown() const { return this->shown; }

private:
	void DrawChunks() const;

	StructureField field;
	std::vector<const StructureChunk *> shown;
	ShaderProgram program;
	ShaderProgram caster;
};

#endif /* MINI_WORLD_STRUCTURE_PASS_H */
