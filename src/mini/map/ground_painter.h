/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ground_painter.h The map's ground, drawn by one shader from the world's texels. */

#ifndef MINI_MAP_GROUND_PAINTER_H
#define MINI_MAP_GROUND_PAINTER_H

#include <array>

#include "../gpu/data_texture.h"
#include "../gpu/gl_program.h"
#include "../ui/shader_painter.h"

class GroundPainter final : public ShaderPainter {
public:
	static constexpr const char NAME[] = "ground";

	void Reload();
	void Paint(const ShaderArea &area) override;
	void Release() override;

private:
	/** The world's data textures, each bound on the texture unit of its value. */
	enum Texture : uint8_t {
		TILES_TEXTURE,
		WATER_TEXTURE,
		SURFACES_TEXTURE,
		NETWORK_TEXTURE,
		TEXTURE_COUNT,
	};

	struct TextureSource {
		const char *sampler;
		TexelFormat format;
		const void *texels;
	};

	struct Uniforms {
		int area;
		int viewport;
		int centre;
		int plane;
		int ppt;
		int right;
		int toward;
		int sun;
		int peak;
		int map;
		int time;
		int landscape;
		int relief;
		int contour;
		int grid;
		int layer;
		int sink;
	};

	static TextureSource SourceOf(uint unit);

	bool Ready();
	void Build();
	void Upload();
	void Configure(const ShaderArea &area) const;

	GlProgram program;
	Uniforms uniforms{};
	std::array<DataTexture, TEXTURE_COUNT> textures;
	uint32_t vertex_array = 0;
	bool attempted = false;
};

extern GroundPainter _ground_painter;

#endif /* MINI_MAP_GROUND_PAINTER_H */
