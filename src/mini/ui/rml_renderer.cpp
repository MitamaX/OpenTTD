/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rml_renderer.cpp The upstream GL3 renderer, with the game's screen and the mini UI's own textures as more image sources. */

#include "../../stdafx.h"
#include "rml_renderer.h"

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Dictionary.h>

#include "../../core/format.hpp"
#include "../../core/string_consumer.hpp"
#include "../gpu/gpu_frame.h"

#include "../../safeguards.h"

static constexpr std::string_view TEXTURE_PREFIX = "?texture/";
static constexpr const char SHADER_DECORATOR[] = "shader";

RmlRenderer::~RmlRenderer()
{
	ReleaseShaderPainters();
}

Rml::String RmlRenderer::TextureSource(TextureId texture)
{
	return fmt::format("{}{}", TEXTURE_PREFIX, texture);
}

/* The mini UI owns these and RmlUi only borrows their names: mipmaps and repeat wrapping stay as the owner set them. */
std::optional<GlImage> RmlRenderer::Lendable(const Rml::String &source)
{
	if (source == SCREEN_SOURCE) return _gpu.Screen();
	if (!source.starts_with(TEXTURE_PREFIX)) return std::nullopt;

	std::optional<TextureId> texture = ParseInteger<TextureId>(std::string_view(source).substr(TEXTURE_PREFIX.size()));
	return texture.has_value() ? _textures.Image(*texture) : GlImage{};
}

/* A lent texture its owner remade or dropped is stale; RmlUi has to forget it and ask again. */
void RmlRenderer::SyncLentTextures()
{
	std::vector<Rml::String> stale;
	for (const auto &[handle, loan] : this->lent) {
		if (Lendable(loan.source) != loan.image) stale.push_back(loan.source);
	}
	for (const Rml::String &source : stale) Rml::ReleaseTexture(source, this);
}

Rml::TextureHandle RmlRenderer::LoadTexture(Rml::Vector2i &texture_dimensions, const Rml::String &source)
{
	std::optional<GlImage> image = Lendable(source);
	if (!image.has_value()) return RenderInterface_GL3::LoadTexture(texture_dimensions, source);
	if (image->name == 0) return {};

	texture_dimensions = Rml::Vector2i(static_cast<int>(image->size.width), static_cast<int>(image->size.height));
	this->lent[image->name] = {source, *image};
	return static_cast<Rml::TextureHandle>(image->name);
}

void RmlRenderer::ReleaseTexture(Rml::TextureHandle texture_handle)
{
	if (this->lent.erase(texture_handle) == 0) RenderInterface_GL3::ReleaseTexture(texture_handle);
}

/* `decorator: shader(name)` reaches a registered painter by its name; any other shader stays upstream's. */
Rml::CompiledShaderHandle RmlRenderer::CompileShader(const Rml::String &name, const Rml::Dictionary &parameters)
{
	ShaderPainter *painter = name == SHADER_DECORATOR ? FindShaderPainter(Rml::Get(parameters, "value", Rml::String())) : nullptr;
	if (painter == nullptr) return RenderInterface_GL3::CompileShader(name, parameters);

	auto shader = std::make_unique<PaintedShader>(PaintedShader{painter, Rml::Get(parameters, "dimensions", Rml::Vector2f(0.0f))});
	Rml::CompiledShaderHandle handle = reinterpret_cast<Rml::CompiledShaderHandle>(shader.get());
	this->painted.emplace(handle, std::move(shader));
	return handle;
}

/* A painter draws with its own program, so the program upstream believes is bound no longer is. */
void RmlRenderer::RenderShader(Rml::CompiledShaderHandle shader_handle, Rml::CompiledGeometryHandle geometry_handle, Rml::Vector2f translation, Rml::TextureHandle texture)
{
	auto it = this->painted.find(shader_handle);
	if (it == this->painted.end()) {
		RenderInterface_GL3::RenderShader(shader_handle, geometry_handle, translation, texture);
		return;
	}

	const PaintedShader &shader = *it->second;
	shader.painter->Paint({translation.x, translation.y, translation.x + shader.size.x, translation.y + shader.size.y});
	this->ResetProgram();
}

void RmlRenderer::ReleaseShader(Rml::CompiledShaderHandle shader_handle)
{
	if (this->painted.erase(shader_handle) == 0) RenderInterface_GL3::ReleaseShader(shader_handle);
}
