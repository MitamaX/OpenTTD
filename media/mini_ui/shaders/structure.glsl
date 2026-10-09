const float TAU = 6.2831853;
const float TEXELS = float(TEXELS_PER_TILE);
const float PLINTH_HEIGHT = 0.016;
const float COURSE_TILT = 0.2;
const vec2 COURSE_SHADES = vec2(0.72, 1.06);
const vec2 COURSE_LIGHT_EDGES = vec2(0.0, 0.4);
const float COURSE_OVERLAP_SHADE = 0.12;
const vec2 COURSE_OVERLAP_EDGES = vec2(0.88, 1.0);
const float TILE_GAP_WIDTH = 0.06;
const float TILE_GAP_SHADE = 0.35;
const float FIRING_BLOTCHES = 4.5;
const vec2 BLOTCH_EDGES = vec2(0.2, 0.8);
const float BLOTCH_VARIETY = 1.5;
const float BLOTCH_SEED = 3.7;
const vec2 RAIN_STREAKS = vec2(13.0, 1.4);
const float STREAK_DEPTH = 0.16;
const float STREAK_SEED = 9.2;
const float LICHEN_PATCHES = 2.7;
const vec2 LICHEN_PATCH_EDGES = vec2(0.62, 0.86);
const float LICHEN_PATCH_SEED = 1.3;
const float LICHEN_SPOTS = 40.0;
const vec2 LICHEN_SPOT_EDGES = vec2(0.565, 0.765);
const float LICHEN_SPOT_MEAN = 0.25;
const float LICHEN_SPOT_SEED = 6.1;
const float LICHEN_COVER = 0.45;
const vec3 LICHEN = vec3(0.62, 0.61, 0.49);
const vec3 DIM_ROOM = vec3(0.4, 0.38, 0.35);

/* What a point of a building is: its colour before light, how its surface tilts off the face across and up it,
 * how rough and how much glass it is, how open it lies to the sky, and how brightly lamps light the room behind its glass. */
struct Clad {
	vec3 albedo;
	vec2 tilt;
	float roughness;
	float glass;
	float occlusion;
	float lamp;
};

Clad Matte(vec3 colour, float roughness)
{
	return Clad(colour, vec2(0.0), roughness, 0.0, 1.0, 0.0);
}

Clad Mixed(Clad a, Clad b, float share)
{
	return Clad(mix(a.albedo, b.albedo, share), mix(a.tilt, b.tilt, share), mix(a.roughness, b.roughness, share), mix(a.glass, b.glass, share), mix(a.occlusion, b.occlusion, share), mix(a.lamp, b.lamp, share));
}

/* How sharply a pattern repeating once a unit of x still shows: whole while a repeat spans a few pixels, gone once it blurs. */
float Sharpness(float x)
{
	return 1.0 - smoothstep(0.2, 0.45, fwidth(x));
}

float Sharpness(vec2 q)
{
	return min(Sharpness(q.x), Sharpness(q.y));
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

/* How much of a pixel lines of this width, one a unit, cover; lines too fine to show blend into the share they cover. */
float Lines(float x, float width, float sharpness)
{
	float pixel = fwidth(x);
	float gap = abs(fract(x + 0.5) - 0.5);
	float line = 1.0 - smoothstep(width * 0.5 - pixel * 0.5, width * 0.5 + pixel * 0.5, gap);
	return mix(width, line, sharpness);
}

float Lines(float x, float width)
{
	return Lines(x, width, Sharpness(x));
}

float Spread(vec2 cell, float variety, float sharpness)
{
	return 1.0 + variety * (Hash(ivec2(floor(cell))) - 0.5) * sharpness;
}

/* Units laid in courses with every other course shifted half a unit, the joints between them filled. */
Clad Coursed(vec2 p, vec3 tint, vec2 units, float joint, float variety, vec3 filling)
{
	float course = p.y * units.y;
	float column = p.x * units.x + 0.5 * floor(course);
	float joints = max(Lines(course, joint), Lines(column, joint * units.x / units.y));
	vec3 unit = tint * Spread(vec2(column, course), variety, Sharpness(course));
	return Matte(mix(unit, filling, joints), 0.85);
}

float Faded(vec2 q, float seed)
{
	float sharp = Sharpness(q);
	return sharp > 0.0 ? mix(0.5, Noise(q + seed), sharp) : 0.5;
}

float Faded(vec2 q, float seed, vec2 edges, float mean)
{
	float sharp = Sharpness(q);
	return sharp > 0.0 ? mix(mean, smoothstep(edges.x, edges.y, Noise(q + seed)), sharp) : mean;
}

vec3 Weathered(vec2 p, vec3 tint, float variety)
{
	if (Landscape() == LANDSCAPE_TOYLAND) return tint;
	float blotches = Faded(p * FIRING_BLOTCHES, BLOTCH_SEED, BLOTCH_EDGES, 0.5);
	float streaks = Faded(p * RAIN_STREAKS, STREAK_SEED);
	float patched = Faded(p * LICHEN_PATCHES, LICHEN_PATCH_SEED, LICHEN_PATCH_EDGES, 0.0);
	float spotted = Faded(p * LICHEN_SPOTS, LICHEN_SPOT_SEED, LICHEN_SPOT_EDGES, LICHEN_SPOT_MEAN);
	vec3 weathered = tint * Varied(blotches, variety * BLOTCH_VARIETY) * Varied(streaks, STREAK_DEPTH);
	return mix(weathered, LICHEN, patched * spotted * LICHEN_COVER);
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
	vec3 colour = Weathered(p, tint, variety) * shade * Spread(vec2(column, course), variety, sharp);
	float gaps = Lines(column, TILE_GAP_WIDTH, rolled) * sharp;
	float unseen_tilt = WaveSpread(roll) * (1.0 - rolled) + RampSpread(COURSE_TILT) * (1.0 - sharp);
	Clad clad = Matte(colour * (1.0 - TILE_GAP_SHADE * gaps), Roughened(roughness, unseen_tilt));
	clad.tilt = vec2(roll * sin(TAU * column) * rolled, -COURSE_TILT * sharp * (along - 0.5));
	return clad;
}

Clad Mottled(vec2 p, vec3 tint, float frequency, float variety, float roughness)
{
	float noise = mix(0.5, Noise(p * frequency), Sharpness(p.x * frequency));
	return Matte(tint * Varied(noise, variety), roughness);
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
	clad.tilt = vec2(depth * sin(TAU * phase) * sharp, 0.0);
	return clad;
}

Clad Boarded(vec2 p, vec3 tint, float boards, float roughness)
{
	float board = p.y * boards;
	float sharp = Sharpness(board);
	float shade = mix(1.0, mix(0.84, 1.06, fract(board)), sharp);
	Clad clad = Matte(tint * shade * Spread(vec2(0.0, board), 0.1, sharp), roughness);
	clad.tilt = vec2(0.0, 0.25 * sharp);
	return clad;
}

Clad Planked(vec2 p, vec3 tint, float planks)
{
	float plank = p.x * planks;
	float sharp = Sharpness(plank);
	vec3 colour = tint * Spread(vec2(plank, 0.0), 0.18, sharp) * (1.0 - 0.4 * Lines(plank, 0.08));
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

float Band(float x, float low, float high)
{
	float pixel = fwidth(x);
	return smoothstep(low - pixel, low + pixel, x) - smoothstep(high - pixel, high + pixel, x);
}

/* The members of a braced lattice, as a share of the point they cover. */
float LatticeCover(vec2 p)
{
	vec2 cell = p * 14.0;
	return max(max(Lines(cell.x, 0.16), Lines(cell.y, 0.16)), max(Lines(cell.x + cell.y, 0.12), Lines(cell.x - cell.y, 0.12)));
}

/* The pickets of a fence and the two rails they are nailed to. */
float PicketCover(vec2 p)
{
	return max(Lines(p.x * 45.0, 0.5), max(Band(p.y, 0.006, 0.0095), Band(p.y, 0.017, 0.0205)));
}

/* How much of a point an open cladding covers; any other cladding covers it whole. */
float Cover(uint material, vec2 p)
{
	switch (material) {
		case CLAD_LATTICE: return LatticeCover(p);
		case CLAD_PICKETS: return PicketCover(p);
		default: return 1.0;
	}
}

Clad Leafy(vec2 p, vec3 tint)
{
	float sharp = Sharpness(p.x * 60.0);
	float clumps = mix(0.5, Noise(p * 60.0), sharp);
	Clad clad = Matte(tint * Varied(clumps, 0.45) * Varied(mix(0.5, Noise(p * 17.0), Sharpness(p.x * 17.0)), 0.25), 0.95);
	clad.tilt = (vec2(Noise(p * 60.0 + 3.1), Noise(p * 60.0 + 7.7)) - 0.5) * 0.9 * sharp;
	return clad;
}

bool IsMasonry(uint material)
{
	return material == CLAD_BRICK || material == CLAD_STONE || material == CLAD_RENDER || material == CLAD_CONCRETE || material == CLAD_TIMBER;
}

/* Masonry walls stand on a darker plinth. */
float Plinth(uint material, vec2 p, float upright)
{
	if (!IsMasonry(material)) return 1.0;
	float pixel = fwidth(p.y);
	return mix(1.0, mix(0.72, 1.0, smoothstep(PLINTH_HEIGHT - pixel, PLINTH_HEIGHT + pixel, p.y)), step(0.7, upright));
}

Clad CladOf(uint material, vec2 p, vec3 tint)
{
	vec3 mortar = mix(tint, vec3(0.78, 0.75, 0.70), 0.7);
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
		case CLAD_CLAY_TILE: return Tiled(p, tint, vec2(34.0, 42.0), 0.14, 0.7, 0.3);
		case CLAD_SLATE: return Tiled(p, tint, vec2(28.0, 60.0), 0.16, 0.4, 0.0);
		case CLAD_SHINGLE: return Tiled(p, tint, vec2(22.0, 50.0), 0.22, 0.8, 0.0);
		case CLAD_THATCH: return Mottled(vec2(p.x * 8.0, p.y), tint, 9.0, 0.3, 0.95);
		case CLAD_METAL_SEAM: return Ribbed(vec2(p.x * 0.25, p.y), tint, 80.0, 0.12, 0.28);
		case CLAD_GRAVEL: return Mottled(p, tint, 110.0, 0.22, 0.95);
		case CLAD_MEMBRANE: return Panelled(p, tint, vec2(4.0, 0.0), 0.015, 0.8);
		case CLAD_ROOF_DECK: return Planked(p, tint, 30.0);
		case CLAD_GLASS_ROOF: return Glazed(p, vec3(0.7), vec2(14.0, 5.0), 0.1);
		case CLAD_ASPHALT: return Mottled(p, tint, 60.0, 0.12, 0.9);
		case CLAD_HEDGE: return Leafy(p, tint);
		case CLAD_PICKETS: return Matte(tint, 0.7);
		default: return Mottled(p, tint, 10.0, 0.05, 0.85);
	}
}
