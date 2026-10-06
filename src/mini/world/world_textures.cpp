/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file world_textures.cpp The world's tile texels on the GPU, one texel per tile, for any world shader to read. */

#include "../../stdafx.h"
#include "world_textures.h"

#include "../../safeguards.h"

WorldTextures::Source WorldTextures::SourceOf(uint unit)
{
	switch (unit) {
		case TILES_UNIT: return {"u_tiles", TexelFormat::ExactRgba, _world_tiles.Ground()};
		case WATER_UNIT: return {"u_water", TexelFormat::FilteredRgba, _world_tiles.Water()};
		case SURFACES_UNIT: return {"u_surfaces", TexelFormat::ExactRgba, _world_tiles.Surfaces()};
		case NETWORK_UNIT: return {"u_network", TexelFormat::ExactRgba, _world_tiles.Network()};
		default: NOT_REACHED();
	}
}

void WorldTextures::Sync(const WorldChanges &changes)
{
	bool whole = changes.whole || !this->textures[TILES_UNIT].Allocated();
	for (uint unit = 0; unit < this->textures.size(); unit++) {
		Source source = SourceOf(unit);
		DataTexture &texture = this->textures[unit];
		if (whole) {
			texture.Allocate(source.format, _world_tiles.Size(), source.texels);
		} else {
			for (const Rect &area : changes.areas) texture.Update(area, source.texels);
		}
	}
	if (!whole && changes.water) this->textures[WATER_UNIT].Refilter();
}

void WorldTextures::Bind() const
{
	for (uint unit = 0; unit < this->textures.size(); unit++) this->textures[unit].Bind(unit);
}

void WorldTextures::Release()
{
	for (DataTexture &texture : this->textures) texture.Release();
}

void WorldTextures::BindSamplers(const ShaderProgram &program)
{
	program.Use();
	for (uint unit = 0; unit < UNIT_COUNT; unit++) program.BindSampler(SourceOf(unit).sampler, unit);
}
