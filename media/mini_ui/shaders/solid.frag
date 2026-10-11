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

const float GRAIN_FREQUENCY = 9.0;
const float GRAIN_VARIETY = 0.22;
const vec2 PATTERN_SHEAR = vec2(0.7, -0.4);
const vec3 STAINS_PER_TILE = vec3(0.35, 1.1, 3.2);
const vec3 STAIN_VARIETY = vec3(0.22, 0.16, 0.12);
const vec3 STAIN_SEEDS = vec3(3.1, 7.3, 11.9);
const float MENDS_PER_TILE = 1.4;
const float MENDED_SHARE = 0.35;
const vec2 MEND_HALF_LENGTHS = vec2(0.22, 0.46);
const vec2 MEND_HALF_WIDTHS = vec2(0.05, 0.15);
const ivec2 MEND_SIZE_SEED = ivec2(101, 37);
const float MENDS_TURNED_SHARE = 0.5;
const float MEND_FEATHER = 0.03;
const float MENDED_SHADE = 0.84;
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
const vec2 DRIFT_REACH = vec2(0.3, 0.8);
const vec2 DRIFT_FACING = vec2(0.5, 0.85);
const vec2 DRIFT_SEED = vec2(3.3, 8.8);
const float ASPHALT_GRAIN_PER_TILE = 48.0;
const float ASPHALT_GRAIN_VARIETY = 0.16;
const uint ALONG_Y_ENDS = 5u;
const uint ALONG_X_ENDS = 10u;
const float LANE_CENTRE = ROAD_HALF * 0.5;
const float WHEEL_HALF_GAUGE = 0.05;
const float WHEEL_TRACK_HALF = 0.022;
const float WHEEL_TRACK_SHADE = 0.9;
const float WHEEL_TRACK_BREAKS_PER_TILE = 2.5;
const vec2 WHEEL_TRACK_BREAK_EDGES = vec2(0.25, 0.55);
const float VERGE_DUST_REACH = 0.06;
const vec3 VERGE_DUST = vec3(1.14, 1.1, 1.02);
const float ROAD_CRACKS_PER_TILE = 3.5;
const float ROAD_CRACK_WIDTH = 0.02;
const vec2 ROAD_CRACK_RUN_EDGES = vec2(0.55, 0.65);
const float ROAD_CRACK_SHADE = 0.75;
const vec2 ROAD_CRACK_SEED = vec2(8.1, 1.7);
const float ROAD_CRACK_JAGS = 7.0;
const float ROAD_CRACK_JAGGED = 0.35;
const float MARK_WEAR_PER_TILE = 6.0;
const float MARK_FLAKES_PER_TILE = 40.0;
const float MARK_FLAKING = 0.3;
const vec2 MARK_WEAR_EDGES = vec2(0.4, 0.62);
const float MARK_WEAR = 0.75;
const vec2 MARK_WEAR_SEED = vec2(4.2, 6.9);
const float SLABS_PER_TILE = 7.0;
const float SLAB_JOINT = 0.06;
const float SLAB_JOINT_SHADE = 0.78;
const float SLAB_VARIETY = 0.12;
const vec2 X_AXIS = vec2(1.0, 0.0);
const vec2 Y_AXIS = vec2(0.0, 1.0);

vec2 PatternPoint(vec3 position)
{
	return position.xy + position.zz * PATTERN_SHEAR;
}

struct Footprint {
	mat2 ground;
	mat2 pattern;
};

Footprint FootprintOf(vec3 position)
{
	vec2 pattern = PatternPoint(position);
	return Footprint(mat2(dFdx(position.xy), dFdy(position.xy)), mat2(dFdx(pattern), dFdy(pattern)));
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

float Mended(vec2 p, float facing_up, mat2 pattern_span)
{
	vec2 q = p * MENDS_PER_TILE;
	mat2 cells = pattern_span * MENDS_PER_TILE;
	float pixel = length(vec2(PixelAcross(cells, X_AXIS), PixelAcross(cells, Y_AXIS)));
	ivec2 cell = ivec2(floor(q));
	float dice = Hash(cell);
	if (dice > MENDED_SHARE) return 0.0;
	vec2 size = Hash2(cell + MEND_SIZE_SEED);
	vec2 half_size = vec2(mix(MEND_HALF_LENGTHS.x, MEND_HALF_LENGTHS.y, size.x), mix(MEND_HALF_WIDTHS.x, MEND_HALF_WIDTHS.y, size.y));
	vec2 offset = abs(fract(q) - HALF_TILE);
	vec2 outside = mix(offset, offset.yx, step(dice, MENDED_SHARE * MENDS_TURNED_SHARE)) - half_size;
	float edge = max(outside.x, outside.y);
	return (1.0 - smoothstep(-pixel - MEND_FEATHER, pixel, edge)) * smoothstep(MENDED_FACING.x, MENDED_FACING.y, facing_up) * Resolved(MENDS_PER_TILE * 2.0);
}

float Worn(vec3 position, vec3 normal, float gloss, mat2 pattern_span)
{
	float worn = u_weathering * (1.0 - gloss);
	if (worn <= 0.0 || !Weathers()) return 1.0;
	vec2 p = PatternPoint(position);
	return mix(1.0, Stains(p) * mix(1.0, MENDED_SHADE, Mended(p, normal.z, pattern_span)), worn);
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

float WearShown(float frequency)
{
	return Weathers() ? Resolved(frequency) : 0.0;
}

vec3 Trafficked(vec2 p, mat2 ground_span)
{
	ivec2 tile = ivec2(floor(p));
	uint ends = texelFetch(u_network, Clamped(tile), 0).y;
	bool along_x = ends == ALONG_X_ENDS;
	if (!along_x && ends != ALONG_Y_ENDS) return vec3(1.0);
	vec2 f = p - vec2(tile) - TILE_CENTRE;
	float lateral = along_x ? f.y : f.x;
	float along = along_x ? p.x : p.y;
	float wheel = abs(abs(abs(lateral) - LANE_CENTRE) - WHEEL_HALF_GAUGE);
	float unbroken = smoothstep(WHEEL_TRACK_BREAK_EDGES.x, WHEEL_TRACK_BREAK_EDGES.y, Noise(vec2(along * WHEEL_TRACK_BREAKS_PER_TILE, lateral * ROAD_CRACKS_PER_TILE)));
	float tracks = Stroke(wheel, WHEEL_TRACK_HALF, PixelAcross(ground_span, along_x ? Y_AXIS : X_AXIS)) * unbroken * WearShown(1.0 / WHEEL_TRACK_HALF);
	float dust = smoothstep(ROAD_HALF - VERGE_DUST_REACH, ROAD_HALF, abs(lateral)) * WearShown(1.0 / VERGE_DUST_REACH);
	return mix(vec3(1.0), VERGE_DUST, dust) * mix(1.0, WHEEL_TRACK_SHADE, tracks);
}

float RoadCracks(vec2 p, mat2 pattern_span)
{
	float shown = WearShown(ROAD_CRACKS_PER_TILE * 8.0);
	if (shown <= 0.0) return 1.0;
	vec2 q = p * ROAD_CRACKS_PER_TILE;
	vec2 jag = (vec2(Noise(q * ROAD_CRACK_JAGS), Noise(q * ROAD_CRACK_JAGS + ROAD_CRACK_SEED)) - 0.5) * ROAD_CRACK_JAGGED;
	vec3 crack = NoiseSlope(q + jag);
	float steepness = max(length(crack.yz), LEAST_DIVISOR);
	float vein = abs(crack.x - 0.5) / steepness;
	float runs = smoothstep(ROAD_CRACK_RUN_EDGES.x, ROAD_CRACK_RUN_EDGES.y, Noise(p * ROAD_CRACKS_PER_TILE * 0.8 + ROAD_CRACK_SEED));
	return mix(1.0, ROAD_CRACK_SHADE, Stroke(vein, ROAD_CRACK_WIDTH, PixelAcross(pattern_span * ROAD_CRACKS_PER_TILE, crack.yz / steepness)) * runs * shown);
}

vec3 Asphalted(vec3 tone, vec3 position, vec3 normal, Footprint footprint)
{
	vec2 p = PatternPoint(position);
	vec3 asphalt = tone * Varied(Layered(p, ASPHALT_GRAIN_PER_TILE), ASPHALT_GRAIN_VARIETY) * Worn(position, normal, 0.0, footprint.pattern);
	return asphalt * Trafficked(position.xy, footprint.ground) * RoadCracks(p, footprint.pattern);
}

vec3 Marked(vec3 tone, vec3 position)
{
	vec2 p = PatternPoint(position) + MARK_WEAR_SEED;
	float flaking = (Octave(p, MARK_FLAKES_PER_TILE) - 0.5) * MARK_FLAKING;
	float worn = smoothstep(MARK_WEAR_EDGES.x, MARK_WEAR_EDGES.y, Octave(p, MARK_WEAR_PER_TILE) + flaking) * WearShown(MARK_WEAR_PER_TILE);
	return mix(tone, DISTANT_ROAD, worn * MARK_WEAR);
}

vec3 Paved(vec3 tone, vec3 position, vec3 normal, Footprint footprint)
{
	vec2 q = position.xy * SLABS_PER_TILE;
	q.x += 0.5 * floor(q.y);
	vec2 joint = 0.5 - abs(fract(q) - 0.5);
	float shown = Resolved(SLABS_PER_TILE * 2.0);
	mat2 slabs = footprint.ground * SLABS_PER_TILE;
	float joints = max(Stroke(joint.x, SLAB_JOINT, PixelAcross(slabs, X_AXIS)), Stroke(joint.y, SLAB_JOINT, PixelAcross(slabs, Y_AXIS))) * shown;
	float slab = Varied(Hash(ivec2(floor(q))), SLAB_VARIETY * shown);
	return tone * slab * mix(1.0, SLAB_JOINT_SHADE, joints) * Worn(position, normal, 0.0, footprint.pattern);
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

vec3 Bedded(vec3 tone, uint bed, vec3 position, vec3 normal, Footprint footprint)
{
	switch (bed) {
		case BED_BALLAST:
		case BED_SHOULDER: return Ballasted(tone, bed, position, normal);
		case BED_ASPHALT: return Asphalted(tone, position, normal, footprint);
		case BED_MARKING: return Marked(tone, position);
		case BED_PAVING: return Paved(tone, position, normal, footprint);
	}
	return tone;
}

/* How far a solid has sunk into the ground it stands on: from where it stands whole at the first pixels per tile given, by the second's share by the time it fades. */
float Receded()
{
	return u_recede.y * (1.0 - smoothstep(u_fade.y, u_recede.x, tile_pixels));
}

/* Solids fade out where they grow too small to show, dithered, handing over to what the ground paints in their place;
 * some first sink into the ground over a longer reach, thinning and lying flatter so their sides draw no dark lines.
 * Under the overlay, ways take their layer's accent, while solids keeping their own colours only sink when out of it.
 * A trait is how glossy a surface is, how brightly it glows below zero, or the code of the bed a way is laid with. */
void main()
{
	tile_pixels = TilePixelsAt(v_position);
	float receded = Receded();
	Footprint footprint = FootprintOf(v_position);
	if (smoothstep(u_fade.x, u_fade.y, tile_pixels) * (1.0 - receded) <= ScreenNoise(gl_FragCoord.xy)) discard;
	uint bed = BedOf(v_trait);
	float gloss = bed == BED_PLAIN ? max(v_trait, 0.0) : 0.0;
	vec3 normal = normalize(mix(normalize(v_normal), vec3(0.0, 0.0, 1.0), receded / max(u_recede.y, LEAST_DIVISOR)));
	vec3 grained = bed == BED_PLAIN ? v_colour.rgb * Grain(v_position, gloss) * Worn(v_position, normal, gloss, footprint.pattern) : Bedded(v_colour.rgb, bed, v_position, normal, footprint);
	vec3 albedo = Linear(u_own_colours ? Kept(grained, u_way) : Overlaid(grained, u_way));
	frag_colour = Radiance(albedo, normal, v_position, CoatOf(gloss), v_colour.a) + vec4(Glowing(albedo, v_trait), 0.0);
}
