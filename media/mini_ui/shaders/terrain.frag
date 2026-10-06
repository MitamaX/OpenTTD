uniform int u_landscape;
uniform float u_contour;
uniform float u_grid;

in vec3 v_world;
in vec3 v_normal;
in float v_mark;

out vec4 frag_colour;

const float TAU = 6.2831853;

const float STANDARD_SLOPE = 1.0;
const float BARE_SLOPE = 1.2;
const float STEEPEST_SLOPE = sqrt(2.0);
const float EROSION_RAGGEDNESS = 1.2;
const float BARE_VARIETY = 0.24;

const float LINE_HALF_PIXELS = 0.5;
const float HALF_LEVEL = 0.5;
const float CONTOUR_DEPTH = 0.75;
const float INDEX_CONTOUR_EVERY = 5.0;
const float INDEX_CONTOUR_WEIGHT = 1.6;
const float CROWDED_CONTOUR_PIXELS = 3.0;
const float SPACED_CONTOUR_PIXELS = 9.0;
const float MIN_RELIEF_SPAN = 6.0;
const vec3 LOWLAND = vec3(0.80, 0.95, 0.83);
const vec3 HIGHLAND = vec3(1.26, 1.12, 0.90);
const float TERRAIN_ROUGHNESS = 0.85;
const float WALL_MARK = 0.5;
const float SUBMERGED_MARK = 0.08;

const float BLEND_WIDTH = 0.28;
const float NATURAL_WARP = 0.32;
const float BUILT_WARP = 0.06;
const float SHORE_WARP = 0.22;
const float DEPTH_LOD = 3.0;

const vec3 SOIL = vec3(0.45, 0.37, 0.27);
const vec3 ROUGH_TINT = vec3(0.46, 0.44, 0.29);
const vec3 ROCK = vec3(0.55, 0.54, 0.51);
const vec3 SNOW = vec3(0.93, 0.95, 0.98);
const vec3 SAND = vec3(0.86, 0.77, 0.55);
const vec3 BEACH = vec3(0.80, 0.74, 0.58);
const vec3 PAVING = vec3(0.57, 0.55, 0.51);
const vec3 YARD = vec3(0.49, 0.41, 0.32);
const vec3 BANK = vec3(0.50, 0.50, 0.48);
const vec3 WET_SAND = vec3(0.52, 0.48, 0.38);
const vec3 SILT = vec3(0.38, 0.40, 0.33);
const vec3 MUD = vec3(0.40, 0.36, 0.27);
const vec3 CANAL_FLOOR = vec3(0.45, 0.45, 0.42);

const int CROP_COUNT = 6;
const vec3 CROPS[CROP_COUNT] = vec3[CROP_COUNT](
	vec3(0.52, 0.40, 0.27),
	vec3(0.47, 0.60, 0.27),
	vec3(0.79, 0.68, 0.33),
	vec3(0.85, 0.79, 0.32),
	vec3(0.64, 0.56, 0.30),
	vec3(0.40, 0.53, 0.30)
);
const float FURROWS_PER_TILE = 6.0;

const float FOREST_COVER = 0.9;
const float CANOPY_OCCLUSION = 0.45;
const float CANOPY_VARIETY = 0.2;
const float GRID_DEPTH = 0.6;

const float COURSES_PER_TILE = 6.0;
const float BLOCKS_PER_TILE = 3.0;
const float MORTAR_SHARE = 0.08;
const vec3 MASONRY = vec3(0.56, 0.53, 0.49);

struct Ground {
	uint material;
	float density;
	bool lush;
	uint variant;
};

struct Grain {
	float broad;
	float local;
	float fine;
};

struct Erosion {
	vec3 tone;
	float share;
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

Grain GrainAt(vec2 p)
{
	return Grain(Layered(p, 0.15), Layered(p, 1.3), Octave(p, 7.0));
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
	vec3 dark = vec3(0.36, 0.52, 0.26);
	vec3 light = vec3(0.53, 0.66, 0.33);
	if (lush) {
		dark = vec3(0.19, 0.40, 0.19);
		light = vec3(0.29, 0.53, 0.25);
	} else if (u_landscape == LANDSCAPE_ARCTIC) {
		dark = vec3(0.38, 0.47, 0.34);
		light = vec3(0.53, 0.60, 0.44);
	} else if (u_landscape == LANDSCAPE_TROPIC) {
		dark = vec3(0.52, 0.55, 0.28);
		light = vec3(0.68, 0.66, 0.36);
	} else if (u_landscape == LANDSCAPE_TOYLAND) {
		dark = vec3(0.42, 0.72, 0.38);
		light = vec3(0.58, 0.85, 0.48);
	}
	return mix(dark, light, shade);
}

float Cover(float density, float clump)
{
	return density >= 1.0 ? 1.0 : smoothstep(-0.06, 0.06, density + clump - 1.0);
}

vec3 Grass(Ground ground, Grain grain)
{
	vec3 blades = GrassTone(grain.broad, ground.lush) * (0.9 + 0.2 * grain.local) * (0.94 + 0.12 * grain.fine);
	vec3 soil = SOIL * (0.9 + 0.2 * grain.local);
	return mix(soil, blades, Cover(ground.density, grain.local));
}

vec3 Meadow(Grain grain, bool lush)
{
	return Grass(Ground(MAT_GRASS, 1.0, lush, 0u), grain);
}

vec3 Rough(Ground ground, Grain grain, vec2 p)
{
	vec3 scrub = mix(GrassTone(grain.broad, ground.lush), ROUGH_TINT, 0.45) * (0.88 + 0.24 * grain.local);
	float tussock = smoothstep(0.58, 0.74, Octave(p, 3.7));
	return scrub * (1.0 - 0.28 * tussock);
}

vec3 Rocks(Grain grain, vec2 p)
{
	float stone = 1.0 - smoothstep(0.28, 0.46, Cells(p * 2.6));
	return ROCK * (0.9 + 0.2 * grain.local) * mix(1.0, mix(0.7, 1.1, stone), Resolved(5.2));
}

vec3 Fields(Ground ground, Grain grain, vec2 p)
{
	vec3 crop = CROPS[int(ground.variant % uint(CROP_COUNT))];
	float across = (ground.variant & 1u) == 0u ? p.x : p.y;
	float furrow = 0.5 + 0.5 * sin(across * TAU * FURROWS_PER_TILE);
	return crop * (0.94 + 0.12 * grain.local) * mix(1.0, 0.8 + 0.2 * furrow, Resolved(2.0 * FURROWS_PER_TILE));
}

vec3 Snow(Ground ground, Grain grain)
{
	vec3 snow = SNOW * (0.95 + 0.05 * grain.fine);
	return mix(Meadow(grain, false), snow, Cover(ground.density, grain.local));
}

vec3 Desert(Ground ground, Grain grain, vec2 p)
{
	float ripple = sin(dot(p, vec2(0.8, 0.6)) * 11.0 + Noise(p * 0.7) * 6.0);
	vec3 sand = SAND * (0.94 + 0.1 * grain.broad) * mix(1.0, 0.96 + 0.04 * ripple, Resolved(3.5));
	return mix(Meadow(grain, ground.lush), sand, Cover(ground.density, grain.local));
}

vec3 Albedo(Ground ground, Grain grain, vec2 p)
{
	switch (ground.material) {
		case MAT_GRASS: return Grass(ground, grain);
		case MAT_ROUGH: return Rough(ground, grain, p);
		case MAT_ROCKS: return Rocks(grain, p);
		case MAT_FIELDS: return Fields(ground, grain, p);
		case MAT_SNOW: return Snow(ground, grain);
		case MAT_DESERT: return Desert(ground, grain, p);
		case MAT_SHORE: return BEACH * (0.95 + 0.08 * grain.local);
		case MAT_PAVED: return PAVING * (0.94 + 0.08 * grain.local) * (0.97 + 0.06 * grain.fine);
		case MAT_DIRT: return YARD * (0.9 + 0.15 * grain.local) * (0.95 + 0.1 * grain.fine);
	}
	return VOID_TONE;
}

bool IsBuilt(uint material)
{
	return material == MAT_FIELDS || material == MAT_PAVED || material == MAT_DIRT;
}

bool IsNatural(uint material)
{
	return material == MAT_GRASS || material == MAT_ROUGH || material == MAT_SNOW || material == MAT_DESERT;
}

Erosion ErosionAt(float slope, Grain grain)
{
	float ragged = Varied(grain.local, EROSION_RAGGEDNESS);
	vec3 bare = mix(SOIL, ROCK, smoothstep(BARE_SLOPE, STEEPEST_SLOPE, slope)) * Varied(grain.fine, BARE_VARIETY);
	return Erosion(bare, clamp(smoothstep(STANDARD_SLOPE, BARE_SLOPE, slope) * ragged, 0.0, 1.0));
}

vec3 Eroded(Ground ground, Grain grain, vec2 p, Erosion erosion)
{
	vec3 tone = Albedo(ground, grain, p);
	return IsNatural(ground.material) ? mix(tone, erosion.tone, erosion.share) : tone;
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

vec3 Surface(vec2 p, Grain grain, Erosion erosion)
{
	float reach = mix(NATURAL_WARP, BUILT_WARP, Builtness(p));
	vec2 warp = (vec2(Noise(p * 0.9), Noise(p * 0.9 + 41.7)) - 0.5) * 2.0 * reach;
	vec2 q = p + warp - TILE_CENTRE;
	ivec2 cell = ivec2(floor(q));
	vec2 f = smoothstep(HALF_TILE - BLEND_WIDTH, HALF_TILE + BLEND_WIDTH, fract(q));

	Ground nw = GroundAt(cell);
	Ground ne = GroundAt(cell + ivec2(1, 0));
	Ground sw = GroundAt(cell + ivec2(0, 1));
	Ground se = GroundAt(cell + ivec2(1, 1));
	if (nw == ne && nw == sw && nw == se) return Eroded(nw, grain, p, erosion);

	vec3 north = mix(Eroded(nw, grain, p, erosion), Eroded(ne, grain, p, erosion), f.x);
	vec3 south = mix(Eroded(sw, grain, p, erosion), Eroded(se, grain, p, erosion), f.x);
	return mix(north, south, f.y);
}

Water WaterAt(vec2 p)
{
	vec2 uv = p / MapSize();
	float canal_here = textureLod(u_water, uv, 0.0).b;
	vec2 warp = (vec2(Noise(p * 1.7 + 7.1), Noise(p * 1.7 + 93.4)) - 0.5) * 2.0 * SHORE_WARP * (1.0 - canal_here);
	vec4 near = texture(u_water, (p + warp) / MapSize());
	float wide = textureLod(u_water, uv, DEPTH_LOD).r;
	float edge = max(fwidth(near.r), 1e-3);

	Water water;
	water.level = near.r;
	water.cover = smoothstep(0.5 - edge, 0.5 + edge, near.r);
	water.depth = smoothstep(0.55, 1.0, wide) * smoothstep(0.5, 0.9, near.r);
	water.share = near.gba / max(near.r, 1e-3);
	water.sea = near.g;
	water.canal = near.b;
	return water;
}

vec3 Banks(vec3 land, Water water)
{
	vec3 damp = land * (1.0 - 0.18 * smoothstep(0.28, 0.5, water.level));
	vec3 walled = mix(damp, BANK, smoothstep(0.3, 0.46, water.canal));
	return mix(walled, BEACH, smoothstep(0.26, 0.44, water.sea));
}

/* The ground under water: wet sand along sea shores giving way to silt in the deep, mud under rivers and dressed stone in canals. */
vec3 Seabed(Water water, Grain grain)
{
	float sea = clamp(water.share.x, 0.0, 1.0);
	float canal = clamp(water.share.y, 0.0, 1.0 - sea);
	float river = 1.0 - sea - canal;
	vec3 bed = mix(WET_SAND, SILT, water.depth) * sea + CANAL_FLOOR * canal + MUD * river;
	return bed * (0.92 + 0.16 * grain.local);
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

vec3 GroundTone(vec2 p, mat2 pixel, Water water, float slope, float levels_per_pixel, out float occlusion)
{
	ivec2 tile = ivec2(floor(p));
	GatherNeighbourhood(tile);

	Grain grain = GrainAt(p);
	Canopy forest = Forest(p);
	Network network = NetworkAt(p, pixel);
	float grid = clamp((tile_pixels - INFRASTRUCTURE_PPT) / GRID_FADE_PPT, 0.0, 1.0);

	vec3 land = Altitude(Surface(p, grain, ErosionAt(slope, grain)), v_world.z);
	land = mix(Banks(land, water), Seabed(water, grain), clamp(-v_mark / SUBMERGED_MARK, 0.0, 1.0));
	land *= 1.0 - (1.0 - grid) * u_contour * CONTOUR_DEPTH * Contour(v_world.z, levels_per_pixel) * (1.0 - water.cover);

	vec3 colour = Composite(land, network.paint);
	colour *= 1.0 - grid * u_grid * GRID_DEPTH * GridLine(p, pixel) * (1.0 - water.cover);
	colour = Composite(colour, forest.paint);
	occlusion = 1.0 - forest.shade;
	return Overlay(clamp(colour, 0.0, 1.0), network);
}

/* Everything read through screen derivatives is read before the fragment branches, where neighbouring pixels may part ways. */
void main()
{
	vec2 p = v_world.xy;
	mat2 pixel = mat2(dFdx(p), dFdy(p));
	tile_pixels = 1.0 / max(max(length(pixel[0]), length(pixel[1])), MIN_SPREAD);
	float levels_per_pixel = max(fwidth(v_world.z), MIN_SPREAD);
	Water water = WaterAt(p);
	vec3 normal = normalize(v_normal);

	vec3 albedo;
	float occlusion = 1.0;
	if (OutsideMap(p)) {
		albedo = Greyed(Seabed(Water(1.0, 1.0, 1.0, vec3(1.0, 0.0, 0.0), 1.0, 0.0), GrainAt(p)));
	} else if (v_mark > WALL_MARK) {
		albedo = Greyed(WallFace(normal));
	} else {
		albedo = GroundTone(p, pixel, water, SlopeOf(normal), levels_per_pixel, occlusion);
	}
	frag_colour = vec4(Lit(albedo, normal, occlusion), 1.0);
}
