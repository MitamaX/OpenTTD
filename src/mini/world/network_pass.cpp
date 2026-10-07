/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file network_pass.cpp The map's transport network in 3D: track, roads, bridges, tunnels and stations, and the signals showing their state. */

#include "../../stdafx.h"
#include "network_pass.h"

#include <array>

#include "../../rail_map.h"
#include "../../tile_map.h"
#include "../../track_func.h"
#include "../gpu/gl_api.h"
#include "shadow_map.h"

#include "../../safeguards.h"

static constexpr std::array<const char *, 2> VERTEX_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/solid.vert",
};
static constexpr std::array<const char *, 2> SPAN_VERTEX_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/span.vert",
};
static constexpr std::array<const char *, 2> SIGNAL_VERTEX_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/signal.vert",
};
static constexpr std::array<const char *, 9> FRAGMENT_SOURCES = {
	"mini_ui/shaders/scene.glsl",
	"mini_ui/shaders/noise.glsl",
	"mini_ui/shaders/common.glsl",
	"mini_ui/shaders/sky.glsl",
	"mini_ui/shaders/cloud_field.glsl",
	"mini_ui/shaders/shadow.glsl",
	"mini_ui/shaders/lighting.glsl",
	"mini_ui/shaders/overlay.glsl",
	"mini_ui/shaders/solid.frag",
};

/* Signals show only where a tile spans enough pixels for a post to read. */
static constexpr double SIGNAL_PIXELS = 10.0;
static constexpr double WAY_CASTER_WIDTH = 0.1;
/* Ways in the middle distance thin toward the ground under them, from whole at these tile pixels to this share gone where they fade, so the network reads as soft lines. */
static constexpr double NETWORK_RECEDE_PIXELS = 40.0;
static constexpr double NETWORK_RECEDE_DEPTH = 0.5;
static constexpr float OFFSET_SLOPE = -1.0f;
static constexpr float OFFSET_UNITS = -2.0f;

NetworkPass::NetworkPass() :
	program(VERTEX_SOURCES, FRAGMENT_SOURCES), caster(CasterProgram(VERTEX_SOURCES)), span_program(SPAN_VERTEX_SOURCES, FRAGMENT_SOURCES),
	signal_program(SIGNAL_VERTEX_SOURCES, FRAGMENT_SOURCES), signal_caster(CasterProgram(SIGNAL_VERTEX_SOURCES))
{
}

void NetworkPass::Reload()
{
	this->program.Reload();
	this->caster.Reload();
	this->span_program.Reload();
	this->signal_program.Reload();
	this->signal_caster.Reload();
}

/* A signal's state is read anew every frame, as trains pass without the world's texels changing. */
void NetworkPass::Prepare(const SceneView &view)
{
	this->field.Prepare(view, this->seen);
	this->signals.Clear(SIGNAL_MODELS);
	for (const NetworkChunk *chunk : this->seen) {
		if (chunk->nearest_pixels < SIGNAL_PIXELS) continue;
		for (const SignalSpot &spot : chunk->signals) {
			if (!IsTileType(spot.tile, MP_RAILWAY) || !HasSignals(spot.tile) || !HasSignalOnTrackdir(spot.tile, spot.trackdir)) continue;
			SignalVariant variant = GetSignalVariant(spot.tile, TrackdirToTrack(spot.trackdir));
			SignalState state = GetSignalStateByTrackdir(spot.tile, spot.trackdir);
			this->signals.Add(SignalModelIndex(variant, state), {static_cast<float>(spot.at.x), static_cast<float>(spot.at.y), static_cast<float>(spot.at.z), static_cast<float>(spot.facing)});
		}
	}
}

void NetworkPass::Sync(const WorldChanges &changes)
{
	this->field.Sync(changes);
}

void NetworkPass::Cast(const ShadowView &view)
{
	if (!this->caster.Ready() || !this->signal_caster.Ready() || !view.Resolves(WAY_CASTER_WIDTH)) return;
	this->field.Gather(view.camera, view.frustum, this->shown);
	this->caster.Use();
	this->DrawLayers(this->caster, &NetworkChunk::layers);
	this->DrawLayers(this->caster, &NetworkChunk::spans);
	this->signal_caster.Use();
	this->DrawSignals();
}

/* Ways lie on the ground they follow, so they are drawn a little toward the eye to win where their faces meet it; bridges' spans stand clear of it and never sink into it. */
void NetworkPass::Draw(const SceneView &view)
{
	if (!this->program.Ready() || !this->span_program.Ready() || !this->signal_program.Ready()) return;
	this->field.Gather(view, view.frustum, this->shown);
	for (const ShaderProgram *solid : {&this->program, &this->span_program, &this->signal_program}) {
		solid->Use();
		UploadOverlay(*solid);
		glUniform2f(solid->Uniform("u_fade"), static_cast<float>(NETWORK_FADE_START), static_cast<float>(NETWORK_FADE_END));
		glUniform2f(solid->Uniform("u_recede"), static_cast<float>(NETWORK_RECEDE_PIXELS), solid == &this->span_program ? 0.0f : static_cast<float>(NETWORK_RECEDE_DEPTH));
	}

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(OFFSET_SLOPE, OFFSET_UNITS);
	this->program.Use();
	this->DrawLayers(this->program, &NetworkChunk::layers);
	this->span_program.Use();
	this->DrawLayers(this->span_program, &NetworkChunk::spans);
	this->signal_program.Use();
	glUniform1i(this->signal_program.Uniform("u_way"), to_underlying(MiniLayer::Rail));
	this->DrawSignals();
	glDisable(GL_POLYGON_OFFSET_FILL);
}

void NetworkPass::DrawLayers(const ShaderProgram &program, NetworkBuffers NetworkChunk::*buffers) const
{
	int way = program.Uniform("u_way");
	for (size_t layer = 0; layer < NETWORK_LAYERS; layer++) {
		glUniform1i(way, static_cast<GLint>(layer));
		for (const NetworkChunk *chunk : this->shown) (chunk->*buffers)[layer].Draw();
	}
}

void NetworkPass::DrawSignals()
{
	if (!this->signal_models.Ready()) {
		std::vector<ModelMesh> models = BuildSignalModels();
		this->signal_models.Upload<ModelMesh>(models, MODEL_LAYOUT, SIGNAL_INSTANCE_LAYOUT, sizeof(SignalInstance));
	}
	this->signals.Draw(this->signal_models, [](size_t mesh) { return mesh; });
}

void NetworkPass::Release()
{
	this->field.Release();
	this->signal_models.Release();
	this->program.Release();
	this->caster.Release();
	this->span_program.Release();
	this->signal_program.Release();
	this->signal_caster.Release();
}
