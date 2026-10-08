uniform float u_contour;
uniform float u_grid;

in vec3 v_world;
in vec3 v_normal;
in float v_mark;

out vec4 frag_colour;

const float STANDARD_SLOPE = 1.0;
const float STEEPEST_SLOPE = sqrt(2.0);
const float ALPINE_ALTITUDE = 0.55;
const float STEEP_BARING = 1.1;
const float SHED_BARING = 0.9;
const float ALPINE_BARING = 0.9;
const float SCREE_EXPOSURE = 0.5;
const float ROCK_EXPOSURE = 0.9;
const float EXPOSURE_BLEND = 0.14;
const float BLANKET_SLIP_SLOPE = 1.1;
const float SNOW_EDGE = 0.07;
const float SAND_EDGE = 0.2;
const float BLANKET_FRAY = 0.35;
const float BLANKET_FRAY_FREQUENCY = 0.6;
const float CLINGING = 0.75;
const float SNOW_LINE_RAGGED = 2.5;
const float CAP_COVER = 0.85;
const vec3 CAP_SNOW = vec3(0.86, 0.89, 0.94);
const float BROAD_FREQUENCY = 0.035;
const vec3 PACKED_SNOW_TINT = vec3(0.8, 0.86, 0.95);
const vec3 BAKED_SAND_TINT = vec3(0.9, 0.85, 0.78);

const float LINE_HALF_PIXELS = 0.5;
const float HALF_LEVEL = 0.5;
const float CONTOUR_DEPTH = 0.75;
const float INDEX_CONTOUR_EVERY = 5.0;
const float INDEX_CONTOUR_WEIGHT = 1.6;
const float CROWDED_CONTOUR_PIXELS = 3.0;
const float SPACED_CONTOUR_PIXELS = 9.0;
const float MIN_RELIEF_SPAN = 6.0;
const vec3 UPRIGHT = vec3(0.0, 0.0, 1.0);
const vec3 LOWLAND = vec3(0.90, 0.95, 0.86);
const vec3 HIGHLAND = vec3(1.12, 1.06, 0.94);
const float TERRAIN_ROUGHNESS = 0.85;
const float TOY_ROUGHNESS = 0.72;
const float SNOW_ROUGHNESS = 0.7;
const float SAND_ROUGHNESS = 0.9;
const float WALL_MARK = 0.75;
const float EDGE_MARK = 0.375;
const float FILL_MARK = 0.25;
const float SUBMERGED_MARK = 0.08;
const float DROWNED_LEVELS = 0.6;
const float SCATTERED_SHADOW = 0.7;
const float SCATTERING_LEVELS = 0.8;

const float BLEND_WIDTH = 0.28;
const float BUILT_BLEND_WIDTH = 0.03;
const float BLEND_PIXELS = 0.8;
const float BLOCKS_BLUR_PIXELS = 4.0;
const float BLOCKS_SHOWN_PIXELS = 12.0;
const float NATURAL_WARP = 0.32;
const float CLIMATE_WARP = 0.6;
const float CLIMATE_SWAY = 1.1;
const float CLIMATE_SWAY_FREQUENCY = 0.19;
const float CLIMATE_FRAY = 1.0;
const float CLIMATE_FRAY_FREQUENCY = 0.33;
const float BUILT_WARP = 0.06;
const float DEPTH_LOD = 3.0;
const int DRY_LEVEL = 3;
const int DRY_REACH = 2;
const float SANDED_SEA = 0.26;

const vec3 SOIL = vec3(0.45, 0.37, 0.27);
const vec3 TOY_SOIL = vec3(0.86, 0.66, 0.55);
const vec3 TOY_TEAL = vec3(0.42, 0.82, 0.6);
const vec3 TOY_MINT = vec3(0.5, 0.86, 0.48);
const vec3 TOY_LIME = vec3(0.7, 0.9, 0.42);
const float TOY_LAWN_CONTRAST = 0.6;
const float ROUGH_BARE = 0.6;
const float TOY_ROUGH_BARE = 0.2;
const vec3 LUSH_DARK = vec3(0.2, 0.35, 0.17);
const vec3 LUSH_LIGHT = vec3(0.31, 0.47, 0.23);
const vec3 ROUGH_TINT = vec3(0.55, 0.50, 0.30);
const vec3 STRAW_TINT = vec3(1.14, 1.04, 0.78);
const float STRAW_PATCHES_PER_TILE = 3.0;
const float STRAW_MEAN = 0.12;
const vec3 ROCK = vec3(0.50, 0.49, 0.47);
const vec3 ROCK_WARM = vec3(0.58, 0.52, 0.44);
const vec3 SCREE = vec3(0.55, 0.52, 0.47);
const vec3 SNOW = vec3(0.87, 0.9, 0.95);
const vec3 SCOURED_SNOW = vec3(0.76, 0.82, 0.9);
const vec3 SAND = vec3(0.87, 0.76, 0.54);
const vec3 RED_SAND = vec3(0.85, 0.64, 0.43);
const vec3 GRAVEL = vec3(0.74, 0.62, 0.48);
const vec3 PACKED_EARTH = vec3(0.72, 0.57, 0.43);
const vec3 DRY_SAND = vec3(0.78, 0.71, 0.55);
const vec3 PAVING = vec3(0.57, 0.55, 0.51);
const vec3 SANDSTONE_PAVING = vec3(0.7, 0.62, 0.5);
const vec3 YARD = vec3(0.49, 0.41, 0.32);
const vec3 BANK = vec3(0.50, 0.50, 0.48);
const vec3 WET_SAND = vec3(0.50, 0.46, 0.37);
const vec3 SILT = vec3(0.38, 0.40, 0.33);
const vec3 MUD = vec3(0.40, 0.36, 0.27);
const vec3 CANAL_FLOOR = vec3(0.45, 0.45, 0.42);
const vec3 HEDGE = vec3(0.24, 0.33, 0.16);

const float STRATA_PER_LEVEL = 2.5;
const float STRATA_WARP = 1.8;
const float LAYER_VARIETY = 0.3;
const float LEDGE_SHADE = 0.8;
const float SLABS_PER_TILE = 1.7;
const float SLAB_VARIETY = 0.3;
const float LEVEL_BEDDING = 0.35;
const float CRACKS_PER_TILE = 3.1;
const float CRACK_WIDTH = 0.05;
const float CRACK_DEPTH = 0.18;
const float OUTCROPS_PER_TILE = 0.9;
const float STONE_PATCHES_PER_TILE = 1.7;
const float STONE_GAPS = 0.3;
const float PALE_STONE = 1.15;
const float BOULDERS_PER_TILE = 5.5;
const float MICRO_PER_TILE = 17.0;
const float CLUMPS_PER_TILE = 3.3;
const float LOCAL_PER_TILE = 1.3;
const float RELIEF_DEPTH = 0.035;
const float STONY_HOLLOWS = 1.0;

const vec2 GRASS_RUGGED = vec2(0.6, 0.0);
const vec2 ROUGH_RUGGED = vec2(0.7, 0.25);
const vec2 SCREE_RUGGED = vec2(0.6, 1.0);
const vec2 ROCK_RUGGED = vec2(1.0, 0.15);
const vec2 FIELD_RUGGED = vec2(0.35, 0.05);
const vec2 SAND_RUGGED = vec2(0.2, 0.05);
const vec2 PAVED_RUGGED = vec2(0.1, 0.0);
const vec2 SNOW_RUGGED = vec2(0.12, 0.0);

const float TAU = 6.2831853;
const vec2 WIND = vec2(0.8, 0.6);
const vec2 ACROSS_WIND = vec2(-0.6, 0.8);
const float DRIFT_ALONG = 0.22;
const float DRIFT_ACROSS = 0.8;
const float DRIFT_HEIGHT = 0.3;
const float RIDGE_ALONG = 0.45;
const float RIDGE_ACROSS = 1.4;
const float RIDGE_HEIGHT = 0.07;
const float SASTRUGI_ALONG = 1.6;
const float SASTRUGI_ACROSS = 6.0;
const float SASTRUGI_HEIGHT = 0.012;
const float GLINT_PIXELS = 3.0;
const int GLINT_LEVEL_STRIDE = 7919;
const float GLINT_SHARE = 0.2;
const float GLINT_SPREAD = 3.0;
const float GLINT_FOCUS = 80.0;
const float GLINT_GAIN = 7.0;
const float GLINT_FROM_PIXELS = 10.0;
const float GLINT_TO_PIXELS = 40.0;
const float DUNE_SPACING = 3.4;
const float DUNE_CREST = 0.65;
const float DUNE_HEIGHT = 0.18;
const float DUNE_WARP = 1.3;
const float DUNE_WARP_FREQUENCY = 0.11;
const float DUNE_SWELL_FREQUENCY = 0.07;
const float DUNE_CALM = 0.22;
const float DUNE_STRETCH = 0.9;
const float DUNE_STRETCH_FREQUENCY = 0.045;
const vec2 CROSS_WIND = vec2(0.47, 0.88);
const float CROSS_SPACING = 1.7;
const float CROSS_SHARE_LOW = 0.15;
const float CROSS_SHARE_HIGH = 1.4;
const float CROSS_SHARE_FREQUENCY = 0.035;
const float RIPPLES_PER_TILE = 9.0;
const float RIPPLE_WARP = 0.5;
const float RIPPLE_HEIGHT = 0.0025;
const float RIPPLE_SHADE = 0.05;
const float CROSS_RIPPLES_PER_TILE = 6.3;
const float RIPPLE_BLEND_FREQUENCY = 0.08;
const float TOY_GRAIN = 0.45;

const int TOY_STRATA_COUNT = 4;
const vec3 TOY_STRATA[TOY_STRATA_COUNT] = vec3[TOY_STRATA_COUNT](
	vec3(0.95, 0.66, 0.76),
	vec3(0.76, 0.69, 0.93),
	vec3(0.99, 0.82, 0.6),
	vec3(0.62, 0.86, 0.9)
);

const int CROP_COUNT = 6;
const vec3 CROPS[CROP_COUNT] = vec3[CROP_COUNT](
	vec3(0.52, 0.40, 0.27),
	vec3(0.47, 0.60, 0.27),
	vec3(0.79, 0.68, 0.33),
	vec3(0.85, 0.79, 0.32),
	vec3(0.64, 0.56, 0.30),
	vec3(0.40, 0.53, 0.30)
);
const float FURROWS_PER_TILE = 7.0;
const float FURROW_DEPTH = 0.55;
const float LEVEL_FIELD_SLOPE = 0.25;
const float HILLSIDE_FIELD_SLOPE = 0.6;
const float TRAMLINES_PER_TILE = 1.0;
const float TRAMLINE_GAUGE = 0.045;
const float TRAMLINE_HALF_WIDTH = 0.012;
const float TRAMLINE_DEPTH = 0.6;
const float HEDGE_WIDTH = 0.05;
const float HEDGE_FRINGE = 2.5;
const float HEDGE_FRINGE_SHADE = 0.25;
const float HEDGE_GAPS_PER_TILE = 1.9;
const float HEDGE_OPACITY = 0.85;

const float FOREST_COVER = 0.9;
const float CANOPY_OCCLUSION = 0.45;
const float CANOPY_VARIETY = 0.2;
const float GRID_DEPTH = 0.6;
const float HIDDEN_BAND_MARGIN = 1.1;

const float COURSES_PER_TILE = 6.0;
const float BLOCKS_PER_TILE = 3.0;
const float MORTAR_SHARE = 0.08;
const vec3 MASONRY = vec3(0.56, 0.53, 0.49);

const int EARTH_STRATA_COUNT = 5;
const vec3 EARTH_STRATA[EARTH_STRATA_COUNT] = vec3[EARTH_STRATA_COUNT](
	vec3(0.55, 0.42, 0.29),
	vec3(0.70, 0.57, 0.40),
	vec3(0.62, 0.55, 0.46),
	vec3(0.50, 0.47, 0.43),
	vec3(0.64, 0.47, 0.33)
);
const float EDGE_STRATA_PER_TILE = 1.6;
const float EDGE_STRATA_WARP = 0.6;
const float EDGE_STRATA_SWAY = 0.15;
const float EDGE_LAYER_VARIETY = 0.12;
const float EDGE_GRAIN_PER_TILE = 4.0;
const float EDGE_SEAM_SHADE = 0.82;
const float EDGE_WET_SHADE = 0.6;
const float EDGE_WET_REACH = 0.35;
const float EDGE_TURF = 0.12;
const float EDGE_TOPSOIL = 0.35;
const float EDGE_TOPSOIL_SHADE = 0.9;
const float EDGE_TOPSOIL_SWAY = 0.7;
const float EDGE_TOPSOIL_SWING = 0.6;
const float EDGE_ROOTS_PER_TILE = 9.0;
const float EDGE_ROOTS_RAGGED = 0.6;

const vec3 RUNWAY_ASPHALT = vec3(0.25, 0.26, 0.27);
const vec3 APRON_CONCRETE = vec3(0.66, 0.65, 0.62);
const vec3 AIRFIELD_WHITE = vec3(0.92, 0.92, 0.9);
const vec3 TAXI_YELLOW = vec3(0.9, 0.72, 0.18);
const vec3 EDGE_LIGHT = vec3(1.0, 0.93, 0.7);
const float RUNWAY_EDGE = 0.41;
const float RUNWAY_EDGE_HALF = 0.015;
const float CENTRELINE_HALF = 0.012;
const float CENTRELINE_DASHES = 3.0;
const float THRESHOLD_FROM = 0.72;
const float THRESHOLD_TO = 0.9;
const float THRESHOLD_REACH = 0.36;
const float THRESHOLD_BARS = 10.0;
const float EDGE_LIGHT_LATERAL = 0.46;
const float EDGE_LIGHTS = 4.0;
const float EDGE_LIGHT_RADIUS = 0.018;
const float TAXI_LINE_HALF = 0.014;
const float PANEL_JOINTS = 4.0;
const float PANEL_JOINT_HALF = 0.006;
const float STAND_BOX = 0.3;
const float HELIPAD_RING = 0.32;
const float HELIPAD_LINE_HALF = 0.02;

struct Ground {
	uint material;
	float density;
	uint variant;
};

struct Grain {
	float broad;
	float local;
	float clump;
	float fine;
	float micro;
	float stones;
	float hollow;
};

/* How the ground leans and how well its rock layers resolve, read before the fragment branches. */
struct Relief {
	float slope;
	float steep;
	float strata;
};

struct Exposure {
	float scree;
	float rock;
};

/* The loose and the solid stone this point would show where its ground wears through, and how far it lies on the shelf atop a layer of the rock. */
struct Bare {
	vec3 scree;
	vec3 rock;
	float shelf;
};

const Bare NO_BARE = Bare(vec3(0.0), vec3(0.0), 0.0);

/* A ground's colour and how much its fine bumps and its stones stand out of it. */
struct Patch {
	vec3 tone;
	vec2 rugged;
};

/* What every ground meeting at a point is shaded with: where it is, how it leans, how far it wears bare, the stone it would show, whether its shore is armoured, its climate and how far a town's feet have trodden it. */
struct Site {
	vec2 p;
	Grain grain;
	Relief relief;
	Exposure exposure;
	Bare bare;
	bool armoured;
	Climate climate;
	float ragged;
	float trodden;
};

struct Blanket {
	float cover;
	vec3 tone;
	vec2 slope;
	vec2 rugged;
	float roughness;
	float glint;
};

const Blanket NO_BLANKET = Blanket(0.0, vec3(0.0), vec2(0.0), vec2(0.0), 0.0, 0.0);

struct Dune {
	float height;
	vec2 slope;
	float along;
	vec2 heading;
	float lee;
};

struct Shade {
	vec3 albedo;
	vec3 normal;
	float roughness;
	float occlusion;
	float glint;
};

struct Water {
	float level;
	float cover;
	float depth;
	vec3 share;
	float sea;
	float canal;
};

struct Canopy {
	vec4 paint;
	float shade;
};

/* The ground one screen pixel steps over across and down the screen; lines on the ground fade by how far apart it puts them. */
mat2 footprint;

Grain Moulded(Grain grain)
{
	return Grain(grain.broad, mix(0.5, grain.local, TOY_GRAIN), mix(0.5, grain.clump, TOY_GRAIN), mix(0.5, grain.fine, TOY_GRAIN), mix(0.5, grain.micro, TOY_GRAIN), grain.stones, grain.hollow * TOY_GRAIN);
}

Grain GrainAt(vec2 p, Detail detail)
{
	float local = (2.0 * FacingOctave(detail, LOCAL_PER_TILE, 0.0) + FacingOctave(detail, LOCAL_PER_TILE * 2.07, 17.3)) / 3.0;
	Grain grain = Grain(Layered(p, 0.15), local, FacingOctave(detail, CLUMPS_PER_TILE, 2.2), detail.fine, detail.micro, detail.stones, detail.hollow);
	return Landscape() == LANDSCAPE_TOYLAND ? Moulded(grain) : grain;
}

Ground GroundAt(ivec2 tile)
{
	uvec4 codes = CodesAt(tile);
	return Ground(codes.r, float(codes.g & DENSITY_MASK) / float(DENSITY_MASK), codes.a);
}

float GroundRoughness()
{
	return Landscape() == LANDSCAPE_TOYLAND ? TOY_ROUGHNESS : TERRAIN_ROUGHNESS;
}

vec3 Paving()
{
	return Landscape() == LANDSCAPE_TROPIC ? SANDSTONE_PAVING : PAVING;
}

vec3 Soil()
{
	return Landscape() == LANDSCAPE_TOYLAND ? TOY_SOIL : SOIL;
}

/* The ground's colours are picked on screen; they are lit as the light they reflect.
 * Below the sea the waves scatter the sunlight, so the shadows falling on the bed lighten with the depth of the water over it. */
vec4 Lit(Shade shade)
{
	vec3 position = RenderPoint(v_world);
	float scattered = SCATTERED_SHADOW * smoothstep(0.0, SCATTERING_LEVELS, -v_world.z);
	float sunlit = mix(SunVisibility(position, shade.normal), 1.0, scattered);
	return SunlitRadiance(Linear(shade.albedo), shade.normal, position, shade.roughness, shade.occlusion, shade.glint * GLINT_GAIN, sunlit);
}

/* How steeply the ground leans at the fragment, in levels per tile. */
float SlopeOf(vec3 normal)
{
	return length(normal.xy) / max(normal.z, MIN_SPREAD) / LevelRise();
}

/* Rock is bedded in layers of uneven thickness that run level through the hills, bowed a little this way and that. */
float BeddingAt(vec2 p)
{
	float thickness = Noise(vec2(v_world.z * 0.7, 3.1)) * STRATA_PER_LEVEL;
	return v_world.z * STRATA_PER_LEVEL + thickness + (Noise(p * 0.3) - 0.5) * STRATA_WARP + (Octave(p + 5.3, 1.1) - 0.5) * 0.35;
}

Relief ReliefAt(vec3 normal, float levels_per_pixel)
{
	float slope = SlopeOf(normal);
	float layer_pixels = 1.0 / (levels_per_pixel * STRATA_PER_LEVEL);
	return Relief(slope, smoothstep(STANDARD_SLOPE, STEEPEST_SLOPE, slope), smoothstep(UNRESOLVED_REPEAT_PIXELS, RESOLVED_REPEAT_PIXELS * 2.0, layer_pixels));
}

vec3 Altitude(vec3 colour, float height)
{
	float t = clamp(height / max(Peak(), MIN_RELIEF_SPAN), 0.0, 1.0);
	vec3 tint = t < 0.5 ? mix(LOWLAND, vec3(1.0), t * 2.0) : mix(vec3(1.0), HIGHLAND, t * 2.0 - 1.0);
	return colour * tint;
}

float Contour(float height, float levels_per_pixel)
{
	bool index = mod(floor(height) + 1.0, INDEX_CONTOUR_EVERY) == 0.0;
	float weight = index ? INDEX_CONTOUR_WEIGHT : 1.0;
	float line = PixelLine(abs(fract(height) - HALF_LEVEL), levels_per_pixel, LINE_HALF_PIXELS * weight);
	return line * smoothstep(CROWDED_CONTOUR_PIXELS, SPACED_CONTOUR_PIXELS, 1.0 / levels_per_pixel) * weight;
}

/* Toyland's lawns drift between teal, mint and lime, held toward the mint between so they wash gently from one to the next. */
vec3 ToyLawn(float shade)
{
	float calm = mix(0.5, shade, TOY_LAWN_CONTRAST);
	return mix(mix(TOY_TEAL, TOY_MINT, smoothstep(0.2, 0.45, calm)), TOY_LIME, smoothstep(0.55, 0.85, calm));
}

vec3 GrassTone(float shade, float lush)
{
	if (Landscape() == LANDSCAPE_TOYLAND) return ToyLawn(shade);
	vec3 dark = vec3(0.33, 0.43, 0.21);
	vec3 light = vec3(0.50, 0.56, 0.30);
	if (Landscape() == LANDSCAPE_ARCTIC) {
		dark = vec3(0.38, 0.47, 0.34);
		light = vec3(0.53, 0.60, 0.44);
	} else if (Landscape() == LANDSCAPE_TROPIC) {
		dark = vec3(0.52, 0.55, 0.28);
		light = vec3(0.68, 0.66, 0.36);
	}
	return mix(mix(dark, light, shade), mix(LUSH_DARK, LUSH_LIGHT, shade), lush);
}

float Hollowed(Grain grain, float depth)
{
	return clamp(1.0 - depth * grain.hollow, 0.55, 1.15);
}

float Cover(float density, float clump)
{
	return density >= 1.0 ? 1.0 : smoothstep(-0.06, 0.06, density + clump - 1.0);
}

/* Grass turns to straw here and there, and its blades fleck it light and dark up close; from afar the straw's patches, too small to make out, settle into one tone. */
vec3 Grass(Ground ground, Grain grain, float lush)
{
	vec3 green = GrassTone(grain.broad, lush);
	vec3 straw = green * STRAW_TINT;
	float shade = Varied(grain.local, 0.16) * Varied(grain.clump, 0.4) * Varied(grain.fine, 0.35) * Varied(grain.micro, 0.7);
	float strawed = mix(STRAW_MEAN, smoothstep(0.5, 0.85, grain.local) + 0.25 * smoothstep(0.6, 0.85, grain.micro), Resolved(STRAW_PATCHES_PER_TILE));
	vec3 blades = mix(green, straw, strawed) * shade;
	vec3 soil = Soil() * Varied(grain.local, 0.2) * Varied(grain.micro, 0.25) * mix(1.0, 1.25, grain.stones);
	return mix(soil, blades, Cover(ground.density, grain.local));
}

vec3 Meadow(Grain grain, float lush)
{
	return Grass(Ground(MAT_GRASS, 1.0, 0u), grain, lush);
}

/* Toyland's strata blended as they read from too far to make out one from another. */
vec3 ToyStrataMean()
{
	return (TOY_STRATA[0] + TOY_STRATA[1] + TOY_STRATA[2] + TOY_STRATA[3]) / float(TOY_STRATA_COUNT);
}

vec3 Stratum(int layer)
{
	if (Landscape() == LANDSCAPE_TOYLAND) return TOY_STRATA[abs(layer) % TOY_STRATA_COUNT];
	return mix(ROCK, ROCK_WARM, Hash(ivec2(layer, 7))) * Varied(Hash(ivec2(layer, 19)), LAYER_VARIETY);
}

/* Loose stones shed from rock: gravel speckled light and dark, with pebbles strewn over it and dark gaps between them. */
vec3 Scree(Grain grain, vec2 p)
{
	vec3 candy = mix(ToyStrataMean(), Stratum(int(Noise(p * BOULDERS_PER_TILE) * 8.0)), Resolved(BOULDERS_PER_TILE));
	vec3 loose = Landscape() == LANDSCAPE_TOYLAND ? candy : SCREE;
	vec3 tone = loose * Varied(grain.local, 0.18) * Varied(grain.fine, 0.14) * Varied(grain.micro, 0.45);
	return tone * mix(1.0, 1.18, grain.stones) * Hollowed(grain, STONY_HOLLOWS);
}

/* Cracks wander through rock, here and there breaking off. */
float Cracks(vec2 p)
{
	float shown = Resolved(CRACKS_PER_TILE * 4.0);
	if (shown <= 0.0) return 0.0;
	float vein = abs(Noise(p * CRACKS_PER_TILE) - 0.5);
	return (1.0 - smoothstep(0.0, CRACK_WIDTH, vein)) * smoothstep(0.4, 0.62, Noise(p * 1.9 + 8.1)) * shown;
}

/* Bare rock in layers stepping out in ledges where it stands steep, weathered and veined with cracks where it lies flat; toyland's candy layers show only where it stands steep. */
vec3 Rock(vec2 p, float bedding, Relief relief, Grain grain)
{
	int layer = int(floor(bedding));
	float strata = relief.steep * relief.strata;
	vec3 bed = Stratum(layer);
	float ledge = mix(LEDGE_SHADE, 1.08, smoothstep(0.0, 0.3, fract(bedding)));
	bool toy = Landscape() == LANDSCAPE_TOYLAND;
	vec3 slabs = (toy ? ToyStrataMean() : mix(ROCK, ROCK_WARM, 0.3)) * Varied(Octave(p + 2.0, SLABS_PER_TILE), SLAB_VARIETY);
	vec3 rock = mix(mix(slabs, bed, toy ? 0.0 : LEVEL_BEDDING), bed * ledge, strata);
	vec3 weathered = mix(rock, rock * vec3(0.72, 0.74, 0.66), smoothstep(0.55, 0.8, Octave(p + 4.4, 2.7)));
	return weathered * Varied(grain.fine, 0.22) * Varied(grain.micro, 0.3) * (1.0 - CRACK_DEPTH * (1.0 - relief.steep) * Cracks(p));
}

Bare BareAt(vec2 p, Relief relief, Grain grain)
{
	float bedding = BeddingAt(p);
	return Bare(Scree(grain, p), Rock(p, bedding, relief, grain), smoothstep(0.7, 0.95, fract(bedding)) * relief.steep * relief.strata);
}

/* Ground lies bare where it stands steep, more so where snow or sand slides off it, or high in the mountains, first as loose scree and then as rock, with edges ragged at every scale. */
Exposure ExposureAt(Relief relief, Grain grain, float blanket)
{
	float altitude = clamp(v_world.z / max(Peak(), MIN_RELIEF_SPAN), 0.0, 1.0);
	float ragged = (grain.local - 0.5) * 0.7 + (grain.fine - 0.5) * 0.3 + (grain.micro - 0.5) * 0.15;
	float bare = relief.steep * (STEEP_BARING + blanket * SHED_BARING) + smoothstep(ALPINE_ALTITUDE, 1.0, altitude) * ALPINE_BARING + ragged;
	float scree = smoothstep(SCREE_EXPOSURE - EXPOSURE_BLEND, SCREE_EXPOSURE + EXPOSURE_BLEND, bare);
	float rock = smoothstep(ROCK_EXPOSURE - EXPOSURE_BLEND, ROCK_EXPOSURE + EXPOSURE_BLEND, bare);
	return Exposure(scree, rock);
}

/* Above the snow line and in snowfields snow lingers on bare stone where it can lie: on the shelves atop the rock's layers and on the tops of stones. */
float SnowCaps(Site site)
{
	if (Landscape() != LANDSCAPE_ARCTIC) return 0.0;
	float snowy = max(site.climate.blanket, smoothstep(0.0, SNOW_LINE_PULL, SnowLift(v_world.z + site.ragged * SNOW_LINE_RAGGED)));
	float perch = max(site.bare.shelf, smoothstep(0.55, 0.85, site.grain.stones + site.grain.fine * 0.3 - 0.15));
	return snowy * perch * CAP_COVER;
}

vec3 Capped(vec3 stone, Site site)
{
	return mix(stone, CAP_SNOW * Varied(site.grain.micro, 0.08), SnowCaps(site));
}

Patch Exposed(Patch ground, Site site)
{
	vec3 tone = mix(mix(ground.tone, Capped(site.bare.scree, site), site.exposure.scree), Capped(site.bare.rock, site), site.exposure.rock);
	vec2 rugged = mix(mix(ground.rugged, SCREE_RUGGED, site.exposure.scree), ROCK_RUGGED, site.exposure.rock);
	return Patch(tone, rugged);
}

/* Rough land: tussocks of dry and green grass in clumps, opening onto patches of stony soil. */
Patch Rough(Grain grain, vec2 p, float lush)
{
	vec3 green = GrassTone(grain.broad, lush) * 0.9;
	vec3 dry = mix(green, ROUGH_TINT, 0.5);
	float clumps = smoothstep(0.35, 0.65, Octave(p, 2.3));
	float tussocks = smoothstep(0.4, 0.75, 0.6 * grain.micro + 0.4 * grain.fine);
	vec3 scrub = mix(green, dry, clumps) * mix(1.08, 0.84, tussocks) * Varied(grain.micro, 0.3);
	float open = smoothstep(0.58, 0.8, 0.65 * Octave(p + 3.3, 1.2) + 0.35 * grain.fine);
	vec3 stony = mix(Soil(), SCREE, 0.65) * mix(0.92, 1.15, clamp(grain.stones * 1.6, 0.0, 1.0)) * Varied(grain.micro, 0.3);
	return Patch(mix(scrub, stony, open * (Landscape() == LANDSCAPE_TOYLAND ? TOY_ROUGH_BARE : ROUGH_BARE)), ROUGH_RUGGED);
}

/* Rocky ground: grass with stones gathered in patches over it, and pale rock breaking through here and there, capped with snow where snow lies. */
Patch Rocks(Site site)
{
	Grain grain = site.grain;
	vec2 p = site.p;
	Bare bare = Bare(Capped(site.bare.scree, site), Capped(site.bare.rock, site), site.bare.shelf);
	float lush = site.climate.lush;
	float patches = smoothstep(0.48, 0.72, 0.7 * Noise(p * STONE_PATCHES_PER_TILE + 6.2) + 0.3 * grain.fine);
	float stones = patches * mix(STONE_GAPS, 1.0, clamp(grain.stones * 1.6, 0.0, 1.0));
	float outcrop = smoothstep(0.64, 0.74, 0.7 * Noise(p * OUTCROPS_PER_TILE + 2.9) + 0.3 * grain.fine);
	vec3 strewn = mix(Meadow(grain, lush), bare.scree * PALE_STONE, stones);
	return Patch(mix(strewn, bare.rock * PALE_STONE, outcrop), mix(mix(GRASS_RUGGED, SCREE_RUGGED, patches), ROCK_RUGGED, outcrop));
}

/* A field's crop grows in rows across it, ripening and thinning unevenly, with bare soil between the rows and a pair of wheel tracks every so often;
 * rows stay sharp seen end on and fade only as they crowd closer than the pixels. On a hillside it grows rough without rows, so no lattice is drawn over the slope. */
Patch Fields(Ground ground, Grain grain, vec2 p)
{
	vec3 crop = CROPS[int(ground.variant % uint(CROP_COUNT))] * Varied(Octave(p + 1.9, 0.6), 0.22) * Varied(Octave(p + 7.4, 2.1), 0.1) * Varied(grain.local, 0.1);
	bool rows_along_y = (ground.variant & 1u) == 0u;
	float across = rows_along_y ? p.x : p.y;
	float along = rows_along_y ? p.y : p.x;
	float row_pitch = Pitch(footprint, rows_along_y ? vec2(1.0, 0.0) : vec2(0.0, 1.0));
	float level = 1.0 - smoothstep(LEVEL_FIELD_SLOPE, HILLSIDE_FIELD_SLOPE, SlopeOf(normalize(v_normal)));
	float gap = abs(fract(across * FURROWS_PER_TILE) - 0.5) * 2.0;
	float furrow = smoothstep(0.45, 0.95, gap) * level * ResolvedAt(FURROWS_PER_TILE, 1.0 / row_pitch);
	float track_gap = abs(abs(fract(across * TRAMLINES_PER_TILE) - 0.5) / TRAMLINES_PER_TILE - TRAMLINE_GAUGE);
	float track = Stroke(track_gap, TRAMLINE_HALF_WIDTH, row_pitch) * level;
	float stalks = ResolvedAt(MICRO_PER_TILE, 1.0 / row_pitch);
	float plants = (stalks > 0.0 ? Varied(Noise(vec2(across * FURROWS_PER_TILE, along * MICRO_PER_TILE)), 0.3 * stalks) : 1.0) * Varied(grain.micro, 0.25);
	vec3 soil = mix(Soil(), crop, 0.25) * 0.85 * Varied(grain.fine, 0.2);
	return Patch(mix(mix(crop * plants, soil, furrow * FURROW_DEPTH), soil, track * TRAMLINE_DEPTH), FIELD_RUGGED);
}

bool CarriesWay(ivec2 tile)
{
	uvec4 ways = texelFetch(u_network, Clamped(tile), 0);
	return (ways.x | ways.y | ways.z) != 0u;
}

/* A shore tile carrying or beside a railway or a road is heaped with stone against the sea. */
bool Armoured(vec2 p)
{
	ivec2 tile = ivec2(floor(p));
	return CarriesWay(tile) || CarriesWay(tile + ivec2(1, 0)) || CarriesWay(tile - ivec2(1, 0)) || CarriesWay(tile + ivec2(0, 1)) || CarriesWay(tile - ivec2(0, 1));
}

/* A beach is wet sand at the water's edge drying up the shore, and grass where the shore rises well clear of the sea. */
Patch Beach(Grain grain)
{
	float lift = v_world.z + (grain.fine - 0.5) * 0.12 + (grain.micro - 0.5) * 0.05;
	vec3 sand = mix(WET_SAND, DRY_SAND, smoothstep(0.03, 0.22, lift)) * Varied(grain.local, 0.12) * Varied(grain.micro, 0.12);
	float grassed = smoothstep(0.45, 0.75, lift + (grain.local - 0.5) * 0.4);
	return Patch(mix(sand, Meadow(grain, 0.0), grassed), mix(SAND_RUGGED, GRASS_RUGGED, grassed));
}

/* Heaped stone armouring a shore: grey boulders, lit on top and dark in the gaps between. */
vec3 Riprap(vec2 p, Bare bare)
{
	float lumps = smoothstep(0.3, 0.7, Octave(p, BOULDERS_PER_TILE));
	vec3 stone = mix(bare.scree, ROCK, 0.5) * Varied(Octave(p + 7.7, BOULDERS_PER_TILE * 0.45), 0.3);
	return stone * mix(0.62, 1.12, lumps);
}

/* Beside a railway or a road the shore is armoured with heaped stone. */
Patch Shore(Site site)
{
	return site.armoured ? Patch(Riprap(site.p, site.bare), SCREE_RUGGED) : Beach(site.grain);
}

vec3 Steppe(Grain grain, float lush)
{
	return mix(Meadow(grain, lush) * STRAW_TINT, DRY_SAND * Varied(grain.micro, 0.15), 0.4);
}

Patch Albedo(Ground ground, Site site)
{
	Grain grain = site.grain;
	vec2 p = site.p;
	switch (ground.material) {
		case MAT_GRASS: return Patch(Grass(ground, grain, site.climate.lush), GRASS_RUGGED);
		case MAT_ROUGH:
		case MAT_SNOW: return Rough(grain, p, site.climate.lush);
		case MAT_ROCKS: return Rocks(site);
		case MAT_FIELDS: return Fields(ground, grain, p);
		case MAT_DESERT: return Patch(Steppe(grain, site.climate.lush), SAND_RUGGED);
		case MAT_SHORE: return Shore(site);
		case MAT_PAVED: return Patch(Paving() * (0.94 + 0.08 * grain.local) * Varied(grain.fine, 0.06) * Varied(grain.micro, 0.08), PAVED_RUGGED);
		case MAT_DIRT: return Patch(YARD * Varied(grain.local, 0.15) * Varied(grain.fine, 0.1) * Varied(grain.micro, 0.2), ROUGH_RUGGED);
	}
	return Patch(VOID_TONE, vec2(0.0));
}

bool IsBuilt(uint material)
{
	return material == MAT_FIELDS || material == MAT_PAVED || material == MAT_DIRT;
}

bool IsNatural(uint material)
{
	return material == MAT_GRASS || material == MAT_ROUGH || material == MAT_SNOW || material == MAT_DESERT || material == MAT_SHORE;
}

Patch Trodden(Patch ground, Site site)
{
	if (site.trodden <= 0.0) return ground;
	vec3 earth = PACKED_EARTH * Varied(site.grain.local, 0.14) * Varied(site.grain.fine, 0.08) * Varied(site.grain.micro, 0.12);
	return Patch(mix(ground.tone, earth, site.trodden), mix(ground.rugged, PAVED_RUGGED, site.trodden));
}

Patch Eroded(Ground ground, Site site)
{
	Patch patch = Albedo(ground, site);
	return IsNatural(ground.material) ? Trodden(Exposed(patch, site), site) : patch;
}

vec2 SettledAt(ivec2 tile)
{
	uvec4 codes = CodesAt(tile);
	float built = float(IsBuilt(codes.r));
	return vec2(built, max(built, float((codes.g & BUILT_BIT) != 0u)));
}

vec2 Settlement(vec2 p)
{
	vec2 q = p - TILE_CENTRE;
	ivec2 cell = ivec2(floor(q));
	vec2 f = fract(q);
	return mix(mix(SettledAt(cell), SettledAt(cell + ivec2(1, 0)), f.x), mix(SettledAt(cell + ivec2(0, 1)), SettledAt(cell + ivec2(1, 1)), f.x), f.y);
}

/* The grounds of the four tiles around blend across their seams; each distinct ground is shaded once, by the share all its corners hold, in a loop the compiler keeps as one copy. */
bool HasCorner(Ground corners[4], uint material)
{
	return corners[0].material == material || corners[1].material == material || corners[2].material == material || corners[3].material == material;
}

float Ragged(vec2 p, Grain grain)
{
	return (Octave(p, 0.17) - 0.5) * 0.5 + (Octave(p, 0.45) - 0.5) * 0.4 + (grain.local - 0.5) * 0.4 + (grain.fine - 0.5) * 0.2;
}

vec2 Drift(vec2 p, float frequency, float reach)
{
	vec2 q = OctaveTurn(frequency) * p * frequency;
	return (vec2(Noise(q + 3.3), Noise(q + 9.1)) - 0.5) * 2.0 * reach;
}

/* The stone is only worked out where some ground at the point shows it. */
Site SiteAt(vec2 p, Grain grain, Relief relief, bool armoured, Ground corners[4], vec2 warp, float settled)
{
	Climate climate = Climate(0.0, 0.0);
	float ragged = 0.0;
	float trodden = 0.0;
	if (Landscape() == LANDSCAPE_ARCTIC || Landscape() == LANDSCAPE_TROPIC) {
		climate = ClimateAt(p + warp + Drift(p, CLIMATE_SWAY_FREQUENCY, CLIMATE_SWAY) + Drift(p, CLIMATE_FRAY_FREQUENCY, CLIMATE_FRAY));
		ragged = Ragged(p, grain);
		climate.lush = smoothstep(0.15, 0.85, climate.lush + ragged * 0.6);
		if (Landscape() == LANDSCAPE_TROPIC) trodden = smoothstep(0.05, 0.4, settled + ragged * 0.4);
	}
	Exposure exposure = ExposureAt(relief, grain, climate.blanket);
	bool bared = exposure.scree > 0.0 || armoured || HasCorner(corners, MAT_ROCKS);
	Bare bare = bared ? BareAt(p, relief, grain) : NO_BARE;
	return Site(p, grain, relief, exposure, bare, armoured, climate, ragged, trodden);
}

Patch Surface(vec2 p, vec2 bend, Grain grain, Relief relief, bool armoured, out Site site)
{
	vec2 settlement = Settlement(p);
	float built = min(settlement.x * 2.0, 1.0) * smoothstep(BLOCKS_BLUR_PIXELS, BLOCKS_SHOWN_PIXELS, tile_pixels);
	float reach = mix(NATURAL_WARP, BUILT_WARP, built);
	float width = mix(BLEND_WIDTH, max(BUILT_BLEND_WIDTH, BLEND_PIXELS / tile_pixels), built);
	vec2 warp = bend * reach;
	vec2 q = p + warp - TILE_CENTRE;
	ivec2 cell = ivec2(floor(q));
	vec2 f = smoothstep(HALF_TILE - width, HALF_TILE + width, fract(q));

	Ground corners[4] = Ground[4](GroundAt(cell), GroundAt(cell + ivec2(1, 0)), GroundAt(cell + ivec2(0, 1)), GroundAt(cell + ivec2(1, 1)));
	float shares[4] = float[4]((1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y), (1.0 - f.x) * f.y, f.x * f.y);
	site = SiteAt(p, grain, relief, armoured, corners, bend * CLIMATE_WARP, settlement.y);
	Ground distinct[4];
	float weights[4];
	int count = 0;
	for (int i = 0; i < 4; i++) {
		if (shares[i] <= 0.0) continue;
		int k = 0;
		while (k < count && distinct[k] != corners[i]) k++;
		if (k == count) {
			distinct[k] = corners[i];
			weights[k] = 0.0;
			count++;
		}
		weights[k] += shares[i];
	}

	Patch blend = Patch(vec3(0.0), vec2(0.0));
	for (int k = 0; k < count; k++) {
		Patch patch = Eroded(distinct[k], site);
		blend.tone += patch.tone * weights[k];
		blend.rugged += patch.rugged * weights[k];
	}
	return blend;
}

bool SameField(ivec2 tile, Ground field)
{
	Ground ground = GroundAt(tile);
	return ground.material == MAT_FIELDS && ground.variant == field.variant;
}

/* A hedge with gaps here and there runs along every side of a field where the next tile is not the same field. */
vec4 Hedgerow(vec2 p, Grain grain)
{
	ivec2 tile = ivec2(floor(p));
	Ground home = GroundAt(tile);
	if (home.material != MAT_FIELDS) return vec4(0.0);
	vec2 f = fract(p);
	float near = 1.0;
	if (!SameField(tile - ivec2(1, 0), home)) near = min(near, f.x);
	if (!SameField(tile + ivec2(1, 0), home)) near = min(near, 1.0 - f.x);
	if (!SameField(tile - ivec2(0, 1), home)) near = min(near, f.y);
	if (!SameField(tile + ivec2(0, 1), home)) near = min(near, 1.0 - f.y);
	float width = HEDGE_WIDTH * (0.6 + 0.8 * grain.fine);
	float gaps = smoothstep(0.25, 0.4, Noise(p * HEDGE_GAPS_PER_TILE + 4.4));
	float pixel = 1.0 / tile_pixels;
	float hedge = Stroke(near, width, pixel) * gaps;
	float fringe = Stroke(near, width * HEDGE_FRINGE, pixel) * gaps * (1.0 - hedge);
	vec4 leaves = vec4(HEDGE * Varied(grain.micro, 0.6) * Varied(grain.clump, 0.4), 1.0) * hedge * HEDGE_OPACITY;
	return leaves + vec4(0.0, 0.0, 0.0, fringe * HEDGE_FRINGE_SHADE);
}

/* Fine bumps and stones catch the light up close: soft on grass and fields, sharp on scree and rock. */
vec3 Roughened(vec3 normal, Detail detail, vec2 rugged)
{
	return normalize(normal - (detail.slope * rugged.x + detail.stone_slope * rugged.y) * RELIEF_DEPTH);
}

/* Whether no tile within the reach the water's field weighs about a tile holds any water, from the water's texels eight tiles to a texel, each of which keeps any water under it however little. */
bool DryAbout(ivec2 tile)
{
	ivec2 low = Clamped(tile - DRY_REACH) >> DRY_LEVEL;
	ivec2 high = Clamped(tile + DRY_REACH) >> DRY_LEVEL;
	float water = texelFetch(u_water, low, DRY_LEVEL).r + texelFetch(u_water, ivec2(high.x, low.y), DRY_LEVEL).r;
	water += texelFetch(u_water, ivec2(low.x, high.y), DRY_LEVEL).r + texelFetch(u_water, high, DRY_LEVEL).r;
	return water <= 0.0;
}

Water WaterAt(vec2 p)
{
	vec4 near = DryAbout(ivec2(floor(p))) ? vec4(0.0) : WaterField(p);
	float wide = textureLod(u_water, p / MapSize(), DEPTH_LOD).r;
	float edge = max(fwidth(near.r), 1e-3);

	Water water;
	water.level = near.r;
	water.cover = smoothstep(WATERLINE - edge, WATERLINE + edge, near.r);
	water.depth = smoothstep(0.55, 1.0, wide) * smoothstep(0.5, 0.9, near.r);
	water.share = near.gba / max(near.r, 1e-3);
	water.sea = near.g;
	water.canal = near.b;
	return water;
}

vec3 Banks(vec3 land, Water water, vec3 beach)
{
	vec3 damp = land * (1.0 - 0.18 * smoothstep(0.28, 0.5, water.level));
	vec3 walled = mix(damp, BANK, smoothstep(0.3, 0.46, water.canal));
	return mix(walled, beach, smoothstep(SANDED_SEA, 0.44, water.sea));
}

/* The ground under water: wet sand along sea shores giving way to silt in the deep, mud under rivers and dressed stone in canals. */
vec3 Seabed(Water water, Grain grain)
{
	float sea = clamp(water.share.x, 0.0, 1.0);
	float canal = clamp(water.share.y, 0.0, 1.0 - sea);
	float river = 1.0 - sea - canal;
	vec3 bed = mix(WET_SAND, SILT, water.depth) * sea + CANAL_FLOOR * canal + MUD * river;
	return bed * (0.92 + 0.16 * grain.local) * Varied(grain.micro, 0.12);
}

float TileCover(ivec2 tile)
{
	return FloraCover(CodesAt(tile));
}

/* The ground under a forest: kept from the sky by the crowns, and painted their colour where distance has thinned the trees themselves away. */
Canopy Forest(vec2 p)
{
	vec2 q = p - TILE_CENTRE;
	ivec2 cell = ivec2(floor(q));
	vec2 f = smoothstep(0.0, 1.0, fract(q));
	float density = mix(mix(TileCover(cell), TileCover(cell + ivec2(1, 0)), f.x), mix(TileCover(cell + ivec2(0, 1)), TileCover(cell + ivec2(1, 1)), f.x), f.y);
	if (density <= 0.0) return Canopy(vec4(0.0), 0.0);

	uvec4 home = CodesAt(ivec2(floor(p)));
	uint kind = TreeCount(home) > 0 ? home.b >> FLORA_KIND_SHIFT : TREE_BROADLEAF;
	float tint = ForestTint(TilePixelsAt(distance(Eye(), RenderPoint(v_world))));
	vec3 tone = TREE_CANOPY[kind] * Varied(Octave(p, 1.1), CANOPY_VARIETY);
	return Canopy(vec4(tone, 1.0) * density * FOREST_COVER * tint, density * CANOPY_OCCLUSION);
}

float GridLine(vec2 p, mat2 pixel)
{
	vec2 gap = abs(fract(p + HALF_TILE) - HALF_TILE);
	float x_edges = PixelLine(gap.x, Pitch(pixel, vec2(1.0, 0.0)), LINE_HALF_PIXELS);
	float y_edges = PixelLine(gap.y, Pitch(pixel, vec2(0.0, 1.0)), LINE_HALF_PIXELS);
	return max(x_edges, y_edges);
}

vec3 Overlay(vec3 colour, Network network)
{
	if (u_layer == LAYER_NONE) return colour;
	return mix(Greyed(colour), Accent(u_layer), u_layer == LAYER_RAIL ? network.layers.x : network.layers.y);
}

bool OutsideMap(vec2 p)
{
	ivec2 tile = ivec2(floor(p));
	return !OnMap(tile) || texelFetch(u_tiles, tile, 0).r == MAT_VOID;
}

/* Past the map's edge the bed deepens from what lies along the edge to the open sea's silt across the shelf, so it meets the bed inside without a seam. */
float OffshoreDepth(vec2 p)
{
	vec2 edge = clamp(p, vec2(0.0), MapSize());
	float wide = textureLod(u_water, edge / MapSize(), DEPTH_LOD).r;
	return mix(smoothstep(0.55, 1.0, wide), 1.0, smoothstep(0.0, SHELF_TILES, distance(p, edge)));
}

uint MarkAt(ivec2 tile)
{
	uvec4 codes = CodesAt(tile);
	return codes.r == MAT_PAVED ? codes.a : AIRFIELD_NONE;
}

bool Taxiable(uint mark)
{
	return mark == AIRFIELD_TAXIWAY || mark == AIRFIELD_RUNWAY || mark == AIRFIELD_STAND;
}

float AirfieldStroke(float gap, float half_width)
{
	return Stroke(gap, half_width, 1.0 / tile_pixels);
}

/* A runway lies along the axis its neighbouring runway tiles run; it carries edge lines, a dashed centre line, edge lights and, where it ends, threshold bars. */
vec4 Runway(ivec2 tile, vec2 f, vec2 p)
{
	bool along_x = MarkAt(tile + ivec2(1, 0)) == AIRFIELD_RUNWAY || MarkAt(tile - ivec2(1, 0)) == AIRFIELD_RUNWAY;
	ivec2 onward = along_x ? ivec2(1, 0) : ivec2(0, 1);
	float along = along_x ? f.x : f.y;
	float across = abs((along_x ? f.y : f.x) - HALF_TILE);
	float run = along_x ? p.x : p.y;

	float white = AirfieldStroke(abs(across - RUNWAY_EDGE), RUNWAY_EDGE_HALF);
	float dash = step(fract(run * CENTRELINE_DASHES), 0.55);
	white = max(white, AirfieldStroke(across, CENTRELINE_HALF) * dash);
	bool ends_ahead = MarkAt(tile + onward) != AIRFIELD_RUNWAY;
	bool ends_behind = MarkAt(tile - onward) != AIRFIELD_RUNWAY;
	float inward = ends_ahead ? along : (ends_behind ? 1.0 - along : 0.0);
	if ((ends_ahead || ends_behind) && across < THRESHOLD_REACH && inward > THRESHOLD_FROM && inward < THRESHOLD_TO) {
		white = max(white, AirfieldStroke(abs(fract(across * THRESHOLD_BARS) - HALF_TILE), 0.25));
	}
	vec3 colour = mix(RUNWAY_ASPHALT, AIRFIELD_WHITE, white);
	float light = 1.0 - smoothstep(EDGE_LIGHT_RADIUS * 0.5, EDGE_LIGHT_RADIUS, length(vec2(fract(run * EDGE_LIGHTS) - HALF_TILE, (across - EDGE_LIGHT_LATERAL) * EDGE_LIGHTS) / EDGE_LIGHTS));
	return vec4(mix(colour, EDGE_LIGHT, light * Resolved(EDGE_LIGHTS * 2.0)), 1.0);
}

/* A taxiway's yellow centre line runs from the tile's middle toward each neighbour aircraft taxi on to. */
float TaxiLines(ivec2 tile, vec2 f)
{
	vec2 off = f - HALF_TILE;
	float line = 0.0;
	if (Taxiable(MarkAt(tile + ivec2(1, 0))) && off.x > -TAXI_LINE_HALF) line = max(line, AirfieldStroke(abs(off.y), TAXI_LINE_HALF));
	if (Taxiable(MarkAt(tile - ivec2(1, 0))) && off.x < TAXI_LINE_HALF) line = max(line, AirfieldStroke(abs(off.y), TAXI_LINE_HALF));
	if (Taxiable(MarkAt(tile + ivec2(0, 1))) && off.y > -TAXI_LINE_HALF) line = max(line, AirfieldStroke(abs(off.x), TAXI_LINE_HALF));
	if (Taxiable(MarkAt(tile - ivec2(0, 1))) && off.y < TAXI_LINE_HALF) line = max(line, AirfieldStroke(abs(off.x), TAXI_LINE_HALF));
	return line;
}

vec3 Concrete(vec2 f)
{
	vec2 joints = abs(fract(f * PANEL_JOINTS + HALF_TILE) - HALF_TILE) / PANEL_JOINTS;
	float joint = max(AirfieldStroke(joints.x, PANEL_JOINT_HALF), AirfieldStroke(joints.y, PANEL_JOINT_HALF));
	return APRON_CONCRETE * (1.0 - 0.18 * joint * Resolved(PANEL_JOINTS * 2.0));
}

/* An airport's paved tiles are painted as what they are: runways, taxiways, stands and helipads on concrete aprons. */
vec4 Airfield(vec2 p)
{
	ivec2 tile = ivec2(floor(p));
	uint mark = MarkAt(tile);
	if (mark == AIRFIELD_NONE) return vec4(0.0);
	vec2 f = fract(p);
	if (mark == AIRFIELD_RUNWAY) return Runway(tile, f, p);

	vec3 colour = Concrete(f);
	vec2 off = abs(f - HALF_TILE);
	if (mark == AIRFIELD_TAXIWAY || mark == AIRFIELD_STAND) colour = mix(colour, TAXI_YELLOW, TaxiLines(tile, f));
	if (mark == AIRFIELD_STAND) colour = mix(colour, TAXI_YELLOW, AirfieldStroke(abs(max(off.x, off.y) - STAND_BOX), TAXI_LINE_HALF));
	if (mark == AIRFIELD_HELIPAD) {
		float ring = AirfieldStroke(abs(length(f - HALF_TILE) - HELIPAD_RING), HELIPAD_LINE_HALF);
		float letter = max(AirfieldStroke(abs(off.x - 0.1), HELIPAD_LINE_HALF) * step(off.y, 0.15), AirfieldStroke(off.y, HELIPAD_LINE_HALF) * step(off.x, 0.1));
		colour = mix(colour, AIRFIELD_WHITE, max(ring, letter));
	}
	return vec4(colour, 1.0);
}

/* Only foundations part the ground into walls, and they are laid in dressed stone courses. */
vec3 WallFace(vec3 normal)
{
	float along = dot(v_world.xy, vec2(-normal.y, normal.x));
	float course = v_world.z * LevelRise() * COURSES_PER_TILE;
	float block = along * BLOCKS_PER_TILE + 0.5 * floor(course);
	vec2 joint = abs(fract(vec2(block, course)) - 0.5);
	float mortar = smoothstep(0.5 - MORTAR_SHARE, 0.5, max(joint.x, joint.y));
	float detail = Resolved(COURSES_PER_TILE);
	vec3 stone = MASONRY * Varied(Hash(ivec2(floor(block), floor(course))), 0.18 * detail) * (1.0 - 0.3 * mortar * detail);
	return Altitude(stone, v_world.z);
}

vec3 EarthStratum(int layer)
{
	if (Landscape() == LANDSCAPE_TOYLAND) return TOY_STRATA[abs(layer) % TOY_STRATA_COUNT];
	int pick = int(Hash(ivec2(layer, 23)) * float(EARTH_STRATA_COUNT)) % EARTH_STRATA_COUNT;
	return EARTH_STRATA[pick] * Varied(Hash(ivec2(layer, 29)), EDGE_LAYER_VARIETY);
}

/* The strata blended as they read from too far to make out one from another. */
vec3 EarthMean()
{
	if (Landscape() == LANDSCAPE_TOYLAND) return ToyStrataMean();
	return (EARTH_STRATA[0] + EARTH_STRATA[1] + EARTH_STRATA[2] + EARTH_STRATA[3] + EARTH_STRATA[4]) / float(EARTH_STRATA_COUNT);
}

/* The level of the ground along the top of the map's edge, on the tile just inside it. */
float EdgeTop(vec3 normal)
{
	ivec2 tile = Clamped(ivec2(floor(v_world.xy - normal.xy * HALF_TILE)));
	return FacetLevel(SurfaceOf(tile), clamp(v_world.xy - vec2(tile), 0.0, 1.0));
}

/* How far below the ground's edge a band reaching this far down still shows, its foot ragged with roots and wavering along the cut. */
float Fringe(float along, float below, float reach, float pixels)
{
	float ragged = reach * Varied(Noise(vec2(along * EDGE_TOPSOIL_SWAY, 8.1)), EDGE_TOPSOIL_SWING) * Varied(OctaveAt(vec2(along, 2.9), EDGE_ROOTS_PER_TILE, pixels), EDGE_ROOTS_RAGGED);
	return 1.0 - smoothstep(ragged * 0.4, ragged, below);
}

/* The map's edge is cut through the land as a face of earth: the turf of the ground above hanging over its lip, dark topsoil, then layers of soil and rock wavering a little along it, dark with wet where the sea washes it. */
vec3 EdgeFace(vec3 normal, float pixels, vec3 turf)
{
	float along = dot(v_world.xy, vec2(-normal.y, normal.x));
	float depth = v_world.z * LevelRise();
	float bedding = (depth + (Noise(vec2(along * EDGE_STRATA_SWAY, 4.3)) - 0.5) * EDGE_STRATA_WARP) * EDGE_STRATA_PER_TILE;
	int layer = int(floor(bedding));
	float bands = ResolvedAt(EDGE_STRATA_PER_TILE, pixels);
	float below = (EdgeTop(normal) - v_world.z) * LevelRise();
	float dry = smoothstep(0.0, EDGE_WET_REACH, depth);
	vec3 earth = mix(mix(EarthMean(), EarthStratum(layer), bands), Soil() * EDGE_TOPSOIL_SHADE, Fringe(along, below, EDGE_TOPSOIL, pixels) * dry);
	float seam = mix(1.0, mix(EDGE_SEAM_SHADE, 1.0, smoothstep(0.0, 0.25, fract(bedding))), bands);
	float grain = Varied(OctaveAt(vec2(along, depth), EDGE_GRAIN_PER_TILE, pixels), 0.25);
	float wet = mix(EDGE_WET_SHADE, 1.0, smoothstep(0.0, EDGE_WET_REACH, depth));
	return mix(earth * seam * grain * wet, turf, Fringe(along + 5.3, below, EDGE_TURF, pixels) * dry);
}

vec3 Windblown(vec2 p, float along, float across)
{
	vec2 frequency = vec2(along, across);
	vec3 noise = NoiseSlope(vec2(dot(p, WIND), dot(p, ACROSS_WIND)) * frequency);
	vec2 slope = noise.yz * frequency;
	return vec3(noise.x, WIND * slope.x + ACROSS_WIND * slope.y);
}

float Glint(vec2 p, vec3 normal)
{
	if (tile_pixels <= GLINT_FROM_PIXELS) return 0.0;
	float level = floor(log2(max(tile_pixels / GLINT_PIXELS, 1.0)));
	vec2 q = p * exp2(level);
	ivec2 cell = ivec2(floor(q)) + ivec2(int(level) * GLINT_LEVEL_STRIDE, 0);
	vec2 dice = Hash2(cell);
	if (dice.x > GLINT_SHARE) return 0.0;
	vec2 spot = Hash2(cell + ivec2(0, GLINT_LEVEL_STRIDE));
	float point = 1.0 - smoothstep(0.08, 0.3, length(fract(q) - 0.25 - 0.5 * spot));
	vec3 facet = normalize(normal + vec3((Hash2(cell + ivec2(GLINT_LEVEL_STRIDE, 0)) - 0.5) * GLINT_SPREAD, 0.0));
	vec3 half_way = normalize(SunDirection() + normalize(Eye() - RenderPoint(v_world)));
	return point * pow(max(dot(facet, half_way), 0.0), GLINT_FOCUS) * smoothstep(GLINT_FROM_PIXELS, GLINT_TO_PIXELS, tile_pixels);
}

/* Across miles a blanket lies in broad sheets, snow wind packed grey and blue in some and fresh in others, sand baked darker in some and pale in others. */
float Broad(vec2 p)
{
	return smoothstep(0.35, 0.72, Layered(p + 31.0, BROAD_FREQUENCY));
}

Blanket Snowfield(Site site, float cover)
{
	vec2 p = site.p;
	vec3 drift = Windblown(p + 3.7, DRIFT_ALONG, DRIFT_ACROSS);
	vec2 slope = drift.yz * DRIFT_HEIGHT * Resolved(DRIFT_ACROSS);
	float ridges = Resolved(RIDGE_ACROSS);
	if (ridges > 0.0) slope += Windblown(p + 1.3, RIDGE_ALONG, RIDGE_ACROSS).yz * RIDGE_HEIGHT * ridges;
	float sastrugi = Resolved(SASTRUGI_ACROSS);
	if (sastrugi > 0.0) slope += Windblown(p, SASTRUGI_ALONG, SASTRUGI_ACROSS).yz * SASTRUGI_HEIGHT * sastrugi;
	float scoured = smoothstep(0.55, 0.85, Octave(p + 9.1, 0.7)) * (1.0 - drift.x);
	vec3 tone = mix(SNOW, SCOURED_SNOW, scoured) * mix(0.95, 1.02, drift.x) * Varied(site.grain.fine, 0.05) * Varied(site.grain.micro, 0.06);
	tone = mix(tone, tone * PACKED_SNOW_TINT, Broad(p));
	return Blanket(cover, tone, slope, SNOW_RUGGED, SNOW_ROUGHNESS, 1.0);
}

Dune DuneTrain(vec2 p, vec2 wind, float spacing, float seed)
{
	vec3 warp_x = NoiseSlope(p * DUNE_WARP_FREQUENCY + seed);
	vec3 warp_y = NoiseSlope(p * DUNE_WARP_FREQUENCY + seed + 31.7);
	float reach = 2.0 * DUNE_WARP;
	vec2 q = p + (vec2(warp_x.x, warp_y.x) - 0.5) * reach;
	vec3 stretch = NoiseSlope(OctaveTurn(DUNE_STRETCH_FREQUENCY) * p * DUNE_STRETCH_FREQUENCY + seed + 19.3);
	float spread = 2.0 * DUNE_STRETCH * spacing;
	vec2 heading = wind + reach * DUNE_WARP_FREQUENCY * (wind.x * warp_x.yz + wind.y * warp_y.yz) + spread * DUNE_STRETCH_FREQUENCY * (stretch.yz * OctaveTurn(DUNE_STRETCH_FREQUENCY));
	float along = dot(q, wind) + (stretch.x - 0.5) * spread;
	float s = fract(along / spacing);
	bool windward = s < DUNE_CREST;
	float t = windward ? s / DUNE_CREST : (1.0 - s) / (1.0 - DUNE_CREST);
	float rate = (windward ? 1.0 / DUNE_CREST : -1.0 / (1.0 - DUNE_CREST)) / spacing;
	float profile = t * t * (3.0 - 2.0 * t);
	vec3 swell = NoiseSlope(p * DUNE_SWELL_FREQUENCY + seed + 5.3);
	float u = clamp((swell.x - DUNE_CALM) / (1.0 - 2.0 * DUNE_CALM), 0.0, 1.0);
	float amplitude = u * u * (3.0 - 2.0 * u);
	vec2 swelling = 6.0 * u * (1.0 - u) / (1.0 - 2.0 * DUNE_CALM) * DUNE_SWELL_FREQUENCY * swell.yz;
	vec2 slope = 6.0 * t * (1.0 - t) * rate * heading * amplitude + profile * swelling;
	return Dune(profile * amplitude, slope, along, heading, windward ? 0.0 : 1.0);
}

struct Ripples {
	float shade;
	vec2 slope;
};

Ripples RipplesAlong(Dune dune, float bent, float per_tile)
{
	float phase = (dune.along + bent) * per_tile * TAU;
	return Ripples(sin(phase), cos(phase) * dune.heading * per_tile * TAU * RIPPLE_HEIGHT);
}

/* The two trains of dunes, and the ripples running across each, take turns to lead from one stretch of desert to the next, so neither repeats unbroken across it. */
Blanket Sandfield(Site site, float cover)
{
	vec2 p = site.p;
	Dune dune = DuneTrain(p, WIND, DUNE_SPACING, 0.0);
	Dune cross = DuneTrain(p, CROSS_WIND, DUNE_SPACING * CROSS_SPACING, 13.1);
	float share = mix(CROSS_SHARE_LOW, CROSS_SHARE_HIGH, smoothstep(0.3, 0.7, Noise(OctaveTurn(CROSS_SHARE_FREQUENCY) * p * CROSS_SHARE_FREQUENCY + 5.9)));
	float height = (dune.height + cross.height * share) / (1.0 + share);
	vec2 dune_slope = (dune.slope + cross.slope * share) * DUNE_HEIGHT;

	vec2 wobble = vec2(Noise(p * 0.9 + 2.1), Noise(p * 1.3 + 7.7)) - 0.5;
	float crossing = smoothstep(0.35, 0.65, Noise(OctaveTurn(RIPPLE_BLEND_FREQUENCY) * p * RIPPLE_BLEND_FREQUENCY + 1.7));
	Ripples along = RipplesAlong(dune, dot(wobble * RIPPLE_WARP, dune.heading), RIPPLES_PER_TILE);
	Ripples athwart = RipplesAlong(cross, dot(wobble * RIPPLE_WARP, cross.heading), CROSS_RIPPLES_PER_TILE);
	Ripples ripples = Ripples(mix(along.shade, athwart.shade, crossing), mix(along.slope, athwart.slope, crossing));
	float lee = mix(dune.lee, cross.lee, crossing);

	float gravel = smoothstep(0.72, 0.86, (1.0 - height) * 0.3 + Octave(p + 4.1, 0.45) * 0.7 + (site.grain.fine - 0.5) * 0.15);
	float rippled = Resolved(RIPPLES_PER_TILE) * (1.0 - 0.7 * lee) * (1.0 - gravel);
	vec2 slope = dune_slope * (1.0 - 0.6 * gravel) + ripples.slope * rippled;
	vec3 sand = mix(SAND, RED_SAND, Layered(p + 8.3, 0.05)) * mix(0.93, 1.04, height) * (1.0 + RIPPLE_SHADE * ripples.shade * rippled) * Varied(site.grain.micro, 0.08);
	sand = mix(sand, sand * BAKED_SAND_TINT, Broad(p));
	vec3 stones = GRAVEL * Varied(site.grain.local, 0.15) * Varied(site.grain.micro, 0.3) * mix(0.9, 1.12, site.grain.stones);
	return Blanket(cover, mix(sand, stones, gravel), slope, mix(SAND_RUGGED, SCREE_RUGGED, gravel * 0.6), SAND_ROUGHNESS, 0.0);
}

Blanket BlanketOver(Site site)
{
	if (site.climate.blanket <= 0.0) return NO_BLANKET;
	float fray = (Octave(site.p + 2.7, BLANKET_FRAY_FREQUENCY) - 0.5) * BLANKET_FRAY;
	float edge = Landscape() == LANDSCAPE_ARCTIC ? SNOW_EDGE : SAND_EDGE;
	float lying = smoothstep(0.5 - edge, 0.5 + edge, site.climate.blanket + SnowLift(v_world.z + site.ragged * SNOW_LINE_RAGGED) + site.ragged + fray - site.trodden);
	float slip = smoothstep(BLANKET_SLIP_SLOPE, STEEPEST_SLOPE, site.relief.slope + site.ragged * 0.8);
	float clinging = smoothstep(0.55, 0.8, site.grain.stones * 0.6 + site.grain.fine * 0.4) * CLINGING;
	float cover = lying * (1.0 - slip * (1.0 - clinging));
	if (cover <= 0.0) return NO_BLANKET;
	return Landscape() == LANDSCAPE_ARCTIC ? Snowfield(site, cover) : Sandfield(site, cover);
}

vec2 Bend(vec2 p)
{
	return (vec2(Noise(p * 0.9), Noise(p * 0.9 + 41.7)) - 0.5) * 2.0;
}

/* The ground's own colour where it meets the map's edge, as it would lie level there. */
vec3 EdgeTurf(Detail detail, float levels_per_pixel)
{
	vec2 p = v_world.xy;
	Grain grain = GrainAt(p, detail);
	Site site;
	Patch ground = Surface(p, Bend(p), grain, ReliefAt(UPRIGHT, levels_per_pixel), false, site);
	Blanket blanket = BlanketOver(site);
	return mix(Altitude(ground.tone, v_world.z), blanket.tone, blanket.cover);
}

/* Where the network's meshes stand opaque they hide its bands, which are then not worked out at all. */
Network BandsAt(vec2 p, mat2 pixel)
{
	if (tile_pixels > NETWORK_OPAQUE_PPT * HIDDEN_BAND_MARGIN) return Network(vec4(0.0), vec2(0.0));
	return NetworkAt(p, pixel);
}

Shade GroundShade(vec2 p, mat2 pixel, Water water, Relief relief, Detail detail, float levels_per_pixel, vec3 normal)
{
	Grain grain = GrainAt(p, detail);
	vec2 bend = Bend(p);
	Canopy forest = Forest(p);
	Network network = BandsAt(p, pixel);
	float grid = clamp((tile_pixels - INFRASTRUCTURE_PPT) / GRID_FADE_PPT, 0.0, 1.0);
	Site site;
	Patch surface = Surface(p, bend, grain, relief, water.sea > 0.0 && Armoured(p), site);
	Blanket blanket = BlanketOver(site);
	float dry = 1.0 - water.cover;
	float blanketed = blanket.cover * dry;
	normal = normalize(normal - vec3(blanket.slope * blanketed, 0.0));
	normal = Roughened(normal, detail, mix(surface.rugged, blanket.rugged, blanket.cover) * dry);
	vec3 beach = water.sea > SANDED_SEA ? Shore(site).tone : vec3(0.0);
	vec3 land = Altitude(Composite(Composite(surface.tone, Hedgerow(p, grain)), Airfield(p)), v_world.z);
	land = mix(land, blanket.tone, blanket.cover);
	float drowned = max(clamp(-v_mark / SUBMERGED_MARK, 0.0, 1.0), water.sea * smoothstep(0.0, DROWNED_LEVELS, -v_world.z));
	land = mix(Banks(land, water, beach), Seabed(water, grain), drowned);
	land *= 1.0 - (1.0 - grid) * u_contour * CONTOUR_DEPTH * Contour(v_world.z, levels_per_pixel) * (1.0 - water.cover);

	vec3 colour = Composite(land, network.paint);
	colour *= 1.0 - grid * u_grid * GRID_DEPTH * GridLine(p, pixel) * (1.0 - water.cover);
	colour = Composite(colour, forest.paint);
	float roughness = mix(GroundRoughness(), blanket.roughness, blanket.cover);
	float glint = blanket.glint > 0.0 ? Glint(p, normal) * blanketed * (1.0 - forest.paint.a) : 0.0;
	return Shade(Overlay(clamp(colour, 0.0, 1.0), network), normal, roughness, 1.0 - forest.shade, glint);
}

/* Everything read through screen derivatives is read before the fragment branches, where neighbouring pixels may part ways.
 * Detail fades by the pixel's mean span, so ground seen at a glancing angle keeps its grain for the antialiasing to settle, and the finest by its longest. */
void main()
{
	vec2 p = v_world.xy;
	mat2 pixel = mat2(dFdx(p), dFdy(p));
	tile_pixels = 1.0 / max(sqrt(length(pixel[0]) * length(pixel[1])), MIN_SPREAD);
	footprint = pixel;
	float levels_per_pixel = max(fwidth(v_world.z), MIN_SPREAD);
	Water water = WaterAt(p);
	vec3 normal = normalize(v_normal);
	Relief relief = ReliefAt(normal, levels_per_pixel);
	Detail detail = DetailAt(RenderPoint(v_world), normal);
	relief.steep *= 1.0 - clamp(v_mark / FILL_MARK, 0.0, 1.0);
	float face_pixels = 1.0 / max(max(length(pixel[0]), length(pixel[1])), levels_per_pixel * LevelRise());

	Shade shade;
	if (v_mark > EDGE_MARK && v_mark <= WALL_MARK) {
		shade = Shade(Greyed(EdgeFace(normal, face_pixels, EdgeTurf(detail, levels_per_pixel))), normal, TERRAIN_ROUGHNESS, 1.0, 0.0);
	} else if (OutsideMap(p)) {
		shade = Shade(Greyed(Seabed(Water(1.0, 1.0, OffshoreDepth(p), vec3(1.0, 0.0, 0.0), 1.0, 0.0), GrainAt(p, detail))), normal, TERRAIN_ROUGHNESS, 1.0, 0.0);
	} else if (v_mark > WALL_MARK) {
		shade = Shade(Greyed(WallFace(normal)), normal, TERRAIN_ROUGHNESS, 1.0, 0.0);
	} else {
		shade = GroundShade(p, pixel, water, relief, detail, levels_per_pixel, normal);
	}
	frag_colour = Lit(shade);
}
