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
const vec3 HAZE = vec3(0.80, 0.86, 0.92);
const float HAZE_DEPTH = 0.1;

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
const vec3 BEACH = vec3(0.87, 0.81, 0.64);
const vec3 PAVING = vec3(0.57, 0.55, 0.51);
const vec3 YARD = vec3(0.49, 0.41, 0.32);
const vec3 BANK = vec3(0.50, 0.50, 0.48);
const vec3 SEA_SHALLOW = vec3(0.23, 0.53, 0.62);
const vec3 SEA_DEEP = vec3(0.10, 0.26, 0.44);
const vec3 RIVER = vec3(0.22, 0.46, 0.56);
const vec3 CANAL = vec3(0.20, 0.41, 0.52);
const vec3 FOAM = vec3(0.92, 0.96, 0.98);
const vec3 GLINT = vec3(1.0, 0.98, 0.92);

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

const int TREE_KINDS = 6;
/* Crowns are sized in tile widths across the screen so they stay round; trunks stand in tiles of the world so they grow with the height scale. */
const float CROWN_RADIUS[TREE_KINDS] = float[TREE_KINDS](0.25, 0.17, 0.30, 0.28, 0.12, 0.22);
const float CROWN_ASPECT[TREE_KINDS] = float[TREE_KINDS](1.0, 2.1, 0.8, 0.5, 1.7, 1.0);
const float TRUNK_HEIGHT[TREE_KINDS] = float[TREE_KINDS](0.30, 0.12, 0.40, 0.80, 0.0, 0.35);
const vec3 TREE_DARK[TREE_KINDS] = vec3[TREE_KINDS](
	vec3(0.16, 0.29, 0.11),
	vec3(0.09, 0.21, 0.14),
	vec3(0.07, 0.25, 0.09),
	vec3(0.21, 0.35, 0.12),
	vec3(0.25, 0.39, 0.23),
	vec3(0.66, 0.28, 0.42)
);
const vec3 TREE_LIGHT[TREE_KINDS] = vec3[TREE_KINDS](
	vec3(0.37, 0.52, 0.20),
	vec3(0.22, 0.37, 0.24),
	vec3(0.22, 0.47, 0.16),
	vec3(0.47, 0.59, 0.24),
	vec3(0.47, 0.61, 0.38),
	vec3(0.98, 0.72, 0.82)
);
const float AGE_SCALE[4] = float[4](0.35, 0.65, 1.0, 0.85);
const vec2 TREE_SLOTS[4] = vec2[4](vec2(0.27, 0.30), vec2(0.30, 0.73), vec2(0.71, 0.27), vec2(0.73, 0.70));
const float TREE_JITTER = 0.16;
const float CROWN_AMBIENT = 0.5;
const float CROWN_UPTURN = 0.5;
const float CROWN_REACH = 1.34;
const float CORE_SIZE = 0.7;
const float LOBE_SIZE = 0.6;
const float LOBE_SPREAD = 0.4;
const float LOBE_BLEND = 0.18;
const float LEAF_RAGGEDNESS = 0.16;
const float CONIFER_TIERS = 4.0;
const float TIER_SHOULDER = 0.7;
const float NEEDLE_LEAN = 0.5;
const float SKIRT_LEAN = -0.4;
const float SKIRT_SHADOW = 0.35;
const float NEEDLE_FREQUENCY = 9.0;
const float NEEDLE_RAGGEDNESS = 0.12;
const float FROND_PAIRS = 3.0;
const float FROND_DROOP = 0.6;
const float CACTUS_COLUMN = 0.34;
const float CACTUS_ARM = 0.24;
const int CACTUS_ARMS = 2;
const vec2 ARM_HEIGHTS[CACTUS_ARMS] = vec2[CACTUS_ARMS](vec2(-0.15, 0.45), vec2(0.10, 0.60));
const float CACTUS_RIBS = 8.0;
const float TRUNK_WIDTH = 0.035;
const vec3 TRUNK = vec3(0.27, 0.20, 0.14);
const vec3 WITHERED = vec3(0.46, 0.37, 0.22);
const float CONTACT_SOFTNESS = 0.08;
const float TREE_FAR_PPT = 5.0;
const float TREE_NEAR_PPT = 9.0;
const float TRUNK_FAR_PPT = 9.0;
const float TRUNK_NEAR_PPT = 16.0;
const float DISTANT_FOREST_COVER = 0.85;
const int CANOPY_LAYERS = 4;
const int FOREST_BEHIND = 1;
const int FOREST_AHEAD = 3;
const int FOREST_SPAN = FOREST_BEHIND + 1 + FOREST_AHEAD;
/* A tile whose centre lies farther across the screen than this from a pixel holds no tree that reaches over it or shades its ground. */
const float FOREST_STRIP = 1.0;

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

struct Tree {
	vec2 base;
	vec2 foot;
	float radius;
	float trunk;
	float tone;
	int kind;
	uint age;
};

struct Crown {
	float edge;
	vec3 facing;
};

struct Layer {
	vec4 paint;
	float depth;
};

struct Canopy {
	vec4 paint;
	float shade;
};

const Layer NO_LAYER = Layer(vec4(0.0), -FAR_AWAY);

Grain GrainAt(vec2 p)
{
	return Grain(Layered(p, 0.15), Layered(p, 1.3), Octave(p, 7.0));
}

Ground GroundAt(ivec2 tile)
{
	uvec4 codes = CodesAt(tile);
	return Ground(codes.r, float(codes.g & DENSITY_MASK) / float(DENSITY_MASK), (codes.g & LUSH_BIT) != 0u, codes.a);
}

float SunLight(vec3 normal, float ambient)
{
	return ambient + (1.0 - ambient) * min(max(dot(normal, u_sun), 0.0) / u_sun.z, SUNLIT_CEILING);
}

float Lighting(vec2 slope)
{
	vec3 normal = normalize(vec3(-slope * LEVEL_TILES * HEIGHT_SCALE, 1.0));
	return max(mix(1.0, SunLight(normal, AMBIENT), u_relief), 0.0);
}

vec3 Altitude(vec3 colour, float height)
{
	float t = clamp(height / max(u_peak, MIN_RELIEF_SPAN), 0.0, 1.0);
	vec3 tint = t < 0.5 ? mix(LOWLAND, vec3(1.0), t * 2.0) : mix(vec3(1.0), HIGHLAND, t * 2.0 - 1.0);
	return mix(colour * tint, HAZE, HAZE_DEPTH * (1.0 - t));
}

float Contour(Relief relief, mat2 pixel)
{
	float levels_per_pixel = Spread(pixel, relief.slope);
	bool index = mod(floor(relief.height) + 1.0, INDEX_CONTOUR_EVERY) == 0.0;
	float weight = index ? INDEX_CONTOUR_WEIGHT : 1.0;
	float line = PixelLine(abs(fract(relief.height) - HALF_LEVEL), levels_per_pixel, LINE_HALF_PIXELS * weight);
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

Erosion ErosionAt(Relief relief, Grain grain)
{
	float slope = length(relief.tile_slope);
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
	vec2 uv = p / u_map;
	float canal_here = textureLod(u_water, uv, 0.0).b;
	vec2 warp = (vec2(Noise(p * 1.7 + 7.1), Noise(p * 1.7 + 93.4)) - 0.5) * 2.0 * SHORE_WARP * (1.0 - canal_here);
	vec4 near = texture(u_water, (p + warp) / u_map);
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

float Ripple(vec2 p)
{
	vec2 drift = vec2(u_time * 0.11, u_time * 0.07);
	float swell = Noise(p * 1.6 + drift) + Noise(p * 2.7 - drift.yx * 1.3);
	return mix(0.5, swell * 0.5, Resolved(2.7));
}

vec3 WaterTone(Water water, vec2 p)
{
	float sea = clamp(water.share.x, 0.0, 1.0);
	float canal = clamp(water.share.y, 0.0, 1.0 - sea);
	float river = 1.0 - sea - canal;
	vec3 tone = mix(SEA_SHALLOW, SEA_DEEP, water.depth) * sea + CANAL * canal + RIVER * river;

	float ripple = Ripple(p);
	tone *= 0.95 + 0.1 * ripple;
	tone += GLINT * pow(smoothstep(0.66, 0.92, ripple), 3.0) * 0.22 * Resolved(2.7);

	float surf = smoothstep(0.5, 0.54, water.level) * (1.0 - smoothstep(0.54, 0.68, water.level));
	float churn = 0.55 + 0.45 * Noise(p * 4.0 + vec2(u_time * 0.3, -u_time * 0.2));
	return mix(tone, FOAM, surf * churn * sea * 0.65 * Resolved(6.0));
}

int TreeCount(uvec4 codes)
{
	return int(codes.b & FLORA_COUNT_MASK);
}

vec2 TreeSlot(int index, int count)
{
	if (count == 1) return TILE_CENTRE;
	if (count == 2) return TREE_SLOTS[index * 3];
	return TREE_SLOTS[index];
}

Tree TreeAt(ivec2 tile, Corners ground, uint flora, int index, int count)
{
	ivec2 seed = ivec2(tile.x * 4 + index, tile.y);
	vec2 slot = TreeSlot(index, count) + (Hash2(seed) - 0.5) * TREE_JITTER;
	vec2 variety = Hash2(seed + ivec2(7919, 104729));

	Tree tree;
	tree.kind = int(flora >> FLORA_KIND_SHIFT);
	tree.age = (flora >> FLORA_AGE_SHIFT) & FLORA_AGE_MASK;
	float scale = AGE_SCALE[int(tree.age)] * (0.85 + 0.3 * variety.x);
	tree.base = vec2(tile) + slot;
	tree.foot = PointView(tree.base, ReliefOn(ground, slot).height);
	tree.radius = CROWN_RADIUS[tree.kind] * scale;
	tree.trunk = Rise(TRUNK_HEIGHT[tree.kind] * scale);
	tree.tone = variety.y;
	return tree;
}

vec2 CrownCentre(Tree tree)
{
	return vec2(0.0, tree.trunk + tree.radius * CROWN_ASPECT[tree.kind]);
}

float SmoothMin(float a, float b, float blend)
{
	float h = max(blend - abs(a - b), 0.0) / blend;
	return min(a, b) - h * h * blend * 0.25;
}

vec3 DomeNormal(vec2 v, float roundness)
{
	return normalize(vec3(v * roundness, sqrt(max(1.0 - dot(v, v), 0.0)) + 0.3));
}

Crown LeafyCrown(Tree tree, vec2 v)
{
	vec2 disc = vec2(v.x, v.y / CROWN_ASPECT[tree.kind]);
	vec2 spin = vec2(cos(tree.tone * TAU), sin(tree.tone * TAU)) * LOBE_SPREAD;
	vec2 lobes[4] = vec2[4](spin, vec2(-spin.y, spin.x), -spin, vec2(spin.y, -spin.x));
	float edge = length(disc) - CORE_SIZE;
	vec2 local = disc / CORE_SIZE;
	for (int lobe = 0; lobe < 4; lobe++) {
		vec2 apart = disc - lobes[lobe];
		float lobe_edge = length(apart) - LOBE_SIZE;
		if (lobe_edge < edge) local = apart / LOBE_SIZE;
		edge = SmoothMin(edge, lobe_edge, LOBE_BLEND);
	}
	edge += (Noise(disc * 3.0 + tree.tone * 17.0) - 0.5) * LEAF_RAGGEDNESS;
	return Crown(edge, normalize(DomeNormal(disc, 0.7) + DomeNormal(local, 1.0)));
}

/* Each tier widens from a shoulder tucked under the skirt above down to its own skirt, and the needles just under a skirt face the ground. */
Crown ConiferCrown(Tree tree, vec2 v)
{
	float height = CROWN_ASPECT[tree.kind];
	float down = clamp((height - v.y) / (2.0 * height), 0.0, 1.0);
	float tier = fract(down * CONIFER_TIERS);
	float ragged = (Noise(v * NEEDLE_FREQUENCY + tree.tone * 17.0) - 0.5) * NEEDLE_RAGGEDNESS;
	float width = down * (mix(TIER_SHOULDER, 1.0, tier) + ragged);
	float across = clamp(v.x / max(width, MIN_SPREAD), -1.0, 1.0);
	float lean = mix(SKIRT_LEAN, NEEDLE_LEAN, smoothstep(0.0, SKIRT_SHADOW, tier));
	return Crown(max(abs(v.x) - width, abs(v.y) - height), normalize(vec3(across, lean, sqrt(1.0 - across * across))));
}

Crown PalmCrown(Tree tree, vec2 v)
{
	vec2 fan = vec2(v.x, (v.y + FROND_DROOP * v.x * v.x) / CROWN_ASPECT[tree.kind]);
	float reach = length(fan);
	float angle = atan(fan.y, fan.x) + tree.tone * TAU;
	float frond = abs(sin(angle * FROND_PAIRS)) * reach / FROND_PAIRS;
	float edge = max(reach - 1.0, frond - (0.22 * (1.0 - clamp(reach, 0.0, 1.0)) + 0.03));
	return Crown(edge, normalize(vec3(fan * 1.2, 0.6)));
}

void AddLimb(inout Crown crown, vec2 v, vec2 from, vec2 to, float half_width)
{
	vec2 offset = SegmentOffset(v, from, to) / half_width;
	float edge = (length(offset) - 1.0) * half_width;
	if (edge >= crown.edge) return;
	crown.edge = edge;
	crown.facing = vec3(offset, sqrt(max(1.0 - dot(offset, offset), 0.0)));
}

Crown CactusCrown(Tree tree, vec2 v)
{
	float height = CROWN_ASPECT[tree.kind];
	float side = tree.tone < 0.5 ? -1.0 : 1.0;
	Crown crown = Crown(FAR_AWAY, vec3(0.0, 0.0, 1.0));
	AddLimb(crown, v, vec2(0.0, -height), vec2(0.0, height - CACTUS_COLUMN), CACTUS_COLUMN);
	for (int arm = 0; arm < CACTUS_ARMS; arm++) {
		vec2 elbow = vec2(side * (1.0 - CACTUS_ARM), ARM_HEIGHTS[arm].x * height);
		AddLimb(crown, v, vec2(0.0, elbow.y), elbow, CACTUS_ARM);
		AddLimb(crown, v, elbow, vec2(elbow.x, ARM_HEIGHTS[arm].y * height), CACTUS_ARM);
		side = -side;
	}
	crown.edge = max(crown.edge, -height - v.y);
	return crown;
}

Crown CrownAt(Tree tree, vec2 v)
{
	if (tree.kind == int(TREE_CONIFER)) return ConiferCrown(tree, v);
	if (tree.kind == int(TREE_PALM)) return PalmCrown(tree, v);
	if (tree.kind == int(TREE_CACTUS)) return CactusCrown(tree, v);
	return LeafyCrown(tree, v);
}

/* The way toward the viewer in the world the sun lights, whose heights stand raised by the height scale. */
vec3 Sight()
{
	return normalize(vec3(u_toward * (VIEW_RISE / (LEVEL_TILES * HEIGHT_SCALE)), VIEW_DEPTH));
}

/* A normal given across the screen, up it and out toward the viewer, turned into the world the sun lights. */
vec3 Facing(vec3 facing)
{
	vec3 sight = Sight();
	vec3 across = vec3(u_right, 0.0);
	return across * facing.x + cross(sight, across) * facing.y + sight * facing.z;
}

vec3 CrownTone(Tree tree, Crown crown, vec2 v, float snow)
{
	vec3 normal = normalize(Facing(crown.facing) + vec3(0.0, 0.0, CROWN_UPTURN));
	float lit = SunLight(normal, CROWN_AMBIENT);
	float cluster = mix(0.5, Noise(v * tree.radius * 14.0 + tree.tone * 13.0), Resolved(14.0));
	if (tree.kind == int(TREE_CACTUS)) cluster = mix(cluster, 0.5 + 0.5 * cos(asin(clamp(crown.facing.x, -1.0, 1.0)) * CACTUS_RIBS), 0.5);

	vec3 leaf = mix(TREE_DARK[tree.kind], TREE_LIGHT[tree.kind], clamp(0.2 + 0.45 * cluster + 0.35 * tree.tone, 0.0, 1.0));
	if (tree.age == AGE_DYING) leaf = mix(leaf, WITHERED, 0.6);
	vec3 shaded = leaf * lit * (0.82 + 0.18 * smoothstep(-0.12, 0.0, -crown.edge));
	return mix(shaded, SNOW * lit, snow * smoothstep(0.35, 0.75, cluster + normal.z * 0.3));
}

float ContactShadow(Tree tree, vec2 p)
{
	float gap = length(p - tree.base) - tree.radius;
	return (1.0 - smoothstep(-CONTACT_SOFTNESS, CONTACT_SOFTNESS, gap)) * TREE_SHADOW_DEPTH;
}

float TrunkCover(Tree tree, vec2 upright, float pixel)
{
	float width = TRUNK_WIDTH * tree.radius / CROWN_RADIUS[int(TREE_BROADLEAF)];
	return Stroke(SegmentDistance(upright, vec2(0.0), CrownCentre(tree)), width, pixel);
}

/* A tree stands on the screen as drawn from its foot upward, whatever slope the pixels around it show. */
vec4 TreePaint(Tree tree, vec2 view, float snow)
{
	vec2 upright = vec2(view.x - tree.foot.x, tree.foot.y - view.y);
	float pixel = 1.0 / u_ppt;
	float trunks = ZoomFade(TRUNK_FAR_PPT, TRUNK_NEAR_PPT);
	vec4 paint = vec4(0.0);
	if (trunks > 0.0) paint = Over(paint, TRUNK, TrunkCover(tree, upright, pixel) * trunks);

	vec2 v = (upright - CrownCentre(tree)) / tree.radius;
	if (any(greaterThan(abs(v), vec2(1.0, max(CROWN_ASPECT[tree.kind], 1.0)) * CROWN_REACH))) return paint;
	Crown crown = CrownAt(tree, v);
	float alpha = 1.0 - smoothstep(-pixel, pixel, crown.edge * tree.radius);
	if (alpha <= 0.0) return paint;
	return Over(paint, CrownTone(tree, crown, v, snow), alpha);
}

/* Crowns are opaque but for their rims, so only the nearest few over a pixel can show. */
void Keep(inout Layer layers[CANOPY_LAYERS], Layer layer)
{
	if (layer.depth <= layers[0].depth) return;
	int slot = 0;
	while (slot + 1 < CANOPY_LAYERS && layers[slot + 1].depth < layer.depth) {
		layers[slot] = layers[slot + 1];
		slot++;
	}
	layers[slot] = layer;
}

vec4 FarToNear(Layer layers[CANOPY_LAYERS])
{
	vec4 paint = vec4(0.0);
	for (int slot = 0; slot < CANOPY_LAYERS; slot++) paint = Over(paint, layers[slot].paint);
	return paint;
}

/* Trees stand square to the way toward the viewer, so the ground a pixel shows hides every tree standing behind it. */
bool Behind(Tree tree, vec2 p)
{
	return dot(tree.base - p, u_toward) < 0.0;
}

/* A tree shows up the screen from its base, so the trees over a pixel stand on its tile or on the tiles toward the viewer. */
ivec2 ForestOrigin(vec2 p)
{
	vec2 behind = mix(vec2(FOREST_BEHIND), vec2(FOREST_AHEAD), lessThan(u_toward, vec2(0.0)));
	return ivec2(floor(p)) - ivec2(behind);
}

bool ForestReaches(ivec2 tile, vec2 p)
{
	return OnMap(tile) && abs(dot(vec2(tile) + TILE_CENTRE - p, u_right)) <= FOREST_STRIP;
}

void AddTrees(inout Layer layers[CANOPY_LAYERS], inout float shade, ivec2 tile, vec2 p, vec2 view)
{
	uvec4 codes = texelFetch(u_tiles, tile, 0);
	int count = TreeCount(codes);
	if (count == 0) return;

	Corners ground = SurfaceOf(tile);
	float snow = codes.r == MAT_SNOW ? float(codes.g & DENSITY_MASK) / float(DENSITY_MASK) : 0.0;
	for (int index = 0; index < count; index++) {
		Tree tree = TreeAt(tile, ground, codes.b, index, count);
		shade = max(shade, ContactShadow(tree, p));
		if (Behind(tree, p)) continue;
		vec4 paint = TreePaint(tree, view, snow);
		if (paint.a > 0.0) Keep(layers, Layer(paint, dot(tree.base, u_toward)));
	}
}

Canopy NearForest(vec2 p, vec2 view)
{
	Layer layers[CANOPY_LAYERS];
	for (int slot = 0; slot < CANOPY_LAYERS; slot++) layers[slot] = NO_LAYER;
	float shade = 0.0;
	ivec2 origin = ForestOrigin(p);
	for (int j = 0; j < FOREST_SPAN; j++) {
		for (int i = 0; i < FOREST_SPAN; i++) {
			ivec2 tile = origin + ivec2(i, j);
			if (ForestReaches(tile, p)) AddTrees(layers, shade, tile, p, view);
		}
	}
	return Canopy(FarToNear(layers), shade);
}

float TileCover(ivec2 tile)
{
	return float(TreeCount(CodesAt(tile))) / 4.0;
}

Canopy DistantForest(vec2 p)
{
	vec2 q = p - TILE_CENTRE;
	ivec2 cell = ivec2(floor(q));
	vec2 f = smoothstep(0.0, 1.0, fract(q));
	float density = mix(mix(TileCover(cell), TileCover(cell + ivec2(1, 0)), f.x), mix(TileCover(cell + ivec2(0, 1)), TileCover(cell + ivec2(1, 1)), f.x), f.y);

	uvec4 home = CodesAt(ivec2(floor(p)));
	int kind = TreeCount(home) > 0 ? int(home.b >> FLORA_KIND_SHIFT) : int(TREE_BROADLEAF);
	vec3 tone = mix(TREE_DARK[kind], TREE_LIGHT[kind], 0.3 + 0.35 * Octave(p, 1.1));
	float alpha = density * DISTANT_FOREST_COVER;
	return Canopy(vec4(tone, 1.0) * alpha, density * TREE_SHADOW_DEPTH * 0.5);
}

Canopy Forest(vec2 p, vec2 view)
{
	Canopy far = DistantForest(p);
	float near = ZoomFade(TREE_FAR_PPT, TREE_NEAR_PPT);
	if (near <= 0.0) return far;
	Canopy close = NearForest(p, view);
	return Canopy(mix(far.paint, close.paint, near), mix(far.shade, close.shade, near));
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
	vec3 grey = vec3(GREY_FLOOR + dot(colour, LUMA_WEIGHTS) * (1.0 - u_sink));
	bool rail = u_layer == LAYER_RAIL;
	return mix(grey, rail ? RAIL_ACCENT : ROAD_ACCENT, rail ? network.layers.x : network.layers.y);
}

bool IsVoid(vec2 p)
{
	ivec2 tile = ivec2(floor(p));
	return !OnMap(tile) || CodesAt(tile).r == MAT_VOID;
}

void main()
{
	vec2 view = PixelView(v_screen);
	vec2 p = GroundUnder(view);
	GatherNeighbourhood(ivec2(floor(p)));

	Relief relief = ReliefAt(p);
	mat2 pixel = PixelFootprint(relief.slope);
	Grain grain = GrainAt(p);
	Water water = WaterAt(p);
	Canopy forest = Forest(p, view);
	Network network = NetworkAt(p, pixel);
	float light = Lighting(relief.slope) * (1.0 - forest.shade * u_relief);

	vec3 land = Altitude(Surface(p, grain, ErosionAt(relief, grain)), relief.height);
	land = Banks(land, water);
	land *= light * (1.0 - u_contour * CONTOUR_DEPTH * Contour(relief, pixel));

	vec3 colour = land;
	if (water.cover > 0.0) colour = mix(land, WaterTone(water, p), water.cover);
	colour = Composite(colour, vec4(network.paint.rgb * light, network.paint.a));
	colour *= 1.0 - u_grid * GridLine(p, pixel);
	colour = Composite(colour, forest.paint);
	colour = Overlay(clamp(colour, 0.0, 1.0), network);
	if (IsVoid(p)) colour = VOID_TONE;

	frag_colour = vec4(colour, 1.0);
}
