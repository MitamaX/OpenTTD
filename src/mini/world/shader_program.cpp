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

#include <algorithm>
#include <array>
#include <ranges>
#include <span>
#include <string>
#include <string_view>

#include "../../3rdparty/fmt/ranges.h"
#include "../../core/format.hpp"
#include "../../debug.h"
#include "../../settings_type.h"
#include "../core/canvas.h"
#include "../core/tones.h"
#include "../core/tuning.h"
#include "../gpu/frame_profile.h"
#include "../gpu/gl_api.h"
#include "../map/airfield_marks.h"
#include "../map/map_overlay.h"
#include "../map/material_palette.h"
#include "../map/network_style.h"
#include "../map/site_shapes.h"
#include "../map/way_bends.h"
#include "../map/world_tiles.h"
#include "../map/zoom_detail.h"
#include "bare_ground.h"
#include "flow_field.h"
#include "forest_field.h"
#include "frame_units.h"
#include "massing_field.h"
#include "network_field.h"
#include "rockfall.h"
#include "scene_view.h"
#include "seabed.h"
#include "shadow_map.h"
#include "structure_mesh.h"
#include "terrain_mesh.h"
#include "track_models.h"
#include "tree_models.h"
#include "vehicle_models.h"
#include "water_outline.h"
#include "water_pass.h"
#include "way_shapes.h"

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

static std::string Pair(double x, double y)
{
	return fmt::format("vec2({:.4f}, {:.4f})", x, y);
}

static std::string Vector(double x, double y, double z)
{
	return fmt::format("vec3({:.4f}, {:.4f}, {:.4f})", x, y, z);
}

static std::string Tone(uint32_t argb)
{
	return Vector(ChannelShare(Red(argb)), ChannelShare(Green(argb)), ChannelShare(Blue(argb)));
}

static void DefineVector(std::string &header, std::string_view name, const std::string &vector)
{
	header += fmt::format("#define {} {}\n", name, vector);
}

static void DefineTone(std::string &header, std::string_view name, uint32_t argb)
{
	DefineVector(header, name, Tone(argb));
}

static void DefineTones(std::string &header, std::string_view name, std::span<const uint32_t> tones)
{
	header += fmt::format("#define {} vec3[{}]({})\n", name, tones.size(), fmt::join(tones | std::views::transform(Tone), ", "));
}

template <size_t N>
static void DefineFloats(std::string &header, std::string_view name, const std::array<double, N> &values)
{
	header += fmt::format("#define {} float[{}]({:.6f})\n", name, N, fmt::join(values, ", "));
}

template <class Field>
static void DefineFacadeTexels(std::string &header, std::string_view name, Field field)
{
	std::array<double, FACADE_METRICS.size()> texels;
	std::ranges::transform(FACADE_METRICS, texels.begin(), [&](const FacadeMetrics &metrics) { return static_cast<double>(metrics.*field); });
	DefineFloats(header, name, texels);
}

static void DefineSurfaceFlag(std::string &header, std::string_view name, SurfaceFlag flag)
{
	DefineUnsigned(header, name, SurfaceFlags{flag}.base());
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
	DefineCodes(header, "AIRFIELD_", AIRFIELD_MARK_NAMES);
	DefineCodes(header, "BED_", WAY_BED_NAMES);
	DefineUnsigned(header, "WAY_BEDS", static_cast<uint>(WAY_BED_NAMES.size()));
	DefineFloat(header, "TRAIT_SCALE", NORMALISED_BYTE_MAX);
	DefineUnsigned(header, "DENSITY_MASK", GROUND_DENSITY_MASK);
	DefineUnsigned(header, "LUSH_BIT", GROUND_LUSH_BIT);
	DefineUnsigned(header, "BUILT_BIT", GROUND_BUILT_BIT);
	DefineUnsigned(header, "GARDEN_BIT", GROUND_GARDEN_BIT);
	DefineUnsigned(header, "NETWORK_KERB_BIT", NETWORK_KERB_BIT);
	DefineUnsigned(header, "NETWORK_BRIDGE_BIT", NETWORK_BRIDGE_BIT);
	DefineUnsigned(header, "FLORA_COUNT_MASK", FLORA_COUNT_MASK);
	DefineUnsigned(header, "FLORA_KIND_SHIFT", FLORA_KIND_SHIFT);
	DefineUnsigned(header, "FLORA_AGE_SHIFT", FLORA_AGE_SHIFT);
	DefineUnsigned(header, "FLORA_AGE_MASK", FLORA_AGE_MASK);
	DefineUnsigned(header, "FLORA_MOST_TREES", FLORA_MOST_TREES);
	DefineFloat(header, "DISTANT_RAIL_HALF", DISTANT_RAIL_HALF);
	DefineFloat(header, "BALLAST_HALF", BALLAST_HALF);
	DefineFloat(header, "BEND_STEPS_PER_TILE", BEND_STEPS_PER_TILE);
	DefineFloat(header, "ROAD_HALF", ROAD_HALF);
	DefineFloat(header, "TRAM_BED_HALF", TRAM_BED_HALF);
	DefineFloat(header, "UNRESOLVED_REPEAT_PIXELS", UNRESOLVED_REPEAT_PIXELS);
	DefineFloat(header, "RESOLVED_REPEAT_PIXELS", RESOLVED_REPEAT_PIXELS);
	DefineFloat(header, "INFRASTRUCTURE_PPT", INFRASTRUCTURE_PPT);
	DefineFloat(header, "GRID_FADE_PPT", GRID_FADE_PPT);
	DefineFloat(header, "NETWORK_OPAQUE_PPT", NETWORK_FADE_END);
	DefineFloat(header, "SHELF_TILES", SHELF_TILES);
	DefineFloat(header, "FLOW_TEXEL_STILL", FLOW_TEXEL_STILL);
	DefineFloat(header, "FLOW_TEXEL_SPAN", FLOW_TEXEL_SPAN);
	DefineFloat(header, "WATERLINE", WATERLINE);
	DefineFloat(header, "CHANNEL_FALL", CHANNEL_FALL);
	DefineFloat(header, "WALL_MARK", WALL_MARK);
	DefineFloat(header, "MASONRY_MARK", MASONRY_MARK);
	DefineFloat(header, "EDGE_MARK", EDGE_MARK);
	DefineFloat(header, "FILL_MARK", FILL_MARK);
	DefineFloat(header, "ARMOUR_MARK", ARMOUR_MARK);
	DefineFloat(header, "SHORE_WARP", SHORE_WARP);
	DefineFloat(header, "SHORE_WARP_FREQUENCY", SHORE_WARP_FREQUENCY);
	DefineFloats(header, "SHORE_WARP_SEEDS", SHORE_WARP_SEEDS);
	DefineFloat(header, "SHEET_HOLD", SHEET_HOLD);
	DefineFloat(header, "SHEET_REACH", SHEET_REACH);
	DefineFloat(header, "BRIM_PULL", BRIM_PULL);
	DefineFloat(header, "BRIM_FADE", BRIM_FADE);
	DefineInt(header, "TREE_DETAILS", static_cast<int>(TREE_DETAILS));
	DefineFloats(header, "TREE_DETAIL_FLOORS", TREE_DETAIL_FLOORS);
	DefineFloats(header, "TREE_AGE_SCALES", TREE_AGE_SCALES);
	DefineInt(header, "TREE_GROWN", to_underlying(TreeAge::Grown));
	DefineFloat(header, "TREE_CROSSFADE", TREE_CROSSFADE_OCTAVES);
	DefineFloat(header, "TREE_SWAY_HEIGHT", TREE_SWAY_HEIGHT);
	DefineFloat(header, "TREE_SWAY_REACH", TREE_SWAY_REACH);
	DefineFloat(header, "TREE_LARGEST_SCALE", TREE_LARGEST_SCALE);
	DefineFloats(header, "TREE_THICKET_SCALES", TREE_THICKET_SCALES);
	DefineFloats(header, "TREE_STRETCHES", TREE_STRETCHES);
	DefineFloat(header, "TREE_LEAN", TREE_LEAN);
	DefineFloat(header, "TREE_MODEL_TALLEST", TREE_MODEL_TALLEST);
	DefineFloat(header, "WOOD_TRANSLUCENCY_BOUND", WOOD_TRANSLUCENCY_BOUND);
	DefineFloat(header, "FOLIAGE_TRANSLUCENCY_FROM", FOLIAGE_TRANSLUCENCY_FROM);
	DefineFloat(header, "FOLIAGE_TRANSLUCENCY_FULL", FOLIAGE_TRANSLUCENCY_FULL);
	DefineTones(header, "TREE_CANOPY", TREE_CANOPY_TONES);
	DefineTones(header, "TREE_LITTER", TREE_LITTER_TONES);
	DefineFloats(header, "TREE_LITTER_COVERS", TREE_LITTER_COVERS);
	DefineTones(header, "FRAME_TONES", SwatchTints(Swatch::Frame));
	DefineTones(header, "MULLION_TONES", SwatchTints(Swatch::Mullion));
	const auto landscape = to_underlying(_settings_game.game_creation.landscape);
	DefineTone(header, "GRASS_DARK", GRASS_DARK_TONES[landscape]);
	DefineTone(header, "GRASS_LIGHT", GRASS_LIGHT_TONES[landscape]);
	DefineTone(header, "VOID_TONE", COL_VOID);
	DefineTone(header, "DISTANT_RAIL", COL_RAIL);
	DefineTone(header, "DISTANT_BALLAST", COL_DISTANT_BALLAST);
	DefineTone(header, "DISTANT_ROAD", COL_ASPHALT);
	DefineTone(header, "PAVEMENT_TONE", COL_PAVEMENT);
	DefineTone(header, "DISTANT_WATER", COL_WATER);
	DefineTone(header, "RAIL_ACCENT", COL_RAIL_ACCENT);
	DefineTone(header, "ROAD_ACCENT", COL_ROAD_ACCENT);
	DefineVector(header, "LUMA_WEIGHTS", Vector(LumaShare(LUMA_RED), LumaShare(LUMA_GREEN), LumaShare(LUMA_BLUE)));
	DefineFloat(header, "GREY_FLOOR", ChannelShare(GREY_FLOOR));
	DefineInt(header, "LANDSCAPE_ARCTIC", to_underlying(LandscapeType::Arctic));
	DefineInt(header, "LANDSCAPE_TROPIC", to_underlying(LandscapeType::Tropic));
	DefineInt(header, "LANDSCAPE_TOYLAND", to_underlying(LandscapeType::Toyland));
	DefineInt(header, "LANDSCAPE", landscape);
	DefineInt(header, "LAYER_NONE", to_underlying(MiniLayer::None));
	DefineInt(header, "LAYER_RAIL", to_underlying(MiniLayer::Rail));
	DefineInt(header, "LAYER_ROAD", to_underlying(MiniLayer::Road));
	DefineCodes(header, "CLAD_", MATERIAL_NAMES);
	DefineCodes(header, "WINDOWS_", WINDOW_GRID_NAMES);
	DefineInt(header, "WINDOW_GRIDS", static_cast<int>(WINDOW_GRID_NAMES.size()));
	DefineInt(header, "TEXELS_PER_TILE", TEXELS_PER_TILE);
	DefineFloat(header, "FORM_HEIGHT_SCALE", FORM_HEIGHT_SCALE);
	DefineFloat(header, "TALLEST_STRUCTURE", MAX_STRUCTURE_TILES * FORM_HEIGHT_SCALE);
	DefineUnsigned(header, "MASSING_GRID_MASK", MASSING_GRID_MASK);
	DefineUnsigned(header, "MASSING_FAR_X_BIT", MASSING_FAR_X_BIT);
	DefineUnsigned(header, "MASSING_FAR_Y_BIT", MASSING_FAR_Y_BIT);
	DefineUnsigned(header, "MASSING_RAISED_SHIFT", MASSING_RAISED_SHIFT);
	DefineUnsigned(header, "MASSING_RAISED_MOST", MASSING_RAISED_MOST);
	DefineUnsigned(header, "MASSING_INSET_SHIFT", MASSING_INSET_SHIFT);
	DefineUnsigned(header, "MASSING_INSET_BITS", MASSING_INSET_BITS);
	DefineUnsigned(header, "MASSING_INSET_MOST", MASSING_INSET_MOST);
	DefineFloat(header, "MASSING_INSET_STEPS", MASSING_INSET_STEPS);
	DefineUnsigned(header, "MASSING_EAVES_SHIFT", MASSING_EAVES_SHIFT);
	DefineFacadeTexels(header, "FACADE_GROUND", &FacadeMetrics::ground_texels);
	DefineFacadeTexels(header, "FACADE_STOREY", &FacadeMetrics::storey_texels);
	DefineFacadeTexels(header, "FACADE_BAY", &FacadeMetrics::bay_texels);
	DefineFacadeTexels(header, "FACADE_PITCH", &FacadeMetrics::pitch_texels);
	DefineSurfaceFlag(header, "SURFACE_FACADE", SurfaceFlag::Facade);
	DefineSurfaceFlag(header, "SURFACE_FRONT", SurfaceFlag::Front);
	DefineSurfaceFlag(header, "SURFACE_ROOF", SurfaceFlag::Roof);
	DefineSurfaceFlag(header, "SURFACE_DECAL", SurfaceFlag::Decal);
	DefineSurfaceFlag(header, "SURFACE_EAVE", SurfaceFlag::Eave);
	DefineInt(header, "MOST_WAKES", MOST_WAKES);
	DefineFloat(header, "PANE_GLOSS", PANE_GLOSS);
	DefineFloat(header, "UNIT_REACH", UNIT_REACH);
	DefineFloat(header, "POOL_RADIUS", POOL_RADIUS);
	DefineFloat(header, "FLEX_REACH", FLEX_REACH);
	DefineInt(header, "CASCADES", ShadowMap::CASCADES);
	DefineFloat(header, "SHADOW_FILTER_SPAN", ShadowMap::FILTER_SPAN);
	DefineFloat(header, "SHADOW_NORMAL_OFFSET_TEXELS", ShadowMap::NORMAL_OFFSET_TEXELS);
	DefineFloat(header, "SHADOW_LEAST_OFFSET_SHARE", ShadowMap::LEAST_OFFSET_SHARE);
	DefineFloat(header, "SHADOW_GRAZING_OFFSET_SHARE", ShadowMap::GRAZING_OFFSET_SHARE);
	DefineFloat(header, "SHADOW_CASCADE_BLEND", ShadowMap::CASCADE_BLEND);
	DefineFloat(header, "ROCK_SHOWN_PIXELS", ROCK_SHOWN_PIXELS);
	DefineFloat(header, "ROCK_SOLID_PIXELS", ROCK_SOLID_PIXELS);
	DefineFloat(header, "STANDARD_SLOPE", STANDARD_SLOPE);
	DefineFloat(header, "STEEPEST_SLOPE", STEEPEST_SLOPE);
	DefineFloat(header, "ALPINE_ALTITUDE", ALPINE_ALTITUDE);
	DefineFloat(header, "STEEP_BARING", STEEP_BARING);
	DefineFloat(header, "SHED_BARING", SHED_BARING);
	DefineFloat(header, "ALPINE_BARING", ALPINE_BARING);
	DefineFloat(header, "SCREE_EXPOSURE", SCREE_EXPOSURE);
	DefineFloat(header, "TOY_BARING", TOY_BARING);
	DefineFloat(header, "ROCK_EXPOSURE", ROCK_EXPOSURE);
	DefineFloat(header, "EXPOSURE_BLEND", EXPOSURE_BLEND);
	DefineFloat(header, "MIN_RELIEF_SPAN", MIN_RELIEF_SPAN);
	DefineFloat(header, "STRATA_PER_LEVEL", STRATA_PER_LEVEL);
	DefineFloat(header, "TOY_STRATA_PER_LEVEL", TOY_STRATA_PER_LEVEL);
	DefineFloat(header, "LAYER_VARIETY", LAYER_VARIETY);
	DefineVector(header, "STRATA_DIP", Pair(STRATA_DIP.x, STRATA_DIP.y));
	DefineTone(header, "ROCK", ROCK_TONE);
	DefineTone(header, "ROCK_WARM", ROCK_WARM_TONE);
	DefineTone(header, "SCREE", SCREE_TONE);
	DefineTone(header, "CAP_SNOW", CAP_SNOW_TONE);
	DefineTone(header, "DRY_SAND", DRY_SAND_TONE);
	DefineTone(header, "LITTER_MOSS", LITTER_MOSS_TONE);
	DefineTones(header, "TOY_STRATA_TONES", TOY_STRATA_TONES);
	DefineInt(header, "TOY_STRATA_COUNT", static_cast<int>(TOY_STRATA_TONES.size()));
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

/* The header names the map's climate, so a program is built anew for a map of another. */
bool ShaderProgram::Ready()
{
	if (this->landscape != _settings_game.game_creation.landscape) {
		this->landscape = _settings_game.game_creation.landscape;
		this->attempted = false;
	}
	if (!this->attempted) {
		this->attempted = true;
		_frame_profile.Count("shader_builds");
		if (this->program.Build(ShaderSource(this->vertex_sources), ShaderSource(this->fragment_sources))) this->BindShared();
	}
	return static_cast<bool>(this->program);
}

void ShaderProgram::BindShared() const
{
	this->program.BindBlock("Scene", SCENE_BINDING);
	this->program.BindBlock("Shadows", SHADOWS_BINDING);
	this->program.Use();
	for (const FrameSampler &sampler : FRAME_SAMPLERS) this->BindSampler(sampler.name, sampler.unit);
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

void UploadOverlay(const ShaderProgram &program)
{
	glUniform1i(program.Uniform("u_layer"), to_underlying(_overlay.Filter()));
	glUniform1f(program.Uniform("u_sink"), ChannelShare(_tuning.filter_alpha));
}
