const float TAU = 6.2831853;
const float TEXELS = float(TEXELS_PER_TILE);
const float PLINTH_HEIGHT = 0.016;
const float UPRIGHT_WALL = 0.7;
const float COURSE_TILT = 0.2;
const vec2 COURSE_SHADES = vec2(0.72, 1.06);
const vec2 COURSE_LIGHT_EDGES = vec2(0.0, 0.4);
const float COURSE_OVERLAP_SHADE = 0.12;
const vec2 COURSE_OVERLAP_EDGES = vec2(0.88, 1.0);
const float TILE_GAP_WIDTH = 0.06;
const float TILE_GAP_SHADE = 0.35;
const float FIRING_BLOTCHES = 4.5;
const vec2 BLOTCH_EDGES = vec2(0.2, 0.8);
const float BLOTCH_MEAN = 0.5;
const float BLOTCH_VARIETY = 1.5;
const float BLOTCH_SEED = 3.7;
const vec2 RAIN_STREAKS = vec2(13.0, 1.4);
const vec2 STREAK_EDGES = vec2(-0.25, 1.25);
const float STREAK_MEAN = 0.5;
const float STREAK_DEPTH = 0.16;
const float STREAK_SEED = 9.2;
const float LICHEN_PATCHES = 2.7;
const vec2 LICHEN_PATCH_EDGES = vec2(0.62, 0.86);
const float LICHEN_PATCH_MEAN = 0.0;
const float LICHEN_PATCH_SEED = 1.3;
const float LICHEN_SPOTS = 40.0;
const vec2 LICHEN_SPOT_EDGES = vec2(0.565, 0.765);
const float LICHEN_SPOT_MEAN = 0.25;
const float LICHEN_SPOT_SEED = 6.1;
const float LICHEN_COVER = 0.45;
const vec3 LICHEN = vec3(0.62, 0.61, 0.49);
const float WALL_BLOTCHES = 2.2;
const float WALL_VARIETY = 0.18;
const float WALL_STREAK_DEPTH = 0.3;
const float GRIME_HEIGHT = 0.18;
const float GRIME_SHADE = 0.75;
const float GRIME_RAGS = 9.0;
const float GRIME_RAGGED = 0.08;
const float GRIME_SEED = 2.9;
const float FLAT_ROOF_VARIETY = 0.12;
const float CLAY_TILE_VARIETY = 0.14;
const float SLATE_VARIETY = 0.16;
const float SHINGLE_VARIETY = 0.22;
const float POND_STAINS = 2.2;
const vec2 POND_STAIN_EDGES = vec2(0.55, 0.68);
const float POND_STAIN_MEAN = 0.2;
const float POND_STAIN_SEED = 7.9;
const vec3 POND_STAIN = vec3(0.72, 0.7, 0.66);
const vec3 DIM_ROOM = vec3(0.3, 0.29, 0.27);
const vec3 CANVAS_STRIPE = vec3(0.92, 0.9, 0.84);
const float CANVAS_STRIPES = 40.0;
const float CANVAS_ROUGHNESS = 0.9;
const vec3 LETTERING = vec3(0.95, 0.93, 0.86);
const float LETTER_PITCH = 1.0;
const vec2 LETTER_SIZE = vec2(0.7, 1.2);
const vec2 LETTER_STROKES = vec2(2.0, 3.0);
const float LETTER_SPACES = 0.2;
const float LETTER_INK = 0.6;
const ivec2 LETTER_SALT = ivec2(41, 23);
const vec2 LETTERS_RESOLVED_PIXELS = vec2(0.15, 0.4);
const float LETTERED_SHARE = LETTER_SIZE.x / LETTER_PITCH * (1.0 - LETTER_SPACES) * LETTER_INK;
const float SIGN_LINE_PITCH = 2.0;
const float SIGN_LETTER_SCALE = 2.0;
const float SIGN_GLOW = 0.8;
const float SIGN_ROUGHNESS = 0.3;
const float RIPPLES = 36.0;
const float RIPPLE_TILT = 0.2;
const vec2 RIPPLE_SEED = vec2(4.7, 9.3);
const float RING_WAVES = 24.0;
const float RING_TILT = 0.35;
const float RING_REACH = 0.3;
const float WATER_ROUGHNESS = 0.06;
const float GRAVEL_GRAIN = 110.0;
const float GRAVEL_VARIETY = 0.22;
const float GRAVEL_ROUGHNESS = 0.95;
const vec2 PEBBLES_PER_TILE = vec2(16.0, 42.0);
const vec2 PEBBLE_SEEDS = vec2(1.7, 6.4);
const vec2 PEBBLE_EDGES = vec2(0.0, 1.0);
const float PEBBLE_MEAN = 0.5;
const float PEBBLE_VARIETY = 0.2;
const float SCOURS_PER_TILE = 1.6;
const vec2 SCOUR_EDGES = vec2(0.66, 0.8);
const float SCOUR_MEAN = 0.15;
const float SCOUR_SEED = 8.3;
const float SCOUR_SHADE = 0.72;
const vec2 MEMBRANE_ROLLS = vec2(0.6, 6.0);
const float MEMBRANE_LAP = 0.08;
const float MEMBRANE_VARIETY = 0.09;
const float MEMBRANE_LAP_LIFT = 1.08;
const float MEMBRANE_ROUGHNESS = 0.8;
const float DRAINS_PER_TILE = 1.2;
const float DRAIN_SHARE = 0.35;
const float DRAIN_RADIUS = 0.06;
const float DRAIN_SHADE = 0.6;
const SpeckLattice DRAIN_LATTICE = SpeckLattice(ivec2(1301, 727), 0.25, 0.7, 0.5);

/* A point of a building before light, its facing told across, up and out of its face. */
struct Clad {
	vec3 albedo;
	vec3 facing;
	float roughness;
	float glass;
	float occlusion;
	float sunlit;
	float daylight;
	float sunbeam;
	vec3 lamp;
	float glow;
};

const vec3 FACING_OUT = vec3(0.0, 0.0, 1.0);

Clad Matte(vec3 colour, float roughness)
{
	return Clad(colour, FACING_OUT, roughness, 0.0, 1.0, 1.0, 1.0, 0.0, vec3(0.0), 0.0);
}

Clad Mixed(Clad a, Clad b, float share)
{
	return Clad(mix(a.albedo, b.albedo, share), mix(a.facing, b.facing, share), mix(a.roughness, b.roughness, share), mix(a.glass, b.glass, share),
		mix(a.occlusion, b.occlusion, share), mix(a.sunlit, b.sunlit, share), mix(a.daylight, b.daylight, share), mix(a.sunbeam, b.sunbeam, share), mix(a.lamp, b.lamp, share),
		mix(a.glow, b.glow, share));
}

/* The variance of the slopes a surface leans through when tilted along a sine wave of this amplitude, and evenly across this span. */
float WaveSpread(float amplitude)
{
	return amplitude * amplitude * 0.5;
}

float RampSpread(float span)
{
	return span * span / 12.0;
}

/* Tilts too fine to show widen the highlight instead, the variance of their slopes adding to the one the roughness stands for. */
float Roughened(float roughness, float unseen_slope_variance)
{
	float alpha = roughness * roughness;
	return sqrt(sqrt(alpha * alpha + unseen_slope_variance));
}

float UnitVaried(vec2 unit, float variety, float sharpness)
{
	return Varied(Hash(ivec2(floor(unit))), variety * sharpness);
}

/* Units laid in courses with every other course shifted half a unit, the joints between them filled. */
Clad Coursed(vec2 p, vec3 tint, vec2 units, float joint, float variety, vec3 filling)
{
	float course = p.y * units.y;
	float column = p.x * units.x + 0.5 * floor(course);
	float joints = max(Lines(course, joint), Lines(column, joint * units.x / units.y));
	vec3 unit = tint * UnitVaried(vec2(column, course), variety, Sharpness(course));
	return Matte(mix(unit, filling, joints), 0.85);
}

float FadedBy(vec2 q, float seed, vec2 edges, float mean, float sharp)
{
	return sharp > 0.0 ? mix(mean, smoothstep(edges.x, edges.y, Noise(q + seed)), sharp) : mean;
}

float Faded(vec2 q, float seed, vec2 edges, float mean)
{
	return FadedBy(q, seed, edges, mean, Sharpness(q));
}

vec3 Stained(vec2 p, vec3 tint, float blotches_per_unit, float variety, float streak_depth)
{
	float blotches = Faded(p * blotches_per_unit, BLOTCH_SEED, BLOTCH_EDGES, BLOTCH_MEAN);
	float streaks = Faded(p * RAIN_STREAKS, STREAK_SEED, STREAK_EDGES, STREAK_MEAN);
	return tint * Varied(blotches, variety * BLOTCH_VARIETY) * Varied(streaks, streak_depth);
}

vec3 Weathered(vec2 p, vec3 tint, float variety)
{
	float patched = Faded(p * LICHEN_PATCHES, LICHEN_PATCH_SEED, LICHEN_PATCH_EDGES, LICHEN_PATCH_MEAN);
	float spot_sharpness = Sharpness(p * LICHEN_SPOTS);
	float spotted = patched > 0.0 ? FadedBy(p * LICHEN_SPOTS, LICHEN_SPOT_SEED, LICHEN_SPOT_EDGES, LICHEN_SPOT_MEAN, spot_sharpness) : 0.0;
	return mix(Stained(p, tint, FIRING_BLOTCHES, variety, STREAK_DEPTH), LICHEN, patched * spotted * LICHEN_COVER);
}

vec3 PondStains(vec2 p)
{
	float ponds = Faded(p * POND_STAINS, POND_STAIN_SEED, POND_STAIN_EDGES, POND_STAIN_MEAN);
	return mix(vec3(1.0), POND_STAIN, ponds);
}

vec3 Grimed(vec2 p, vec3 tint)
{
	if (!Weathers()) return tint;
	float ragged = (Faded(vec2(p.x * GRIME_RAGS, 0.0), GRIME_SEED, vec2(0.0, 1.0), 0.5) - 0.5) * GRIME_RAGGED;
	float splashed = 1.0 - smoothstep(0.0, GRIME_HEIGHT, p.y + ragged);
	return Stained(p, tint, WALL_BLOTCHES, WALL_VARIETY, WALL_STREAK_DEPTH) * mix(1.0, GRIME_SHADE, splashed);
}

/* Overlapping courses, each shaded under the one above it and lit along its own lower edge. */
Clad Tiled(vec2 p, vec3 tint, vec2 units, float variety, float roughness, float roll)
{
	float course = p.y * units.y;
	float column = p.x * units.x + 0.5 * floor(course);
	float sharp = Sharpness(course);
	float rolled = Sharpness(column);
	float along = fract(course);
	float lit = mix(COURSE_SHADES.x, COURSE_SHADES.y, smoothstep(COURSE_LIGHT_EDGES.x, COURSE_LIGHT_EDGES.y, along));
	float overlapped = 1.0 - COURSE_OVERLAP_SHADE * smoothstep(COURSE_OVERLAP_EDGES.x, COURSE_OVERLAP_EDGES.y, along);
	float shade = mix(1.0, lit * overlapped, sharp);
	vec3 colour = tint * shade * UnitVaried(vec2(column, course), variety, sharp);
	float gaps = Lines(column, TILE_GAP_WIDTH, rolled) * sharp;
	float unseen_tilt = WaveSpread(roll) * (1.0 - rolled) + RampSpread(COURSE_TILT) * (1.0 - sharp);
	Clad clad = Matte(colour * (1.0 - TILE_GAP_SHADE * gaps), Roughened(roughness, unseen_tilt));
	clad.facing = vec3(roll * sin(TAU * column) * rolled, -COURSE_TILT * sharp * (along - 0.5), 1.0);
	return clad;
}

Clad Mottled(vec2 p, vec3 tint, float frequency, float variety, float roughness)
{
	float noise = mix(0.5, Noise(p * frequency), Sharpness(p.x * frequency));
	return Matte(tint * Varied(noise, variety), roughness);
}

Clad Gravelled(vec2 p, vec3 tint)
{
	float coarse = Faded(p * PEBBLES_PER_TILE.x, PEBBLE_SEEDS.x, PEBBLE_EDGES, PEBBLE_MEAN);
	float fine = Faded(p * PEBBLES_PER_TILE.y, PEBBLE_SEEDS.y, PEBBLE_EDGES, PEBBLE_MEAN);
	float scoured = Faded(p * SCOURS_PER_TILE, SCOUR_SEED, SCOUR_EDGES, SCOUR_MEAN);
	vec3 stones = tint * Varied(coarse, PEBBLE_VARIETY) * Varied(fine, PEBBLE_VARIETY);
	return Mottled(p, mix(stones, tint * SCOUR_SHADE, scoured), GRAVEL_GRAIN, GRAVEL_VARIETY, GRAVEL_ROUGHNESS);
}

Clad Membrane(vec2 p, vec3 tint)
{
	Clad clad = Coursed(p, tint, MEMBRANE_ROLLS, MEMBRANE_LAP, MEMBRANE_VARIETY, tint * MEMBRANE_LAP_LIFT);
	vec2 q = p * DRAINS_PER_TILE;
	float drain = Speck(q, DRAIN_SHARE, vec2(1.0, 0.0), vec2(DRAIN_RADIUS), DRAIN_LATTICE) * Sharpness(q / (2.0 * DRAIN_RADIUS));
	clad.albedo *= 1.0 - DRAIN_SHADE * drain;
	clad.roughness = MEMBRANE_ROUGHNESS;
	return clad;
}

Clad Panelled(vec2 p, vec3 tint, vec2 panels, float joint, float roughness)
{
	float joints = max(Lines(p.x * panels.x, joint), Lines(p.y * panels.y, joint));
	Clad clad = Mottled(p, tint, 9.0, 0.08, roughness);
	clad.albedo *= 1.0 - 0.3 * joints;
	return clad;
}

Clad Ribbed(vec2 p, vec3 tint, float frequency, float depth, float roughness)
{
	float phase = p.x * frequency;
	float sharp = Sharpness(phase);
	Clad clad = Matte(tint * (1.0 + 0.06 * cos(TAU * phase) * sharp), Roughened(roughness, WaveSpread(depth) * (1.0 - sharp)));
	clad.facing = vec3(depth * sin(TAU * phase) * sharp, 0.0, 1.0);
	return clad;
}

Clad Boarded(vec2 p, vec3 tint, float boards, float roughness)
{
	float board = p.y * boards;
	float sharp = Sharpness(board);
	float shade = mix(1.0, mix(0.84, 1.06, fract(board)), sharp);
	Clad clad = Matte(tint * shade * UnitVaried(vec2(0.0, board), 0.1, sharp), roughness);
	clad.facing = vec3(0.0, 0.25 * sharp, 1.0);
	return clad;
}

Clad Planked(vec2 p, vec3 tint, float planks)
{
	float plank = p.x * planks;
	float sharp = Sharpness(plank);
	vec3 colour = tint * UnitVaried(vec2(plank, 0.0), 0.18, sharp) * (1.0 - 0.4 * Lines(plank, 0.08));
	return Matte(colour, 0.9);
}

/* Frames holding panes; the frames take the tint and the panes the glass. */
Clad Glazed(vec2 p, vec3 frame, vec2 panes, float bar)
{
	float bars = max(Lines(p.x * panes.x, bar), Lines(p.y * panes.y, bar));
	Clad clad = Matte(mix(DIM_ROOM, frame, bars), mix(0.08, 0.5, bars));
	clad.glass = 1.0 - bars;
	return clad;
}

/* A line of block letters along x, in texels about its middle at y = 0, as a share of the point they ink; letters too fine to show blend into the share they ink. */
float Lettering(vec2 at, int line, float pixel)
{
	float shown = 1.0 - smoothstep(LETTERS_RESOLVED_PIXELS.x, LETTERS_RESOLVED_PIXELS.y, pixel);
	if (abs(at.y) > LETTER_SIZE.y * 0.5) return 0.0;
	if (shown <= 0.0) return LETTERED_SHARE;
	float column = floor(at.x / LETTER_PITCH);
	vec2 in_letter = vec2(at.x - column * LETTER_PITCH - (LETTER_PITCH - LETTER_SIZE.x) * 0.5, at.y + LETTER_SIZE.y * 0.5);
	ivec2 letter = ivec2(column, line) * LETTER_SALT;
	bool spaced = in_letter.x < 0.0 || in_letter.x > LETTER_SIZE.x || Hash(letter) < LETTER_SPACES;
	ivec2 stroke = ivec2(floor(in_letter / LETTER_SIZE * LETTER_STROKES));
	float inked = !spaced && Hash(letter + stroke + 1) < LETTER_INK ? 1.0 : 0.0;
	return mix(LETTERED_SHARE, inked, shown);
}

/* Awning canvas runs in stripes of its own colour and a pale one. */
Clad Striped(vec2 p, vec3 tint)
{
	return Matte(mix(tint, CANVAS_STRIPE, Lines(p.x * CANVAS_STRIPES, 0.5)), CANVAS_ROUGHNESS);
}

/* A lightbox's face printed with lines of lettering, glowing from within. */
Clad Lightbox(vec2 p, vec3 tint)
{
	vec2 at = p * TEXELS / SIGN_LETTER_SCALE;
	float pixel = max(fwidth(at.x), fwidth(at.y));
	float line = floor(at.y / SIGN_LINE_PITCH);
	Clad face = Matte(mix(tint, LETTERING, Lettering(vec2(at.x, at.y - (line + 0.5) * SIGN_LINE_PITCH), int(line), pixel)), SIGN_ROUGHNESS);
	face.glow = SIGN_GLOW;
	return face;
}

Clad Rippled(vec2 p, vec3 tint)
{
	vec2 q = p * RIPPLES;
	vec2 from_jet = fract(p) - HALF_TILE;
	float reach = length(from_jet);
	float phase = reach * RING_WAVES;
	float rings = Sharpness(phase);
	float ripples = Sharpness(q);
	float swell = RING_TILT * cos(TAU * phase) * (1.0 - smoothstep(0.0, RING_REACH, reach));
	vec2 tilt = from_jet / max(reach, LEAST_DIVISOR) * swell * rings + (vec2(Noise(q), Noise(q + RIPPLE_SEED)) - 0.5) * RIPPLE_TILT * ripples;
	Clad clad = Matte(tint, Roughened(WATER_ROUGHNESS, WaveSpread(RING_TILT) * (1.0 - rings) + RampSpread(RIPPLE_TILT) * (1.0 - ripples)));
	clad.facing = vec3(tilt, 1.0);
	clad.glass = 1.0;
	return clad;
}

Clad Leafy(vec2 p, vec3 tint)
{
	float sharp = Sharpness(p.x * 60.0);
	float clumps = mix(0.5, Noise(p * 60.0), sharp);
	Clad clad = Matte(tint * Varied(clumps, 0.45) * Varied(mix(0.5, Noise(p * 17.0), Sharpness(p.x * 17.0)), 0.25), 0.95);
	clad.facing = vec3((vec2(Noise(p * 60.0 + 3.1), Noise(p * 60.0 + 7.7)) - 0.5) * 0.9 * sharp, 1.0);
	return clad;
}

bool IsMasonry(uint material)
{
	return material == CLAD_BRICK || material == CLAD_STONE || material == CLAD_RENDER || material == CLAD_CONCRETE || material == CLAD_TIMBER;
}

/* Masonry walls stand on a darker plinth. */
float Plinth(uint material, vec2 p)
{
	if (!IsMasonry(material)) return 1.0;
	float pixel = fwidth(p.y);
	return mix(0.72, 1.0, smoothstep(PLINTH_HEIGHT - pixel, PLINTH_HEIGHT + pixel, p.y));
}

bool Grimes(uint material)
{
	return IsMasonry(material) || material == CLAD_PLANKS || material == CLAD_CORRUGATED || material == CLAD_METAL;
}

vec3 Walled(uint material, vec2 p, vec3 albedo, float upright)
{
	vec3 walled = albedo * Plinth(material, p);
	if (Grimes(material)) walled = Grimed(p, walled);
	return mix(albedo, walled, step(UPRIGHT_WALL, upright));
}

bool IsTiled(uint material)
{
	return material == CLAD_CLAY_TILE || material == CLAD_SLATE || material == CLAD_SHINGLE;
}

bool Ponds(uint material)
{
	return material == CLAD_GRAVEL || material == CLAD_MEMBRANE;
}

float RoofVariety(uint material)
{
	switch (material) {
		case CLAD_CLAY_TILE: return CLAY_TILE_VARIETY;
		case CLAD_SLATE: return SLATE_VARIETY;
		case CLAD_SHINGLE: return SHINGLE_VARIETY;
		default: return FLAT_ROOF_VARIETY;
	}
}

vec3 Aged(uint material, vec2 p, vec3 tint)
{
	if (!Weathers() || !(IsTiled(material) || Ponds(material))) return tint;
	vec3 aged = Weathered(p, tint, RoofVariety(material));
	return Ponds(material) ? aged * PondStains(p) : aged;
}

Clad CladOf(uint material, vec2 p, vec3 tint)
{
	vec3 mortar = mix(tint, vec3(0.78, 0.75, 0.70), 0.7);
	vec3 aged = Aged(material, p, tint);
	switch (material) {
		case CLAD_BRICK: return Coursed(p, tint, vec2(40.0, 80.0), 0.18, 0.18, mortar);
		case CLAD_STONE: return Coursed(p, tint, vec2(16.0, 32.0), 0.1, 0.12, mortar);
		case CLAD_FOUNDATION: return Coursed(p, tint * 0.9, vec2(12.0, 24.0), 0.1, 0.25, mortar * 0.8);
		case CLAD_RENDER: return Mottled(p, tint, 14.0, 0.07, 0.9);
		case CLAD_CONCRETE: return Panelled(p, tint, vec2(6.0, 8.0), 0.03, 0.85);
		case CLAD_TIMBER: return Boarded(p, tint, 56.0, 0.85);
		case CLAD_PLANKS: return Planked(p, tint, 36.0);
		case CLAD_CORRUGATED: return Ribbed(p, tint, 70.0, 0.35, 0.35);
		case CLAD_METAL: return Panelled(p, tint, vec2(9.0, 0.0), 0.04, 0.3);
		case CLAD_GLASS: return Glazed(p, tint, vec2(12.0, 10.0), 0.08);
		case CLAD_LATTICE: return Matte(tint, 0.5);
		case CLAD_CLAY_TILE: return Tiled(p, aged, vec2(34.0, 42.0), CLAY_TILE_VARIETY, 0.7, 0.3);
		case CLAD_SLATE: return Tiled(p, aged, vec2(28.0, 60.0), SLATE_VARIETY, 0.4, 0.0);
		case CLAD_SHINGLE: return Tiled(p, aged, vec2(22.0, 50.0), SHINGLE_VARIETY, 0.8, 0.0);
		case CLAD_THATCH: return Mottled(vec2(p.x * 8.0, p.y), tint, 9.0, 0.3, 0.95);
		case CLAD_METAL_SEAM: return Ribbed(vec2(p.x * 0.25, p.y), tint, 80.0, 0.12, 0.28);
		case CLAD_GRAVEL: return Gravelled(p, aged);
		case CLAD_MEMBRANE: return Membrane(p, aged);
		case CLAD_ROOF_DECK: return Planked(p, tint, 30.0);
		case CLAD_GLASS_ROOF: return Glazed(p, vec3(0.7), vec2(14.0, 5.0), 0.1);
		case CLAD_ASPHALT: return Mottled(p, tint, 60.0, 0.12, 0.9);
		case CLAD_HEDGE: return Leafy(p, tint);
		case CLAD_PICKETS: return Matte(tint, 0.7);
		case CLAD_CANVAS: return Striped(p, tint);
		case CLAD_SIGN: return Lightbox(p, tint);
		case CLAD_WATER: return Rippled(p, tint);
		default: return Mottled(p, tint, 10.0, 0.05, 0.85);
	}
}
