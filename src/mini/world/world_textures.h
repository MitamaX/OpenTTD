/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file world_textures.h The world's tile texels on the GPU, one texel per tile, for any world shader to read. */

#ifndef MINI_WORLD_WORLD_TEXTURES_H
#define MINI_WORLD_WORLD_TEXTURES_H

#include <array>
#include <span>
#include <vector>

#include "../gpu/data_texture.h"
#include "../map/world_tiles.h"
#include "flow_field.h"
#include "massing_field.h"
#include "shader_program.h"

class WorldTextures {
public:
	/** Each texture is bound on the texture unit of its value. */
	enum Unit : uint8_t {
		TILES_UNIT,
		WATER_UNIT,
		SURFACES_UNIT,
		NETWORK_UNIT,
		BENDS_UNIT,
		FLOW_UNIT,
		MASSING_UNIT,
		UNIT_COUNT,
	};

	void Sync(const WorldChanges &changes);
	void Bind() const;
	void Release();

	MassingField &Massing() { return this->massing; }

	static void BindSamplers(const ShaderProgram &program);

private:
	/* A texture's texels, and the kinds of change they follow; the bends and the flow keep track of their own changes. */
	struct Source {
		TexelFormat format;
		const void *texels;
		ChangeKinds kinds;
	};

	struct ChangedSpan {
		Rect area;
		int64_t changed;
	};

	Source SourceOf(uint unit) const;
	void Allocate(uint unit);
	void Take(const Rect &changed);
	void Coalesce();
	void SendSpans(uint unit, const void *texels);
	void ResurveyFlow(std::span<const Rect> water_moves);

	std::array<DataTexture, UNIT_COUNT> textures;
	FlowField flow;
	MassingField massing;
	std::vector<ChangedSpan> spans;
};

#endif /* MINI_WORLD_WORLD_TEXTURES_H */
