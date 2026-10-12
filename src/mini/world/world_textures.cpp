/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file world_textures.cpp The world's tile texels on the GPU, one texel per tile, for any world shader to read. */

#include "../../stdafx.h"
#include "world_textures.h"

#include <algorithm>
#include <iterator>

#include "../gpu/frame_profile.h"
#include "../map/tile_shapes.h"
#include "../map/way_bends.h"
#include "frame_units.h"
#include "rect_spans.h"

#include "../../safeguards.h"

static constexpr std::array<const char *, WorldTextures::UNIT_COUNT> SAMPLERS = {"u_tiles", "u_water", "u_surfaces", "u_network", "u_bends", "u_flow", "u_massing", "u_sky_field"};
/* Two spans go up as one where the area bounding both is at most this many texels, or MOST_JOINED_SPREAD times what they cover. */
static constexpr int64_t FREELY_JOINED_TEXELS = 128 * 128;
/* The kinds of change that move where water lies. */
static constexpr ChangeKinds WATER_MOVES = {ChangeKind::Water, ChangeKind::Shape};

WorldTextures::Source WorldTextures::SourceOf(uint unit) const
{
	switch (unit) {
		case TILES_UNIT: return {TexelFormat::ExactRgba, _world_tiles.Ground(), {ChangeKind::Cover, ChangeKind::Flora}};
		case WATER_UNIT: return {TexelFormat::FilteredRgba, _world_tiles.Water(), ChangeKind::Water};
		case SURFACES_UNIT: return {TexelFormat::ExactRgba, _world_tiles.Surfaces(), ChangeKind::Shape};
		case NETWORK_UNIT: return {TexelFormat::ExactRgba, _world_tiles.Network(), ChangeKind::Ways};
		case BENDS_UNIT: return {TexelFormat::ExactRgba, _way_bends.Texels(), {}};
		case FLOW_UNIT: return {TexelFormat::FilteredRg, this->flow.Texels(), {}};
		case MASSING_UNIT: return {TexelFormat::ExactRgba16, this->massing.Texels(), {}};
		case SKY_UNIT: return {TexelFormat::ExactRgba32, this->sky.Texels(), {}};
		default: NOT_REACHED();
	}
}

void WorldTextures::Allocate(uint unit)
{
	Source source = this->SourceOf(unit);
	this->textures[unit].Allocate(source.format, _world_tiles.Size(), source.texels);
}

void WorldTextures::Sync(const WorldChanges &changes)
{
	if (changes.whole || !this->textures[TILES_UNIT].Allocated()) {
		{
			ProfileScope profile("build", "flow", ProfileClock::Cpu);
			this->flow.Survey(_world_tiles.Size());
		}
		this->massing.Clear(_world_tiles.Size());
		this->sky.Clear(_world_tiles.Size());
		for (uint unit = 0; unit < UNIT_COUNT; unit++) this->Allocate(unit);
	} else {
		for (uint unit = 0; unit < UNIT_COUNT; unit++) {
			Source source = this->SourceOf(unit);
			this->spans.clear();
			for (ChangeKind kind : source.kinds) {
				for (const Rect &changed : changes.Of(kind)) this->Take(changed);
			}
			this->SendSpans(unit, source.texels);
		}
		this->spans.clear();
		std::vector<Rect> massing_moves = this->massing.TakeMoved();
		for (const Rect &moved : massing_moves) this->Take(moved);
		this->SendSpans(MASSING_UNIT, this->massing.Texels());
		this->ResurveySky(massing_moves, changes.Of(ChangeKind::Shape));
		std::vector<Rect> water_moves;
		for (ChangeKind kind : WATER_MOVES) std::ranges::copy(changes.Of(kind), std::back_inserter(water_moves));
		if (!water_moves.empty()) this->ResurveyFlow(water_moves);
	}
	if (std::optional<Rect> bent = _way_bends.TakeUpdate(); bent.has_value()) this->textures[BENDS_UNIT].Update(*bent, _way_bends.Texels());
}

void WorldTextures::ResurveyFlow(std::span<const Rect> water_moves)
{
	ProfileScope profile("build", "flow", ProfileClock::Cpu);
	this->spans.clear();
	for (const Rect &changed : this->flow.Resurvey(water_moves)) this->Take(changed);
	this->SendSpans(FLOW_UNIT, this->flow.Texels());
}

void WorldTextures::ResurveySky(std::span<const Rect> massing_moves, std::span<const Rect> shape_moves)
{
	std::vector<Rect> moves;
	std::ranges::copy(massing_moves, std::back_inserter(moves));
	std::ranges::copy(shape_moves, std::back_inserter(moves));
	if (moves.empty()) return;

	ProfileScope profile("build", "sky", ProfileClock::Cpu);
	this->spans.clear();
	for (const Rect &changed : this->sky.Resurvey(moves, this->massing)) this->Take(changed);
	this->SendSpans(SKY_UNIT, this->sky.Texels());
}

void WorldTextures::Take(const Rect &changed)
{
	this->spans.push_back({changed, AreaOf(changed)});
}

/* Spans lying over or close by one another are joined, so a texture is sent each area once a frame and in as few pieces as it takes. */
void WorldTextures::Coalesce()
{
	for (size_t kept = 0; kept < this->spans.size(); kept++) {
		for (size_t other = kept + 1; other < this->spans.size();) {
			Rect joined = Enclosing(this->spans[kept].area, this->spans[other].area);
			int64_t changed = this->spans[kept].changed + this->spans[other].changed;
			if (AreaOf(joined) > std::max(FREELY_JOINED_TEXELS, MOST_JOINED_SPREAD * changed)) {
				other++;
				continue;
			}
			this->spans[kept] = {joined, changed};
			this->spans.erase(this->spans.begin() + static_cast<std::ptrdiff_t>(other));
			other = kept + 1;
		}
	}
}

void WorldTextures::SendSpans(uint unit, const void *texels)
{
	this->Coalesce();
	for (const ChangedSpan &span : this->spans) this->textures[unit].Update(span.area, texels);
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
