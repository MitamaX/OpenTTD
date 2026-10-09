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

float Weathered(vec3 position, vec3 normal, float gloss)
{
	float worn = u_weathering * (1.0 - gloss);
	if (worn <= 0.0) return 1.0;
	vec2 p = PatternPoint(position);
	return mix(1.0, Stains(p) * mix(1.0, MENDED_SHADE, Mended(p, normal.z)), worn);
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
	float gloss = max(v_trait, 0.0);
	vec3 normal = normalize(mix(normalize(v_normal), vec3(0.0, 0.0, 1.0), receded / max(u_recede.y, MIN_SPREAD)));
	vec3 grained = v_colour.rgb * Grain(v_position, gloss) * Weathered(v_position, normal, gloss);
	vec3 albedo = Linear(u_own_colours ? Kept(grained, u_way) : Overlaid(grained, u_way));
	Coat coat = Coat(mix(MATTE, POLISHED, gloss), mix(DIELECTRIC_REFLECTANCE, GLASS_REFLECTANCE, smoothstep(MIRROR_GLOSS.x, MIRROR_GLOSS.y, gloss)));
	vec4 lit = Radiance(albedo, normal, v_position, coat, v_colour.a);
	lit.rgb += albedo * max(-v_trait, 0.0) * GLOW_RADIANCE;
	frag_colour = lit;
}
