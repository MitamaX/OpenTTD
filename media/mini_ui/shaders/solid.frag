uniform int u_way;
uniform bool u_own_colours;
uniform vec2 u_fade;
uniform vec2 u_recede;
uniform float u_weathering;

in vec3 v_position;
in vec3 v_normal;
in vec4 v_colour;
in float v_trait;

out vec4 frag_colour;

const float MATTE = 0.85;
const float POLISHED = 0.2;
const float GLOW_RADIANCE = 5.0;
const float GRAIN_FREQUENCY = 9.0;
const float GRAIN_VARIETY = 0.22;
const vec2 MIRROR_GLOSS = vec2(0.6, 0.8);
const vec2 PATTERN_SHEAR = vec2(0.7, -0.4);
const vec3 STAINS_PER_TILE = vec3(0.35, 1.1, 3.2);
const vec3 STAIN_VARIETY = vec3(0.22, 0.16, 0.12);
const vec3 STAIN_SEEDS = vec3(3.1, 7.3, 11.9);
const float MENDS_PER_TILE = 1.4;
const float MENDED_SHARE = 0.35;
const vec2 MEND_HALF_SIZES = vec2(0.12, 0.42);
const ivec2 MEND_SIZE_SEED = ivec2(101, 37);
const float MENDED_SHADE = 0.78;
const vec2 MENDED_FACING = vec2(0.7, 0.9);
const float STONES_PER_TILE = 80.0;
const ivec2 STONE_SEED = ivec2(613, 1949);
const float STONE_NEIGHBOURHOOD_REACH = 8.0;
const float STONE_VARIETY = 0.5;
const vec2 STONE_GAP_EDGES = vec2(0.02, 0.18);
const float STONE_GAP_SHADE = 0.6;
const float STONE_MEAN = 0.9;
const float BALLAST_CLUMPS_PER_TILE = 22.0;
const float BALLAST_VARIETY = 0.3;
const float OIL_STAINS_PER_TILE = 2.4;
const vec2 OIL_STAIN_EDGES = vec2(0.5, 0.78);
const vec2 OIL_SEED = vec2(5.7, 2.3);
const vec3 OIL_STAIN = vec3(0.66, 0.6, 0.54);
const float WEEDS_PER_TILE = 4.5;
const float WEED_TUFTS_PER_TILE = 45.0;
const float WEED_TUFT_RAGGED = 0.7;
const vec2 WEED_EDGES = vec2(0.58, 0.72);
const vec2 WEED_SEED = vec2(9.4, 6.6);
const vec3 WEEDS = vec3(0.28, 0.34, 0.17);
const float WEED_VARIETY = 0.4;
const float DRIFTS_PER_TILE = 3.0;
const float DRIFT_RAGGED = 0.6;
const vec2 DRIFT_EDGES = vec2(0.35, 0.65);
const vec2 DRIFT_REACH = vec2(0.6, 1.0);
const vec2 DRIFT_FACING = vec2(0.5, 0.85);
const vec2 DRIFT_SEED = vec2(3.3, 8.8);

vec2 PatternPoint(vec3 position)
{
	return position.xy + position.zz * PATTERN_SHEAR;
}

/* Matte surfaces show a fine grain where they come near enough for it to resolve; glossy ones stay clean. */
float Grain(vec3 position, float gloss)
{
	vec2 p = PatternPoint(position);
	return mix(Varied(Layered(p, GRAIN_FREQUENCY), GRAIN_VARIETY), 1.0, gloss);
}

float Stains(vec2 p)
{
	return Varied(Octave(p + STAIN_SEEDS.x, STAINS_PER_TILE.x), STAIN_VARIETY.x) * Varied(Octave(p + STAIN_SEEDS.y, STAINS_PER_TILE.y), STAIN_VARIETY.y) * Varied(Octave(p + STAIN_SEEDS.z, STAINS_PER_TILE.z), STAIN_VARIETY.z);
}

float Mended(vec2 p, float facing_up)
{
	vec2 q = p * MENDS_PER_TILE;
	float pixel = length(fwidth(q));
	ivec2 cell = ivec2(floor(q));
	if (Hash(cell) > MENDED_SHARE) return 0.0;
	vec2 half_size = mix(vec2(MEND_HALF_SIZES.x), vec2(MEND_HALF_SIZES.y), Hash2(cell + MEND_SIZE_SEED));
	vec2 outside = abs(fract(q) - HALF_TILE) - half_size;
	return (1.0 - smoothstep(-pixel, pixel, max(outside.x, outside.y))) * smoothstep(MENDED_FACING.x, MENDED_FACING.y, facing_up) * Resolved(MENDS_PER_TILE * 2.0);
}

float Worn(vec3 position, vec3 normal, float gloss)
{
	float worn = u_weathering * (1.0 - gloss);
	if (worn <= 0.0 || !Weathers()) return 1.0;
	vec2 p = PatternPoint(position);
	return mix(1.0, Stains(p) * mix(1.0, MENDED_SHADE, Mended(p, normal.z)), worn);
}

uint BedOf(float trait)
{
	uint code = uint(round(max(trait, 0.0) * TRAIT_SCALE));
	return code < WAY_BEDS ? code : BED_PLAIN;
}

float Stones(vec2 p)
{
	float shown = Resolved(STONES_PER_TILE);
	if (shown <= 0.0) return STONE_MEAN;
	vec2 q = p * STONES_PER_TILE;
	ivec2 cell = ivec2(floor(q));
	vec2 nearest = vec2(STONE_NEIGHBOURHOOD_REACH);
	float shade = 0.5;
	for (int j = -1; j <= 1; j++) {
		for (int i = -1; i <= 1; i++) {
			ivec2 near = cell + ivec2(i, j);
			vec2 offset = vec2(near) + Hash2(near) - q;
			float reach = dot(offset, offset);
			if (reach < nearest.x) {
				nearest = vec2(reach, nearest.x);
				shade = Hash(near + STONE_SEED);
			} else {
				nearest.y = min(nearest.y, reach);
			}
		}
	}
	float gap = sqrt(nearest.y) - sqrt(nearest.x);
	float stone = Varied(shade, STONE_VARIETY) * mix(STONE_GAP_SHADE, 1.0, smoothstep(STONE_GAP_EDGES.x, STONE_GAP_EDGES.y, gap));
	return mix(STONE_MEAN, stone, shown);
}

vec3 Drifted(vec3 ground, uint bed, vec2 at, vec2 p, vec3 normal)
{
	if (Landscape() != LANDSCAPE_ARCTIC && Landscape() != LANDSCAPE_TROPIC) return ground;
	float blanket = ClimateAt(at).blanket;
	if (blanket <= 0.0) return ground;
	float reach = blanket * (bed == BED_BALLAST ? DRIFT_REACH.x : DRIFT_REACH.y);
	float lying = smoothstep(DRIFT_EDGES.x, DRIFT_EDGES.y, reach + (Layered(p + DRIFT_SEED, DRIFTS_PER_TILE) - 0.5) * DRIFT_RAGGED);
	return mix(ground, Landscape() == LANDSCAPE_ARCTIC ? SNOW : SAND, lying * smoothstep(DRIFT_FACING.x, DRIFT_FACING.y, normal.z));
}

vec3 Ballasted(vec3 tone, uint bed, vec3 position, vec3 normal)
{
	vec2 p = PatternPoint(position);
	vec3 ballast = tone * Varied(Layered(p, BALLAST_CLUMPS_PER_TILE), BALLAST_VARIETY) * Stones(p);
	if (Weathers()) {
		float stained = bed == BED_BALLAST ? smoothstep(OIL_STAIN_EDGES.x, OIL_STAIN_EDGES.y, Octave(p + OIL_SEED, OIL_STAINS_PER_TILE)) : 0.0;
		float tufts = (Octave(p, WEED_TUFTS_PER_TILE) - 0.5) * WEED_TUFT_RAGGED;
		float weedy = bed == BED_SHOULDER ? smoothstep(WEED_EDGES.x, WEED_EDGES.y, Layered(p + WEED_SEED, WEEDS_PER_TILE) + tufts) : 0.0;
		ballast = mix(ballast * mix(vec3(1.0), OIL_STAIN, stained), WEEDS * Varied(Octave(p, BALLAST_CLUMPS_PER_TILE), WEED_VARIETY), weedy);
	}
	return Drifted(ballast, bed, position.xy, p, normal);
}

/* How far a solid has sunk into the ground it stands on: from where it stands whole at the first pixels per tile given, by the second's share by the time it fades. */
float Receded()
{
	return u_recede.y * (1.0 - smoothstep(u_fade.y, u_recede.x, tile_pixels));
}

/* Solids fade out where they grow too small to show, dithered, handing over to what the ground paints in their place;
 * some first sink into the ground over a longer reach, thinning and lying flatter so their sides draw no dark lines.
 * Under the overlay, ways take their layer's accent, while solids keeping their own colours only sink when out of it.
 * A positive trait is how glossy the surface is, a negative one how brightly it glows; glass, the glossiest, mirrors the sky. */
void main()
{
	tile_pixels = TilePixelsAt(distance(Eye(), v_position));
	float receded = Receded();
	if (smoothstep(u_fade.x, u_fade.y, tile_pixels) * (1.0 - receded) <= ScreenNoise(gl_FragCoord.xy)) discard;
	uint bed = BedOf(v_trait);
	float gloss = bed == BED_PLAIN ? max(v_trait, 0.0) : 0.0;
	vec3 normal = normalize(mix(normalize(v_normal), vec3(0.0, 0.0, 1.0), receded / max(u_recede.y, MIN_SPREAD)));
	vec3 grained = bed == BED_PLAIN ? v_colour.rgb * Grain(v_position, gloss) * Worn(v_position, normal, gloss) : Ballasted(v_colour.rgb, bed, v_position, normal);
	vec3 albedo = Linear(u_own_colours ? Kept(grained, u_way) : Overlaid(grained, u_way));
	Coat coat = Coat(mix(MATTE, POLISHED, gloss), mix(DIELECTRIC_REFLECTANCE, GLASS_REFLECTANCE, smoothstep(MIRROR_GLOSS.x, MIRROR_GLOSS.y, gloss)));
	vec4 lit = Radiance(albedo, normal, v_position, coat, v_colour.a);
	lit.rgb += albedo * max(-v_trait, 0.0) * GLOW_RADIANCE;
	frag_colour = lit;
}
