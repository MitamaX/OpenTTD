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
const float ALPINE_BARING = 0.9;
const float SCREE_EXPOSURE = 0.5;
const float ROCK_EXPOSURE = 0.9;
const float EXPOSURE_BLEND = 0.14;
const float SNOW_SLIP_SLOPE = 1.15;

const float LINE_HALF_PIXELS = 0.5;
const float HALF_LEVEL = 0.5;
const float CONTOUR_DEPTH = 0.75;
const float INDEX_CONTOUR_EVERY = 5.0;
const float INDEX_CONTOUR_WEIGHT = 1.6;
const float CROWDED_CONTOUR_PIXELS = 3.0;
const float SPACED_CONTOUR_PIXELS = 9.0;
const float MIN_RELIEF_SPAN = 6.0;
const vec3 LOWLAND = vec3(0.90, 0.95, 0.86);
const vec3 HIGHLAND = vec3(1.12, 1.06, 0.94);
const float TERRAIN_ROUGHNESS = 0.85;
const float WALL_MARK = 0.5;
const float FILL_MARK = 0.25;
const float SUBMERGED_MARK = 0.08;
const float DROWNED_LEVELS = 0.6;

const float BLEND_WIDTH = 0.28;
const float BUILT_BLEND_WIDTH = 0.03;
const float BLEND_PIXELS = 0.8;
const float BLOCKS_BLUR_PIXELS = 4.0;
const float BLOCKS_SHOWN_PIXELS = 12.0;
const float NATURAL_WARP = 0.32;
const float BUILT_WARP = 0.06;
const float DEPTH_LOD = 3.0;
const float SANDED_SEA = 0.26;

const vec3 SOIL = vec3(0.45, 0.37, 0.27);
const vec3 ROUGH_TINT = vec3(0.55, 0.50, 0.30);
const vec3 STRAW_TINT = vec3(1.14, 1.04, 0.78);
const vec3 ROCK = vec3(0.50, 0.49, 0.47);
const vec3 ROCK_WARM = vec3(0.58, 0.52, 0.44);
const vec3 SCREE = vec3(0.55, 0.52, 0.47);
const vec3 SNOW = vec3(0.93, 0.95, 0.98);
const vec3 SAND = vec3(0.86, 0.77, 0.55);
const vec3 DRY_SAND = vec3(0.78, 0.71, 0.55);
const vec3 PAVING = vec3(0.57, 0.55, 0.51);
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
	bool lush;
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

/* The loose and the solid stone this point would show where its ground wears through. */
struct Bare {
	vec3 scree;
	vec3 rock;
};

/* A ground's colour and how much its fine bumps and its stones stand out of it. */
struct Patch {
	vec3 tone;
	vec2 rugged;
};

/* What every ground meeting at a point is shaded with: where it is, how it leans, how far it wears bare, the stone it would show and whether its shore is armoured. */
struct Site {
	vec2 p;
	Grain grain;
	Relief relief;
	Exposure exposure;
	Bare bare;
	bool armoured;
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

Grain GrainAt(vec2 p, Detail detail)
{
	float local = (2.0 * FacingOctave(detail, LOCAL_PER_TILE, 0.0) + FacingOctave(detail, LOCAL_PER_TILE * 2.07, 17.3)) / 3.0;
	return Grain(Layered(p, 0.15), local, FacingOctave(detail, CLUMPS_PER_TILE, 2.2), detail.fine, detail.micro, detail.stones, detail.hollow);
}

Ground GroundAt(ivec2 tile)
{
	uvec4 codes = CodesAt(tile);
	return Ground(codes.r, float(codes.g & DENSITY_MASK) / float(DENSITY_MASK), (codes.g & LUSH_BIT) != 0u, codes.a);
}

/* The ground's colours are picked on screen; they are lit as the light they reflect. */
vec3 Lit(vec3 albedo, vec3 normal, float occlusion)
{
	return Radiance(Linear(albedo), normal, RenderPoint(v_world), TERRAIN_ROUGHNESS, occlusion);
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

vec3 GrassTone(float shade, bool lush)
{
	vec3 dark = vec3(0.33, 0.43, 0.21);
	vec3 light = vec3(0.50, 0.56, 0.30);
	if (lush) {
		dark = vec3(0.20, 0.35, 0.17);
		light = vec3(0.31, 0.47, 0.23);
	} else if (Landscape() == LANDSCAPE_ARCTIC) {
		dark = vec3(0.38, 0.47, 0.34);
		light = vec3(0.53, 0.60, 0.44);
	} else if (Landscape() == LANDSCAPE_TROPIC) {
		dark = vec3(0.52, 0.55, 0.28);
		light = vec3(0.68, 0.66, 0.36);
	} else if (Landscape() == LANDSCAPE_TOYLAND) {
		dark = vec3(0.42, 0.72, 0.38);
		light = vec3(0.58, 0.85, 0.48);
	}
	return mix(dark, light, shade);
}

float Hollowed(Grain grain, float depth)
{
	return clamp(1.0 - depth * grain.hollow, 0.55, 1.15);
}

float Cover(float density, float clump)
{
	return density >= 1.0 ? 1.0 : smoothstep(-0.06, 0.06, density + clump - 1.0);
}

/* Grass turns to straw here and there, and its blades fleck it light and dark up close. */
vec3 Grass(Ground ground, Grain grain)
{
	vec3 green = GrassTone(grain.broad, ground.lush);
	vec3 straw = green * STRAW_TINT;
	float shade = Varied(grain.local, 0.16) * Varied(grain.clump, 0.4) * Varied(grain.fine, 0.35) * Varied(grain.micro, 0.7);
	vec3 blades = mix(green, straw, smoothstep(0.5, 0.85, grain.local) + 0.25 * smoothstep(0.6, 0.85, grain.micro)) * shade;
	vec3 soil = SOIL * Varied(grain.local, 0.2) * Varied(grain.micro, 0.25) * mix(1.0, 1.25, grain.stones);
	return mix(soil, blades, Cover(ground.density, grain.local));
}

vec3 Meadow(Grain grain, bool lush)
{
	return Grass(Ground(MAT_GRASS, 1.0, lush, 0u), grain);
}

/* Loose stones shed from rock: gravel speckled light and dark, with pebbles strewn over it and dark gaps between them. */
vec3 Scree(Grain grain)
{
	vec3 tone = SCREE * Varied(grain.local, 0.18) * Varied(grain.fine, 0.14) * Varied(grain.micro, 0.45);
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

/* Bare rock in layers stepping out in ledges where it stands steep, weathered and veined with cracks where it lies flat. */
vec3 Rock(vec2 p, Relief relief, Grain grain)
{
	float bedding = BeddingAt(p);
	int layer = int(floor(bedding));
	float strata = relief.steep * relief.strata;
	vec3 bed = mix(ROCK, ROCK_WARM, Hash(ivec2(layer, 7))) * Varied(Hash(ivec2(layer, 19)), LAYER_VARIETY);
	float ledge = mix(LEDGE_SHADE, 1.08, smoothstep(0.0, 0.3, fract(bedding)));
	vec3 slabs = mix(ROCK, ROCK_WARM, 0.3) * Varied(Octave(p + 2.0, SLABS_PER_TILE), SLAB_VARIETY);
	vec3 rock = mix(mix(slabs, bed, 0.35), bed * ledge, strata);
	vec3 weathered = mix(rock, rock * vec3(0.72, 0.74, 0.66), smoothstep(0.55, 0.8, Octave(p + 4.4, 2.7)));
	return weathered * Varied(grain.fine, 0.22) * Varied(grain.micro, 0.3) * (1.0 - CRACK_DEPTH * (1.0 - relief.steep) * Cracks(p));
}

Bare BareAt(vec2 p, Relief relief, Grain grain)
{
	return Bare(Scree(grain), Rock(p, relief, grain));
}

/* Ground lies bare where it stands steep or high in the mountains, first as loose scree and then as rock, with edges ragged at every scale. */
Exposure ExposureAt(Relief relief, Grain grain)
{
	float altitude = clamp(v_world.z / max(Peak(), MIN_RELIEF_SPAN), 0.0, 1.0);
	float ragged = (grain.local - 0.5) * 0.7 + (grain.fine - 0.5) * 0.3 + (grain.micro - 0.5) * 0.15;
	float bare = relief.steep * STEEP_BARING + smoothstep(ALPINE_ALTITUDE, 1.0, altitude) * ALPINE_BARING + ragged;
	float scree = smoothstep(SCREE_EXPOSURE - EXPOSURE_BLEND, SCREE_EXPOSURE + EXPOSURE_BLEND, bare);
	float rock = smoothstep(ROCK_EXPOSURE - EXPOSURE_BLEND, ROCK_EXPOSURE + EXPOSURE_BLEND, bare);
	return Exposure(scree, rock);
}

Patch Exposed(Patch ground, Site site)
{
	vec3 tone = mix(mix(ground.tone, site.bare.scree, site.exposure.scree), site.bare.rock, site.exposure.rock);
	vec2 rugged = mix(mix(ground.rugged, SCREE_RUGGED, site.exposure.scree), ROCK_RUGGED, site.exposure.rock);
	return Patch(tone, rugged);
}

/* Rough land: tussocks of dry and green grass in clumps, opening onto patches of stony soil. */
Patch Rough(Ground ground, Grain grain, vec2 p)
{
	vec3 green = GrassTone(grain.broad, ground.lush) * 0.9;
	vec3 dry = mix(green, ROUGH_TINT, 0.5);
	float clumps = smoothstep(0.35, 0.65, Octave(p, 2.3));
	float tussocks = smoothstep(0.4, 0.75, 0.6 * grain.micro + 0.4 * grain.fine);
	vec3 scrub = mix(green, dry, clumps) * mix(1.08, 0.84, tussocks) * Varied(grain.micro, 0.3);
	float open = smoothstep(0.58, 0.8, 0.65 * Octave(p + 3.3, 1.2) + 0.35 * grain.fine);
	vec3 stony = mix(SOIL, SCREE, 0.65) * mix(0.92, 1.15, clamp(grain.stones * 1.6, 0.0, 1.0)) * Varied(grain.micro, 0.3);
	return Patch(mix(scrub, stony, open * 0.6), ROUGH_RUGGED);
}

/* Rocky ground: grass with stones gathered in patches over it, and pale rock breaking through here and there. */
Patch Rocks(Grain grain, vec2 p, Bare bare)
{
	float patches = smoothstep(0.48, 0.72, 0.7 * Noise(p * STONE_PATCHES_PER_TILE + 6.2) + 0.3 * grain.fine);
	float stones = patches * mix(STONE_GAPS, 1.0, clamp(grain.stones * 1.6, 0.0, 1.0));
	float outcrop = smoothstep(0.64, 0.74, 0.7 * Noise(p * OUTCROPS_PER_TILE + 2.9) + 0.3 * grain.fine);
	vec3 strewn = mix(Meadow(grain, false), bare.scree * PALE_STONE, stones);
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
	float plants = Varied(Noise(vec2(across * FURROWS_PER_TILE, along * MICRO_PER_TILE)), 0.3 * ResolvedAt(MICRO_PER_TILE, 1.0 / row_pitch)) * Varied(grain.micro, 0.25);
	vec3 soil = mix(SOIL, crop, 0.25) * 0.85 * Varied(grain.fine, 0.2);
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
	return Patch(mix(sand, Meadow(grain, false), grassed), mix(SAND_RUGGED, GRASS_RUGGED, grassed));
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

vec3 Snow(Ground ground, Grain grain, Relief relief, Bare bare)
{
	vec3 snow = SNOW * (0.95 + 0.05 * grain.fine);
	float slip = smoothstep(SNOW_SLIP_SLOPE, STEEPEST_SLOPE, relief.slope + (grain.fine - 0.5) * 0.3);
	vec3 under = mix(Meadow(grain, false), bare.rock, relief.steep);
	return mix(under, snow, Cover(ground.density, grain.local) * (1.0 - slip));
}

vec3 Desert(Ground ground, Grain grain, vec2 p)
{
	float ripple = sin(dot(p, vec2(0.8, 0.6)) * 11.0 + Noise(p * 0.7) * 6.0);
	vec3 sand = SAND * (0.94 + 0.1 * grain.broad) * mix(1.0, 0.96 + 0.04 * ripple, Resolved(3.5)) * Varied(grain.micro, 0.1);
	return mix(Meadow(grain, ground.lush), sand, Cover(ground.density, grain.local));
}

Patch Albedo(Ground ground, Site site)
{
	Grain grain = site.grain;
	vec2 p = site.p;
	switch (ground.material) {
		case MAT_GRASS: return Patch(Grass(ground, grain), GRASS_RUGGED);
		case MAT_ROUGH: return Rough(ground, grain, p);
		case MAT_ROCKS: return Rocks(grain, p, site.bare);
		case MAT_FIELDS: return Fields(ground, grain, p);
		case MAT_SNOW: return Patch(Snow(ground, grain, site.relief, site.bare), SAND_RUGGED);
		case MAT_DESERT: return Patch(Desert(ground, grain, p), SAND_RUGGED);
		case MAT_SHORE: return Shore(site);
		case MAT_PAVED: return Patch(PAVING * (0.94 + 0.08 * grain.local) * Varied(grain.fine, 0.06) * Varied(grain.micro, 0.08), PAVED_RUGGED);
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

Patch Eroded(Ground ground, Site site)
{
	Patch patch = Albedo(ground, site);
	return IsNatural(ground.material) ? Exposed(patch, site) : patch;
}

float Builtness(vec2 p)
{
	vec2 q = p - TILE_CENTRE;
	ivec2 cell = ivec2(floor(q));
	vec2 f = fract(q);
	float a = float(IsBuilt(GroundAt(cell).material));
	float b = float(IsBuilt(GroundAt(cell + ivec2(1, 0)).material));
	float c = float(IsBuilt(GroundAt(cell + ivec2(0, 1)).material));
	float d = float(IsBuilt(GroundAt(cell + ivec2(1, 1)).material));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

/* The grounds of the four tiles around blend across their seams; each distinct ground is shaded once, by the share all its corners hold, in a loop the compiler keeps as one copy. */
bool HasCorner(Ground corners[4], uint material)
{
	return corners[0].material == material || corners[1].material == material || corners[2].material == material || corners[3].material == material;
}

/* The stone is only worked out where some ground at the point shows it. */
Site SiteAt(vec2 p, Grain grain, Relief relief, bool armoured, Ground corners[4])
{
	Exposure exposure = ExposureAt(relief, grain);
	bool bared = exposure.scree > 0.0 || armoured || HasCorner(corners, MAT_ROCKS) || HasCorner(corners, MAT_SNOW);
	Bare bare = bared ? BareAt(p, relief, grain) : Bare(vec3(0.0), vec3(0.0));
	return Site(p, grain, relief, exposure, bare, armoured);
}

Patch Surface(vec2 p, Grain grain, Relief relief, bool armoured, out Site site)
{
	float built = min(Builtness(p) * 2.0, 1.0) * smoothstep(BLOCKS_BLUR_PIXELS, BLOCKS_SHOWN_PIXELS, tile_pixels);
	float reach = mix(NATURAL_WARP, BUILT_WARP, built);
	float width = mix(BLEND_WIDTH, max(BUILT_BLEND_WIDTH, BLEND_PIXELS / tile_pixels), built);
	vec2 warp = (vec2(Noise(p * 0.9), Noise(p * 0.9 + 41.7)) - 0.5) * 2.0 * reach;
	vec2 q = p + warp - TILE_CENTRE;
	ivec2 cell = ivec2(floor(q));
	vec2 f = smoothstep(HALF_TILE - width, HALF_TILE + width, fract(q));

	Ground corners[4] = Ground[4](GroundAt(cell), GroundAt(cell + ivec2(1, 0)), GroundAt(cell + ivec2(0, 1)), GroundAt(cell + ivec2(1, 1)));
	float shares[4] = float[4]((1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y), (1.0 - f.x) * f.y, f.x * f.y);
	site = SiteAt(p, grain, relief, armoured, corners);
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

Water WaterAt(vec2 p)
{
	vec4 near = WaterField(p);
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

/* Where the network's meshes stand opaque they hide its bands, which are then not worked out at all. */
Network BandsAt(vec2 p, mat2 pixel)
{
	if (tile_pixels > NETWORK_OPAQUE_PPT * HIDDEN_BAND_MARGIN) return Network(vec4(0.0), vec2(0.0));
	return NetworkAt(p, pixel);
}

vec3 GroundTone(vec2 p, mat2 pixel, Water water, Relief relief, Detail detail, float levels_per_pixel, inout vec3 normal, out float occlusion)
{
	Grain grain = GrainAt(p, detail);
	Canopy forest = Forest(p);
	Network network = BandsAt(p, pixel);
	float grid = clamp((tile_pixels - INFRASTRUCTURE_PPT) / GRID_FADE_PPT, 0.0, 1.0);
	Site site;
	Patch surface = Surface(p, grain, relief, water.sea > 0.0 && Armoured(p), site);
	normal = Roughened(normal, detail, surface.rugged * (1.0 - water.cover));
	vec3 beach = water.sea > SANDED_SEA ? Shore(site).tone : vec3(0.0);
	vec3 land = Altitude(Composite(Composite(surface.tone, Hedgerow(p, grain)), Airfield(p)), v_world.z);
	float drowned = max(clamp(-v_mark / SUBMERGED_MARK, 0.0, 1.0), water.sea * smoothstep(0.0, DROWNED_LEVELS, -v_world.z));
	land = mix(Banks(land, water, beach), Seabed(water, grain), drowned);
	land *= 1.0 - (1.0 - grid) * u_contour * CONTOUR_DEPTH * Contour(v_world.z, levels_per_pixel) * (1.0 - water.cover);

	vec3 colour = Composite(land, network.paint);
	colour *= 1.0 - grid * u_grid * GRID_DEPTH * GridLine(p, pixel) * (1.0 - water.cover);
	colour = Composite(colour, forest.paint);
	occlusion = 1.0 - forest.shade;
	return Overlay(clamp(colour, 0.0, 1.0), network);
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

	vec3 albedo;
	float occlusion = 1.0;
	if (OutsideMap(p)) {
		albedo = Greyed(Seabed(Water(1.0, 1.0, 1.0, vec3(1.0, 0.0, 0.0), 1.0, 0.0), GrainAt(p, detail)));
	} else if (v_mark > WALL_MARK) {
		albedo = Greyed(WallFace(normal));
	} else {
		albedo = GroundTone(p, pixel, water, relief, detail, levels_per_pixel, normal, occlusion);
	}
	frag_colour = vec4(Lit(albedo, normal, occlusion), 1.0);
}
