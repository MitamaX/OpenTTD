/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file shader_program.cpp A program built from the mini UI's shader files, behind the header that names the world's codes, sizes and tones. */

#include "../../stdafx.h"
#include "shader_program.h"

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FileInterface.h>

#include <array>
#include <ranges>
#include <string>
#include <string_view>

#include "../../3rdparty/fmt/ranges.h"
#include "../../core/format.hpp"
#include "../../debug.h"
#include "../../settings_type.h"
#include "../core/canvas.h"
#include "../core/sunlight.h"
#include "../core/tones.h"
#include "../gpu/gl_api.h"
#include "../map/map_overlay.h"
#include "../map/network_style.h"
#include "../map/world_tiles.h"
#include "../map/zoom_detail.h"
#include "scene_view.h"

#include "../../safeguards.h"

static constexpr double GRID_FADE_PPT = INFRASTRUCTURE_PPT / 2.0;

template <size_t N>
static void DefineCodes(std::string &header, std::string_view prefix, const std::array<std::string_view, N> &names)
{
	for (size_t code = 0; code < N; code++) header += fmt::format("#define {}{} {}u\n", prefix, names[code], code);
}

static void DefineUnsigned(std::string &header, std::string_view name, uint value)
{
	header += fmt::format("#define {} {}u\n", name, value);
}

static void DefineInt(std::string &header, std::string_view name, int value)
{
	header += fmt::format("#define {} {}\n", name, value);
}

static void DefineFloat(std::string &header, std::string_view name, double value)
{
	header += fmt::format("#define {} {:.6f}\n", name, value);
}

static void DefineVector(std::string &header, std::string_view name, double x, double y, double z)
{
	header += fmt::format("#define {} vec3({:.4f}, {:.4f}, {:.4f})\n", name, x, y, z);
}

static void DefineLookWidths(std::string &header, std::string_view name, double RailLookWidths::*width)
{
	header += fmt::format("#define {} float[RAIL_LOOKS]({:.4f})\n", name, fmt::join(RAIL_LOOK_WIDTHS | std::views::transform(width), ", "));
}

static void DefineTone(std::string &header, std::string_view name, uint32_t argb)
{
	DefineVector(header, name, ChannelShare(Red(argb)), ChannelShare(Green(argb)), ChannelShare(Blue(argb)));
}

static double LumaShare(uint weight)
{
	return static_cast<double>(weight) / (1u << LUMA_SHIFT);
}

/* The shaders name the world's codes, sizes and tones the way the packer and the painters write them. */
static std::string ShaderHeader()
{
	std::string header = "#version 330 core\n";
	DefineCodes(header, "MAT_", GROUND_MATERIAL_NAMES);
	DefineCodes(header, "TREE_", TREE_KIND_NAMES);
	DefineCodes(header, "LOOK_", RAIL_LOOK_NAMES);
	DefineInt(header, "RAIL_LOOKS", to_underlying(RailLook::End));
	DefineInt(header, "SCENE_BINDING", SCENE_BINDING);
	DefineUnsigned(header, "DENSITY_MASK", GROUND_DENSITY_MASK);
	DefineUnsigned(header, "LUSH_BIT", GROUND_LUSH_BIT);
	DefineUnsigned(header, "FLORA_COUNT_MASK", FLORA_COUNT_MASK);
	DefineUnsigned(header, "FLORA_KIND_SHIFT", FLORA_KIND_SHIFT);
	DefineUnsigned(header, "RAIL_LOOK_MASK", NETWORK_RAIL_LOOK_MASK);
	DefineUnsigned(header, "KERB_BIT", NETWORK_KERB_BIT);
	DefineFloat(header, "RAIL_BED_HALF", RAIL_BED_HALF);
	DefineFloat(header, "RAIL_GAUGE_HALF", RAIL_GAUGE_HALF);
	DefineFloat(header, "RAIL_HALF", RAIL_HALF);
	DefineFloat(header, "DISTANT_RAIL_HALF", DISTANT_RAIL_HALF);
	DefineLookWidths(header, "RAIL_BODY_HALVES", &RailLookWidths::body);
	DefineLookWidths(header, "RAIL_STRIP_HALVES", &RailLookWidths::strip);
	DefineFloat(header, "ROAD_HALF", ROAD_HALF);
	DefineFloat(header, "TRAM_BED_HALF", TRAM_BED_HALF);
	DefineFloat(header, "SLEEPERS_PER_TILE", SLEEPERS_PER_TILE);
	DefineFloat(header, "RAIL_FREQUENCY", RAIL_FREQUENCY);
	DefineFloat(header, "MARKING_FREQUENCY", MARKING_FREQUENCY);
	DefineFloat(header, "UNRESOLVED_REPEAT_PIXELS", UNRESOLVED_REPEAT_PIXELS);
	DefineFloat(header, "RESOLVED_REPEAT_PIXELS", RESOLVED_REPEAT_PIXELS);
	DefineFloat(header, "INFRASTRUCTURE_PPT", INFRASTRUCTURE_PPT);
	DefineFloat(header, "GRID_FADE_PPT", GRID_FADE_PPT);
	DefineFloat(header, "AMBIENT", AMBIENT_LIGHT);
	DefineFloat(header, "SUNLIT_CEILING", SUNLIT_CEILING);
	DefineFloat(header, "TREE_SHADOW_DEPTH", SHADOW_DEPTH);
	DefineTone(header, "VOID_TONE", COL_VOID);
	DefineTone(header, "BALLAST", COL_BALLAST);
	DefineTone(header, "CONCRETE", COL_CONCRETE);
	DefineTone(header, "STEEL", COL_STEEL);
	DefineTone(header, "ASPHALT", COL_ASPHALT);
	DefineTone(header, "DISTANT_RAIL", COL_RAIL);
	DefineTone(header, "DISTANT_ROAD", COL_ROAD);
	DefineTone(header, "RAIL_ACCENT", COL_RAIL_ACCENT);
	DefineTone(header, "ROAD_ACCENT", COL_ROAD_ACCENT);
	DefineVector(header, "LUMA_WEIGHTS", LumaShare(LUMA_RED), LumaShare(LUMA_GREEN), LumaShare(LUMA_BLUE));
	DefineFloat(header, "GREY_FLOOR", ChannelShare(GREY_FLOOR));
	DefineInt(header, "LANDSCAPE_ARCTIC", to_underlying(LandscapeType::Arctic));
	DefineInt(header, "LANDSCAPE_TROPIC", to_underlying(LandscapeType::Tropic));
	DefineInt(header, "LANDSCAPE_TOYLAND", to_underlying(LandscapeType::Toyland));
	DefineInt(header, "LAYER_NONE", to_underlying(MiniLayer::None));
	DefineInt(header, "LAYER_RAIL", to_underlying(MiniLayer::Rail));
	DefineInt(header, "LAYER_ROAD", to_underlying(MiniLayer::Road));
	return header;
}

static std::string LoadSource(const char *path)
{
	Rml::String source;
	if (!Rml::GetFileInterface()->LoadFile(path, source)) Debug(misc, 0, "[mini] shader source missing: {}", path);
	return source;
}

/* Each file counts its lines from one under its own source string number, so compile errors point into that file. */
static std::string ShaderSource(std::span<const char *const> paths)
{
	std::string source = ShaderHeader();
	for (size_t index = 0; index < paths.size(); index++) source += fmt::format("#line 1 {}\n{}\n", index, LoadSource(paths[index]));
	return source;
}

ShaderProgram::ShaderProgram(std::span<const char *const> vertex_sources, std::span<const char *const> fragment_sources) :
	vertex_sources(vertex_sources.begin(), vertex_sources.end()),
	fragment_sources(fragment_sources.begin(), fragment_sources.end())
{
}

bool ShaderProgram::Ready()
{
	if (!this->attempted) {
		this->attempted = true;
		if (this->program.Build(ShaderSource(this->vertex_sources), ShaderSource(this->fragment_sources))) this->program.BindBlock("Scene", SCENE_BINDING);
	}
	return static_cast<bool>(this->program);
}

void ShaderProgram::Release()
{
	this->program.Release();
	this->attempted = false;
}

void ShaderProgram::BindSampler(const char *sampler, uint unit) const
{
	glUniform1i(this->program.Uniform(sampler), static_cast<GLint>(unit));
}
