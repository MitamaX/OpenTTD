/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file frame_units.h The texture units the frame's shared textures stay bound on while the world is drawn. */

#ifndef MINI_WORLD_FRAME_UNITS_H
#define MINI_WORLD_FRAME_UNITS_H

#include <array>

/** Units above the world's tile textures; every world shader finds these samplers there. */
enum FrameUnit : uint8_t {
	SHADOW_UNIT = 4,
	SCENE_COLOUR_UNIT,
	SCENE_DEPTH_UNIT,
};

struct FrameSampler {
	const char *name;
	FrameUnit unit;
};

inline constexpr std::array<FrameSampler, 3> FRAME_SAMPLERS = {{
	{"u_shadow_map", SHADOW_UNIT},
	{"u_scene_colour", SCENE_COLOUR_UNIT},
	{"u_scene_depth", SCENE_DEPTH_UNIT},
}};

inline constexpr uint32_t SHADOWS_BINDING = 1;

#endif /* MINI_WORLD_FRAME_UNITS_H */
