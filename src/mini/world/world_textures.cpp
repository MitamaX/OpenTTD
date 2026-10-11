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

#include "../../safeguards.h"

static constexpr std::array<const char *, WorldTextures::UNIT_COUNT> SAMPLERS = {"u_tiles", "u_water", "u_surfaces", "u_network", "u_bends", "u_shore", "u_flow"};
/* Two spans go up as one where the area bounding both is at most this many times what they cover, or at most this many texels. */
static constexpr int64_t MOST_JOINED_SPREAD = 2;
static constexpr int64_t FREELY_JOINED_TEXELS = 128 * 128;
/* The kinds of change that move where water lies. */
static constexpr ChangeKinds WATER_MOVES = {ChangeKind::Water, ChangeKind::Shape};

static int64_t Area(const Rect &span)
{
	return static_cast<int64_t>(span.Width()) * span.Height();
}

static Rect Joined(const Rect &a, const Rect &b)
{
	return TakingIn(TakingIn(a, b.left, b.top), b.right, b.bottom);
}

struct Sent {
	Rect area;
	int64_t taken;
};

/* Spans lying over or close by one another are joined, so a texture is sent each area once a frame and in as few pieces as it takes. */
static void Coalesce(std::vector<Rect> &spans)
{
	std::vector<Sent> sent;
	for (const Rect &span : spans) sent.push_back({span, Area(span)});
	for (size_t kept = 0; kept < sent.size(); kept++) {
		for (size_t other = kept + 1; other < sent.size();) {
			Rect joined = Joined(sent[kept].area, sent[other].area);
			if (Area(joined) > std::max(FREELY_JOINED_TEXELS, MOST_JOINED_SPREAD * (sent[kept].taken + sent[other].taken))) {
				other++;
				continue;
			}
			sent[kept] = {joined, sent[kept].taken + sent[other].taken};
			sent.erase(sent.begin() + static_cast<std::ptrdiff_t>(other));
			other = kept + 1;
		}
	}
	spans.clear();
	for (const Sent &span : sent) spans.push_back(span.area);
}

WorldTextures::Source WorldTextures::SourceOf(uint unit) const
{
	switch (unit) {
		case TILES_UNIT: return {TexelFormat::ExactRgba, _world_tiles.Ground(), {ChangeKind::Cover, ChangeKind::Flora}};
		case WATER_UNIT: return {TexelFormat::FilteredRgba, _world_tiles.Water(), ChangeKind::Water};
		case SURFACES_UNIT: return {TexelFormat::ExactRgba, _world_tiles.Surfaces(), ChangeKind::Shape};
		case NETWORK_UNIT: return {TexelFormat::ExactRgba, _world_tiles.Network(), ChangeKind::Ways};
		case BENDS_UNIT: return {TexelFormat::ExactRgba, _way_bends.Texels(), {}};
		case SHORE_UNIT: return {TexelFormat::FilteredRed, this->shore.Texels(), {}};
		case FLOW_UNIT: return {TexelFormat::FilteredRg, this->flow.Texels(), {}};
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
		_frame_profile.Count("shore_surveys");
		{
			ProfileScope profile("build", "shore", ProfileClock::Cpu);
			this->shore.Survey(_world_tiles.Size());
		}
		{
			ProfileScope profile("build", "flow", ProfileClock::Cpu);
			this->flow.Survey(_world_tiles.Size());
		}
		for (uint unit = 0; unit < UNIT_COUNT; unit++) this->Allocate(unit);
	} else {
		for (uint unit = 0; unit < UNIT_COUNT; unit++) {
			Source source = this->SourceOf(unit);
			this->spans.clear();
			for (ChangeKind kind : source.kinds) std::ranges::copy(changes.Of(kind), std::back_inserter(this->spans));
			this->SendSpans(unit, source.texels);
		}
		std::vector<Rect> water_moves;
		for (ChangeKind kind : WATER_MOVES) std::ranges::copy(changes.Of(kind), std::back_inserter(water_moves));
		if (!water_moves.empty()) {
			this->ResurveyShore(water_moves);
			this->ResurveyFlow(water_moves);
		}
	}
	if (std::optional<Rect> bent = _way_bends.TakeUpdate(); bent.has_value()) this->textures[BENDS_UNIT].Update(*bent, _way_bends.Texels());
}

/* The shore's distances move only out to the shelf's edge about a tile whose water or shape changed, so only there are they measured again and sent up. */
void WorldTextures::ResurveyShore(std::span<const Rect> water_moves)
{
	ProfileScope profile("build", "shore", ProfileClock::Cpu);
	this->spans.clear();
	for (const Rect &span : water_moves) {
		_frame_profile.Count("shore_surveys");
		std::optional<Rect> shifted = this->shore.Resurvey(span);
		if (shifted.has_value()) this->spans.push_back(*shifted);
	}
	this->SendSpans(SHORE_UNIT, this->shore.Texels());
}

void WorldTextures::ResurveyFlow(std::span<const Rect> water_moves)
{
	ProfileScope profile("build", "flow", ProfileClock::Cpu);
	this->spans = this->flow.Resurvey(water_moves);
	this->SendSpans(FLOW_UNIT, this->flow.Texels());
}

void WorldTextures::SendSpans(uint unit, const void *texels)
{
	Coalesce(this->spans);
	for (const Rect &span : this->spans) this->textures[unit].Update(span, texels);
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
