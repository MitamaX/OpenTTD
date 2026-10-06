/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ground_painter.cpp The map's ground, drawn by one shader from the world's texels. */

#include "../../stdafx.h"
#include "ground_painter.h"

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FileInterface.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ranges>
#include <span>
#include <string>
#include <string_view>

#include "../../3rdparty/fmt/ranges.h"
#include "../../core/format.hpp"
#include "../../debug.h"
#include "../../settings_type.h"
#include "../core/camera.h"
#include "../core/canvas.h"
#include "../core/sunlight.h"
#include "../core/tones.h"
#include "../core/tuning.h"
#include "../gpu/draw_list.h"
#include "../gpu/gl_api.h"
#include "map_overlay.h"
#include "network_style.h"
#include "world_tiles.h"
#include "zoom_detail.h"

#include "../../safeguards.h"

static constexpr std::array<const char *, 1> VERTEX_SOURCES = {
	"mini_ui/shaders/ground.vert",
};
static constexpr std::array<const char *, 3> FRAGMENT_SOURCES = {
	"mini_ui/shaders/common.glsl",
	"mini_ui/shaders/network.glsl",
	"mini_ui/shaders/ground.frag",
};
static constexpr int QUAD_CORNERS = 4;
static constexpr double GRID_FADE_PPT = INFRASTRUCTURE_PPT / 2.0;
static constexpr double CLOCK_PERIOD_SECONDS = 3600.0;

GroundPainter _ground_painter;

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

static float ChannelShare(double channel)
{
	return static_cast<float>(channel / CHANNEL_MAX);
}

static void DefineTone(std::string &header, std::string_view name, uint32_t argb)
{
	DefineVector(header, name, ChannelShare(Red(argb)), ChannelShare(Green(argb)), ChannelShare(Blue(argb)));
}

static double LumaShare(uint weight)
{
	return static_cast<double>(weight) / (1u << LUMA_SHIFT);
}

/* The shader names the world's codes, sizes and tones the way the packer and the painters write them. */
static std::string ShaderHeader()
{
	std::string header = "#version 330 core\n";
	DefineCodes(header, "MAT_", GROUND_MATERIAL_NAMES);
	DefineCodes(header, "TREE_", TREE_KIND_NAMES);
	DefineCodes(header, "AGE_", TREE_AGE_NAMES);
	DefineCodes(header, "LOOK_", RAIL_LOOK_NAMES);
	DefineInt(header, "RAIL_LOOKS", to_underlying(RailLook::End));
	DefineUnsigned(header, "DENSITY_MASK", GROUND_DENSITY_MASK);
	DefineUnsigned(header, "LUSH_BIT", GROUND_LUSH_BIT);
	DefineUnsigned(header, "FLORA_COUNT_MASK", FLORA_COUNT_MASK);
	DefineUnsigned(header, "FLORA_AGE_SHIFT", FLORA_AGE_SHIFT);
	DefineUnsigned(header, "FLORA_AGE_MASK", FLORA_AGE_MASK);
	DefineUnsigned(header, "FLORA_KIND_SHIFT", FLORA_KIND_SHIFT);
	DefineUnsigned(header, "RAIL_LOOK_MASK", NETWORK_RAIL_LOOK_MASK);
	DefineUnsigned(header, "CATENARY_BIT", NETWORK_CATENARY_BIT);
	DefineUnsigned(header, "KERB_BIT", NETWORK_KERB_BIT);
	DefineInt(header, "GROUND_SEARCH_STEPS", GROUND_SEARCH_STEPS);
	DefineFloat(header, "GROUND_SEARCH_SHARE", GROUND_SEARCH_SHARE);
	DefineFloat(header, "LEVEL_TILES", LEVEL_TILES);
	DefineFloat(header, "VIEW_DEPTH", VIEW_DEPTH);
	DefineFloat(header, "VIEW_RISE", ViewRise());
	DefineFloat(header, "HEIGHT_SCALE", _tuning.height_scale);
	DefineFloat(header, "RAIL_BED_HALF", RAIL_BED_HALF);
	DefineFloat(header, "RAIL_GAUGE_HALF", RAIL_GAUGE_HALF);
	DefineFloat(header, "RAIL_HALF", RAIL_HALF);
	DefineFloat(header, "DISTANT_RAIL_HALF", DISTANT_RAIL_HALF);
	DefineLookWidths(header, "RAIL_BODY_HALVES", &RailLookWidths::body);
	DefineLookWidths(header, "RAIL_STRIP_HALVES", &RailLookWidths::strip);
	DefineFloat(header, "ROAD_HALF", ROAD_HALF);
	DefineFloat(header, "TRAM_BED_HALF", TRAM_BED_HALF);
	DefineFloat(header, "CATENARY_RISE", CATENARY_RISE);
	DefineFloat(header, "WIRE_HALF", WIRE_HALF);
	DefineFloat(header, "SLEEPERS_PER_TILE", SLEEPERS_PER_TILE);
	DefineFloat(header, "RAIL_FREQUENCY", RAIL_FREQUENCY);
	DefineFloat(header, "MARKING_FREQUENCY", MARKING_FREQUENCY);
	DefineFloat(header, "UNRESOLVED_REPEAT_PIXELS", UNRESOLVED_REPEAT_PIXELS);
	DefineFloat(header, "RESOLVED_REPEAT_PIXELS", RESOLVED_REPEAT_PIXELS);
	DefineFloat(header, "CATENARY_FAR_PPT", CATENARY_FAR_PPT);
	DefineFloat(header, "CATENARY_NEAR_PPT", CATENARY_NEAR_PPT);
	DefineFloat(header, "AMBIENT", AMBIENT_LIGHT);
	DefineFloat(header, "SUNLIT_CEILING", SUNLIT_CEILING);
	DefineFloat(header, "TREE_SHADOW_DEPTH", SHADOW_DEPTH);
	DefineTone(header, "VOID_TONE", COL_VOID);
	DefineTone(header, "BALLAST", COL_BALLAST);
	DefineTone(header, "CONCRETE", COL_CONCRETE);
	DefineTone(header, "STEEL", COL_STEEL);
	DefineTone(header, "ASPHALT", COL_ASPHALT);
	DefineTone(header, "WIRE", COL_WIRE);
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

static float WaveClock()
{
	static const auto start = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
	return static_cast<float>(std::fmod(elapsed.count(), CLOCK_PERIOD_SECONDS));
}

template <class Pair>
static void SetPairUniform(int location, const Pair &pair)
{
	glUniform2f(location, static_cast<float>(pair.x), static_cast<float>(pair.y));
}

static void SetSunUniform(int location, const SunVector &sun)
{
	glUniform3f(location, static_cast<float>(sun.x), static_cast<float>(sun.y), static_cast<float>(sun.z));
}

/* Over the first steps of the infrastructure zoom the tile grid takes over the ground's shape from the contours. */
static float GridShare(double ppt)
{
	return static_cast<float>(std::clamp((ppt - INFRASTRUCTURE_PPT) / GRID_FADE_PPT, 0.0, 1.0));
}

void GroundPainter::Reload()
{
	this->attempted = false;
}

void GroundPainter::Paint(const ShaderArea &area)
{
	if (_world_tiles.Size().width == 0 || !this->Ready()) return;
	this->Upload();

	this->program.Use();
	this->Configure(area);
	for (uint unit = 0; unit < this->textures.size(); unit++) this->textures[unit].Bind(unit);
	glBindVertexArray(this->vertex_array);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, QUAD_CORNERS);
	glBindVertexArray(0);
	for (uint unit = 0; unit < this->textures.size(); unit++) DataTexture::Unbind(unit);
}

void GroundPainter::Release()
{
	this->program.Release();
	for (DataTexture &texture : this->textures) texture.Release();
	if (this->vertex_array != 0) glDeleteVertexArrays(1, &this->vertex_array);
	this->vertex_array = 0;
	this->attempted = false;
}

/* A program that fails to build stays missing until the next reload, instead of failing again every frame. */
bool GroundPainter::Ready()
{
	if (!this->attempted) {
		this->attempted = true;
		this->Build();
	}
	return static_cast<bool>(this->program);
}

void GroundPainter::Build()
{
	if (!this->program.Build(ShaderSource(VERTEX_SOURCES), ShaderSource(FRAGMENT_SOURCES))) return;

	this->uniforms = {
		this->program.Uniform("u_area"),
		this->program.Uniform("u_viewport"),
		this->program.Uniform("u_centre"),
		this->program.Uniform("u_plane"),
		this->program.Uniform("u_ppt"),
		this->program.Uniform("u_right"),
		this->program.Uniform("u_toward"),
		this->program.Uniform("u_sun"),
		this->program.Uniform("u_peak"),
		this->program.Uniform("u_map"),
		this->program.Uniform("u_time"),
		this->program.Uniform("u_landscape"),
		this->program.Uniform("u_relief"),
		this->program.Uniform("u_contour"),
		this->program.Uniform("u_grid"),
		this->program.Uniform("u_layer"),
		this->program.Uniform("u_sink"),
	};
	this->program.Use();
	for (uint unit = 0; unit < this->textures.size(); unit++) glUniform1i(this->program.Uniform(SourceOf(unit).sampler), unit);
	if (this->vertex_array == 0) glGenVertexArrays(1, &this->vertex_array);
}

GroundPainter::TextureSource GroundPainter::SourceOf(uint unit)
{
	switch (unit) {
		case TILES_TEXTURE: return {"u_tiles", TexelFormat::ExactRgba, _world_tiles.Ground()};
		case WATER_TEXTURE: return {"u_water", TexelFormat::FilteredRgba, _world_tiles.Water()};
		case SURFACES_TEXTURE: return {"u_surfaces", TexelFormat::ExactRgba, _world_tiles.Surfaces()};
		case NETWORK_TEXTURE: return {"u_network", TexelFormat::ExactRgba, _world_tiles.Network()};
		default: NOT_REACHED();
	}
}

void GroundPainter::Upload()
{
	WorldChanges changes = _world_tiles.TakeChanges();
	bool whole = changes.whole || !this->textures[TILES_TEXTURE].Allocated();
	for (uint unit = 0; unit < this->textures.size(); unit++) {
		TextureSource source = SourceOf(unit);
		DataTexture &texture = this->textures[unit];
		if (whole) {
			texture.Allocate(source.format, _world_tiles.Size(), source.texels);
		} else {
			for (const Rect &area : changes.areas) texture.Update(area, source.texels);
		}
	}
	if (!whole && changes.water) this->textures[WATER_TEXTURE].Refilter();
}

/* The quad covers the element in RmlUi's pixels; the viewport is whichever layer RmlUi is drawing into. */
void GroundPainter::Configure(const ShaderArea &area) const
{
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	Dimension map = _world_tiles.Size();
	WorldPoint plane{_camera.X(), _camera.Y(), 0.0};
	double ppt = _camera.Ppt();
	float grid = GridShare(ppt);
	const Uniforms &u = this->uniforms;

	glUniform4f(u.area, area.left, area.top, area.right, area.bottom);
	glUniform2f(u.viewport, static_cast<float>(viewport[2]), static_cast<float>(viewport[3]));
	SetPairUniform(u.centre, _camera.ExactScreenOf(plane));
	SetPairUniform(u.plane, plane);
	glUniform1f(u.ppt, static_cast<float>(ppt));
	SetPairUniform(u.right, _camera.Right());
	SetPairUniform(u.toward, _camera.Toward());
	SetSunUniform(u.sun, Sun());
	glUniform1f(u.peak, static_cast<float>(_world_tiles.Peak()));
	glUniform2f(u.map, static_cast<float>(map.width), static_cast<float>(map.height));
	glUniform1f(u.time, WaveClock());
	glUniform1i(u.landscape, to_underlying(_settings_game.game_creation.landscape));
	glUniform1f(u.relief, static_cast<float>(ReliefShare()));
	glUniform1f(u.contour, (1.0f - grid) * ChannelShare(_tuning.contour_alpha));
	glUniform1f(u.grid, grid * ChannelShare(_tuning.grid_alpha));
	glUniform1i(u.layer, to_underlying(_overlay.Filter()));
	glUniform1f(u.sink, ChannelShare(_tuning.filter_alpha));
}
