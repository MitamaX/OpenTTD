/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file world_textures.cpp The world's tile texels on the GPU, one texel per tile, for any world shader to read. */

#include "../../stdafx.h"
#include "world_textures.h"

#include "../map/way_bends.h"
#include "frame_units.h"

#include "../../safeguards.h"

static_assert(WorldTextures::UNIT_COUNT <= SHADOW_UNIT);

static constexpr std::array<const char *, WorldTextures::UNIT_COUNT> SAMPLERS = {"u_tiles", "u_water", "u_surfaces", "u_network", "u_bends", "u_shore"};

WorldTextures::Source WorldTextures::SourceOf(uint unit) const
{
	switch (unit) {
		case TILES_UNIT: return {TexelFormat::ExactRgba, _world_tiles.Ground()};
		case WATER_UNIT: return {TexelFormat::FilteredRgba, _world_tiles.Water()};
		case SURFACES_UNIT: return {TexelFormat::ExactRgba, _world_tiles.Surfaces()};
		case NETWORK_UNIT: return {TexelFormat::ExactRgba, _world_tiles.Network()};
		case BENDS_UNIT: return {TexelFormat::ExactRgba, _way_bends.Texels()};
		case SHORE_UNIT: return {TexelFormat::FilteredRed, this->shore.Texels()};
		default: NOT_REACHED();
	}
}

void WorldTextures::Allocate(uint unit)
{
	Source source = this->SourceOf(unit);
	this->textures[unit].Allocate(source.format, _world_tiles.Size(), source.texels);
}

/* The shore's distances reach across the whole map, so any change of water measures them all again. */
void WorldTextures::Sync(const WorldChanges &changes)
{
	bool whole = changes.whole || !this->textures[TILES_UNIT].Allocated();
	if (whole || changes.water) this->shore.Survey(_world_tiles.Size());
	if (whole) {
		for (uint unit = 0; unit < UNIT_COUNT; unit++) this->Allocate(unit);
	} else {
		for (uint unit = 0; unit < SHORE_UNIT; unit++) {
			for (const Rect &area : changes.areas) this->textures[unit].Update(area, this->SourceOf(unit).texels);
		}
		if (changes.water) {
			this->textures[WATER_UNIT].Refilter();
			this->Allocate(SHORE_UNIT);
		}
	}
	if (std::optional<Rect> bent = _way_bends.TakeUpdate(); bent.has_value()) this->textures[BENDS_UNIT].Update(*bent, _way_bends.Texels());
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
	for (uint unit = 0; unit < UNIT_COUNT; unit++) program.BindSampler(SAMPLERS[unit], unit);
}
