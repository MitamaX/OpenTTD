/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file network_pass.h The map's transport network in 3D: track, roads, bridges, tunnels and stations, and the signals showing their state. */

#ifndef MINI_WORLD_NETWORK_PASS_H
#define MINI_WORLD_NETWORK_PASS_H

#include <vector>

#include "../gpu/instanced_meshes.h"
#include "network_field.h"
#include "shader_program.h"
#include "signal_models.h"
#include "world_pass.h"

class NetworkPass final : public WorldPass {
public:
	NetworkPass();

	std::string_view Name() const override { return "network"; }
	void Reload() override;
	void Prepare(const SceneView &view) override;
	void Sync(const WorldChanges &changes) override;
	void Cast(const ShadowView &view) override;
	void Draw(const SceneView &view) override;
	void Release() override;

private:
	void DrawLayers(const ShaderProgram &program, NetworkBuffers NetworkChunk::*buffers) const;
	void DrawSignals();

	NetworkField field;
	std::vector<const NetworkChunk *> seen;
	std::vector<const NetworkChunk *> shown;
	InstancedMeshes signal_models;
	SignalBatch signals;
	ShaderProgram program;
	ShaderProgram caster;
	ShaderProgram span_program;
	ShaderProgram signal_program;
	ShaderProgram signal_caster;
};

#endif /* MINI_WORLD_NETWORK_PASS_H */
