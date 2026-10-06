/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ground_overlay.h Shapes laid on the world's ground, drawn over the finished world against its depth, so what stands in front of them hides them. */

#ifndef MINI_WORLD_GROUND_OVERLAY_H
#define MINI_WORLD_GROUND_OVERLAY_H

#include "../gpu/draw_list.h"
#include "../gpu/mesh_buffer.h"
#include "shader_program.h"
#include "world_target.h"

class GroundOverlay {
public:
	GroundOverlay();

	void Reload() { this->program.Reload(); }
	bool Ready() { return this->program.Ready(); }
	void Release();

	void Upload(const DrawList &list);
	void Draw(const WorldTarget &target) const;

private:
	ShaderProgram program;
	MeshBuffer mesh;
};

#endif /* MINI_WORLD_GROUND_OVERLAY_H */
