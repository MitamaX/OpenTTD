/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rml_renderer.h The upstream GL3 renderer, with the game's screen and the mini UI's own textures as more image sources and its shader painters as more shaders. */

#ifndef MINI_UI_RML_RENDERER_H
#define MINI_UI_RML_RENDERER_H

#include <RmlUi_Renderer_GL3.h>

#include <memory>
#include <optional>
#include <unordered_map>

#include "../gpu/texture_store.h"
#include "shader_painter.h"

class RmlRenderer final : public RenderInterface_GL3 {
public:
	static constexpr const char SCREEN_SOURCE[] = "?screen";

	~RmlRenderer() override;

	static Rml::String TextureSource(TextureId texture);

	void SyncLentTextures();

	Rml::TextureHandle LoadTexture(Rml::Vector2i &texture_dimensions, const Rml::String &source) override;
	void ReleaseTexture(Rml::TextureHandle texture_handle) override;

	Rml::CompiledShaderHandle CompileShader(const Rml::String &name, const Rml::Dictionary &parameters) override;
	void RenderShader(Rml::CompiledShaderHandle shader_handle, Rml::CompiledGeometryHandle geometry_handle, Rml::Vector2f translation, Rml::TextureHandle texture) override;
	void ReleaseShader(Rml::CompiledShaderHandle shader_handle) override;

private:
	struct Loan {
		Rml::String source;
		GlImage image;
	};

	struct PaintedShader {
		ShaderPainter *painter;
		Rml::Vector2f size;
	};

	static std::optional<GlImage> Lendable(const Rml::String &source);

	std::unordered_map<Rml::TextureHandle, Loan> lent;
	std::unordered_map<Rml::CompiledShaderHandle, std::unique_ptr<PaintedShader>> painted;
};

#endif /* MINI_UI_RML_RENDERER_H */
