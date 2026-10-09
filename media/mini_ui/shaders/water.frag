uniform sampler2D u_scene_colour;
uniform sampler2D u_scene_depth;
uniform vec4 u_wake_hulls[MOST_WAKES];
uniform vec4 u_wake_shapes[MOST_WAKES];
uniform int u_wakes;

in vec3 v_world;
in vec3 v_position;

out vec4 frag_colour;

const float TAU = 6.2831853;
const vec3 UP = vec3(0.0, 0.0, 1.0);

const int WAVE_OCTAVES = 5;
const float WAVE_FREQUENCY = 0.55;
const float WAVE_LACUNARITY = 1.95;
const float WAVE_PERSISTENCE = 0.55;
const float WAVE_TURN = 1.17;
const float WAVE_SPEED = 0.32;
const float WAVE_STEEPNESS = 0.38;
const float CALM_STEEPNESS = 0.16;
const float CALM_FREQUENCY = 1.8;
const float RESOLVED_WAVE_PIXELS = 14.0;
const float UNRESOLVED_WAVE_PIXELS = 4.0;
const float GUSTS_PER_TILE = 0.04;
const float GUST_DRIFT = 0.02;
const float LULL_STEEPNESS = 0.5;
const float GUST_STEEPNESS = 1.2;
const vec2 WIND = vec2(0.8, 0.6);
const float SWELL_PER_TILE = 0.11;
const float SWELL_SPEED = 0.25;
const float SWELL_STEEPNESS = 0.06;
const float RUFFLED_SPREAD = 0.35;
const float RUFFLED_DIMMING = 0.45;
const vec2 STREAK_STRETCH = vec2(0.9, 5.0);
const vec2 STREAK_SEED = vec2(31.7, 11.3);
const float MIN_STREAK_SHARE = 0.02;
const float FLOW_PACE = 1.2;
const float FLOW_CYCLE = 1.5;
const float FLOW_PHASE_SPREAD = 0.37;
const vec2 FLOW_PHASE_SEED = vec2(5.2, 1.3);
const float FLOW_STEEPNESS = 0.02;
const float FLOW_STILLING = 0.6;
const float PAST_EDGE_FLOW_TILES = 5.0;
const float MIN_PACE = 0.01;
const float FLECK_FOAM = 0.45;
const float FLECK_SCALE = 4.0;
const float FLECK_SHARE = 0.5;
const float FLECK_MARGIN = 0.25;
const vec2 FLECK_EXTENT = vec2(0.24, 0.12);
const float FLECK_SMALLEST = 0.5;
const float FLECK_SOFTNESS = 0.35;
const ivec2 FLECK_SEED = ivec2(7919, 3571);
const vec2 FLECK_PACES = vec2(0.14, 0.3);
const float LOWEST_PITCH_SINE = 0.3;
const float RAPIDS_TILT = 0.12;
const float RAPIDS_REACH = 0.35;
const float RAPIDS_SPEED = 1.3;
const float RAPIDS_STEEPNESS = 0.045;
const float RAPIDS_FOAM = 0.8;
const float RAPIDS_ALONG = 2.6;
const float RAPIDS_ACROSS = 9.0;
const float RAPIDS_PATH = 0.4;
const float RAPIDS_RUFFLE = 0.8;
const float LIP_BEND = 0.25;
const float PLUNGE_BEND = 0.2;
const float PLUNGE_FROTH = 0.2;
const float CREST_GLOW = 0.35;

const float WATER_REFLECTANCE = 0.02;
const float INLAND_REFLECTANCE = 0.035;
const float REFLECTANCE_CEILING = 0.62;
const float REFLECTED_LIFT = 0.04;
const float REFLECTED_SKY = 0.9;
const int MIRROR_STEPS = 16;
const float MIRROR_REACH = 6.0;
const float MIRROR_THICKNESS = 1.0;
const float MIRROR_EDGE = 0.08;
const float REFRACTION = 0.012;
const float INLAND_REFRACTION = 0.024;
const float REFRACTION_DEPTH = 0.5;

const Coat PATH_COAT = Coat(0.3, DIELECTRIC_REFLECTANCE);
const float PATH_STRENGTH = 0.03;
const float PATH_SWAY = 0.3;
const float PATH_CEILING = 0.35;
const float GLINTS_PER_TILE = 9.0;
const Coat GLINT_COAT = Coat(0.06, DIELECTRIC_REFLECTANCE);
const float GLINT_TILT = 0.22;
const float GLINT_TWINKLE = 0.7;
const float GLINT_SIZE = 0.3;
const float GLINT_STRENGTH = 0.05;
const float GLINT_CEILING = 3.0;
const float INLAND_SPARKLE = 2.2;

const vec3 SEA_ABSORPTION = vec3(1.6, 0.55, 0.35);
const vec3 SEA_SCATTER = vec3(0.004, 0.022, 0.060);
const vec3 RIVER_ABSORPTION = vec3(3.0, 1.8, 1.6);
const vec3 RIVER_SCATTER = vec3(0.026, 0.066, 0.068);
const vec3 CANAL_ABSORPTION = vec3(2.6, 1.6, 1.4);
const vec3 CANAL_SCATTER = vec3(0.022, 0.060, 0.074);
const float INLAND_MURK = 0.45;
const float MURK_DEPTH = 0.5;
const vec3 TOY_ABSORPTION = vec3(2.4, 0.6, 0.3);
const vec3 TOY_SCATTER = vec3(0.012, 0.075, 0.2);
const float TOY_REFLECTANCE_CEILING = 0.3;

const vec3 FOAM = vec3(0.80, 0.84, 0.86);
const vec3 TOY_FOAM = vec3(0.95, 0.97, 1.0);
const vec2 TOY_WHITEWATER_EDGES = vec2(0.55, 0.61);
const float FOAM_SCALE = 3.1;
const float FOAM_DRIFT = 0.12;
const float CONTACT_DEPTH = 0.025;
const float INLAND_CONTACT_DEPTH = 0.08;
const float INLAND_FOAM = 0.45;
const float FOAM_EDGE_THRESHOLD = 0.3;
const float FOAM_OPACITY = 0.85;
const float OUTLINE_FOAM_WIDTH = 0.12;
const float STREAM_REACH = 1.0;
const float STREAM_TILT = 0.25;
const float BREAKER_REACH = 0.38;
const float BREAKERS_PER_REACH = 40.0;
const float BREAKER_SPEED = 1.3;
const float BREAKER_OPACITY = 0.7;
const float CONTACT_FOAM_REACH = 0.05;
const float SHELF_DEPTH = 0.4;
const float DECK_SHELTER = 0.75;
const float DECK_SHADE_INNER = 0.2;
const float DECK_SHADE_OUTER = 0.5;
const float SHELF_REACH = 2.5;
const float OPEN_SEA_LEVEL = 0.0;
const float OPEN_SEA_LOD = 2.0;
const vec4 OPEN_SEA = vec4(1.0, 1.0, 0.0, 0.0);
const float FETCH_LOD = 4.0;
const float FETCH_TILES = 16.0;
const vec2 OPEN_FETCH = vec2(0.12, 0.4);
const float GRAZING_SIGHT = 0.2;
const vec3 SHORE_ICE = vec3(0.8, 0.86, 0.9);
const float ICE_REACH = 0.24;
const float FLOES_PER_TILE = 3.7;
const float ICE_ROUGHNESS = 0.08;

const float KELVIN_SLOPE = 0.354;
const float WAKE_ARM_WIDTH = 0.03;
const float WAKE_ARM_SPREAD = 0.05;
const float WAKE_ARM_FOAM = 0.7;
const float WAKE_LINE_PIXELS = 1.2;
const float BOW_FOAM_LENGTHS = 0.25;
const float WASH_SPREAD = 0.25;
const float WASH_FOAM = 0.75;
const float WASH_SHARE = 0.6;
const float BOW_FLARE_LENGTHS = 0.5;
const float WAKE_STREAK_ALONG = 2.5;
const float WAKE_STREAK_ACROSS = 14.0;
const float WAKE_DRIFT = 0.3;

/* How the water's surface falls about a point: which way is down, how steeply, how surely that reads as a drop, and how it bends, bowing over a lip and hollowing at a foot. */
struct Drop {
	vec2 down;
	float slope;
	float steep;
	float bend;
};

struct Body {
	vec3 absorption;
	vec3 scatter;
	float calm;
	float murk;
	float sea;
	float canal;
	float sheen;
};

/* The water's normal from every wave the pixels show, the normal its broadest wave alone gives, the share of the waves too fine to show,
 * and how ruffled those fine waves are by the wind. */
struct Surface {
	vec3 normal;
	vec3 broad;
	float lost;
	float ruffled;
};

float SwellTrain(vec2 p, vec2 heading, float frequency, float speed)
{
	float phase = dot(p, heading) * frequency * TAU - Clock() * speed + Noise(p * 0.05) * 6.0;
	return cos(phase) * frequency;
}

vec3 Stretched(vec2 q, vec2 stretch)
{
	vec3 noise = NoiseSlope(q * stretch);
	return vec3(noise.x, noise.yz * stretch);
}

/* Noise and its slope drawn out along a run: stretched along each of the map's axes and weighed by how far the run lies along it, so a bend shows no swirl. */
vec3 Streaked(vec2 q, vec2 run, vec2 stretch)
{
	vec2 share = run * run;
	vec3 streaks = vec3(0.0);
	if (share.x > MIN_STREAK_SHARE) streaks += Stretched(q, stretch) * share.x;
	if (share.y > MIN_STREAK_SHARE) streaks += Stretched(q + STREAK_SEED, stretch.yx) * share.y;
	return streaks;
}

/* Where the current has carried a point's pattern from: two phases half a cycle apart, each drifting with the flow for a cycle and then starting afresh,
 * and how far toward the trailing one they blend so neither restart shows. */
struct Carried {
	vec2 leading;
	vec2 trailing;
	float blend;
};

Carried CarriedBy(vec2 p, vec2 flow)
{
	float cycle = Clock() / FLOW_CYCLE + Noise(p * FLOW_PHASE_SPREAD);
	float lead = fract(cycle);
	vec2 drift = flow * FLOW_PACE * FLOW_CYCLE;
	return Carried(p - drift * lead, p - drift * fract(cycle + 0.5) + FLOW_PHASE_SEED, abs(1.0 - 2.0 * lead));
}

vec3 Current(vec2 p, vec2 flow, vec2 stretch)
{
	vec2 run = normalize(flow);
	Carried carried = CarriedBy(p, flow);
	return mix(Streaked(carried.leading, run, stretch), Streaked(carried.trailing, run, stretch), carried.blend);
}

Surface Stirred(Surface surface, vec2 slope)
{
	surface.normal = normalize(surface.normal - vec3(slope, 0.0));
	return surface;
}

Surface Stilled(Surface surface, float stilling)
{
	surface.normal = normalize(mix(surface.normal, UP, stilling));
	return surface;
}

Surface Tilted(Surface surface, vec2 lean)
{
	surface.broad = normalize(surface.broad + vec3(lean, 0.0));
	return Stirred(surface, -lean);
}

/* Long swell rolls in across the open sea before the wind with a weaker cross swell over it, in crests broken here and there, still to be made out from far away. */
vec2 Swell(vec2 p)
{
	float shown = Resolved(SWELL_PER_TILE);
	if (shown <= 0.0) return vec2(0.0);
	vec2 across = vec2(WIND.y, -WIND.x * 0.4);
	vec2 slope = WIND * SwellTrain(p, WIND, SWELL_PER_TILE, SWELL_SPEED) + across * SwellTrain(p, normalize(across), SWELL_PER_TILE * 1.7, SWELL_SPEED * 1.3) * 0.5;
	return slope / SWELL_PER_TILE * SWELL_STEEPNESS * shown * smoothstep(0.2, 0.6, Noise(p * 0.09 + 3.3));
}

/* Octaves of noise swell turned against each other, each drifting at its own pace; octaves finer than the pixels can show give their share to roughness instead.
 * Gusts roughen the open water in broad patches between calmer lulls. */
Surface Waves(vec2 p, float calm)
{
	float gust = smoothstep(0.25, 0.75, Noise(p * GUSTS_PER_TILE + Clock() * GUST_DRIFT));
	vec2 slope = vec2(0.0);
	vec2 broad = vec2(0.0);
	float height = 1.0;
	float frequency = WAVE_FREQUENCY * mix(1.0, CALM_FREQUENCY, calm);
	float lost = 0.0;
	float total = 0.0;
	for (int octave = 0; octave < WAVE_OCTAVES; octave++) {
		float angle = float(octave) * WAVE_TURN;
		mat2 turn = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
		vec2 drift = vec2(cos(angle), sin(angle)) * WAVE_SPEED * Clock() / sqrt(frequency);
		float shown = smoothstep(UNRESOLVED_WAVE_PIXELS, RESOLVED_WAVE_PIXELS, tile_pixels / frequency);
		vec2 rise = vec2(0.0);
		if (octave == 0 || shown > 0.0) rise = (NoiseSlope(turn * p * frequency + drift + float(octave) * 17.3).yz * turn) * frequency * height;
		slope += rise * shown;
		if (octave == 0) broad = rise;
		lost += height * (1.0 - shown);
		total += height;
		height *= WAVE_PERSISTENCE;
		frequency *= WAVE_LACUNARITY;
	}
	float steepness = mix(WAVE_STEEPNESS * mix(LULL_STEEPNESS, GUST_STEEPNESS, mix(gust, 0.5, calm)), CALM_STEEPNESS, calm) / total;
	slope = slope * steepness + Swell(p) * (1.0 - calm);
	float ruffled = gust * (1.0 - calm);
	return Surface(normalize(vec3(-slope, 1.0)), normalize(vec3(-broad * steepness * 0.5, 1.0)), mix(lost / total, 1.0, ruffled * 0.4), ruffled * lost / total);
}

/* A tile's water where its surface comes within reach of a sheet at this level: a level tile's water lies at its lowest corner, a stream's runs from its lowest corner to its highest. */
vec4 SheetWater(ivec2 tile, float level, float reach)
{
	vec4 water = texelFetch(u_water, Clamped(tile), 0);
	Corners c = SurfaceOf(tile);
	float low = min(min(c.north, c.west), min(c.east, c.south));
	float high = water.r >= 1.0 ? max(max(c.north, c.west), max(c.east, c.south)) : low;
	return level >= low - reach && level <= high + reach ? water : vec4(0.0);
}

/* Whether the four by four tiles from one before the cell to two past it are all open sea, from the water's texels averaged four tiles to a texel;
 * read between texels at the corner the tiles share, the two by two texels blended hold every one of the tiles, and any other water leaves the blend short of full. */
bool OpenSeaAbout(ivec2 cell)
{
	vec4 water = textureLod(u_water, (vec2(cell) + 1.0) / MapSize(), OPEN_SEA_LOD);
	return water.r >= 1.0 && water.g >= 1.0;
}

/* The water field over only the tiles whose water joins this sheet, so a pool's outline is not drawn out over the water of the pool below it;
 * a stream running down between two pools joins both. Open sea, all flat and at one level, joins every tile it weighs. */
vec4 SheetField(vec2 p, float level)
{
	float reach = fwidth(level) > STREAM_TILT * length(fwidth(p)) ? STREAM_REACH : SHEET_TOLERANCE;
	vec2 t = p + ShoreWarp(p) - TILE_CENTRE;
	ivec2 cell = ivec2(floor(t));
	if (OpenSeaAbout(cell)) return OPEN_SEA;
	float wx[4] = SplineWeights(fract(t.x));
	float wy[4] = SplineWeights(fract(t.y));
	vec4 field = vec4(0.0);
	for (int j = 0; j < 4; j++) {
		for (int i = 0; i < 4; i++) field += wx[i] * wy[j] * SheetWater(cell + ivec2(i - 1, j - 1), level, reach);
	}
	return Walled(field, p);
}

float DistancePastEdge(vec2 p)
{
	return distance(p, clamp(p, vec2(0.0), MapSize()));
}

/* Past the map's edge lies open sea. */
vec4 FieldAt(vec2 p, float level)
{
	return DistancePastEdge(p) > 0.0 ? OPEN_SEA : SheetField(p, level);
}

Body BodyOf(vec4 field)
{
	float total = dot(field.gba, vec3(1.0));
	vec3 share = total > MIN_SPREAD ? field.gba / total : vec3(1.0, 0.0, 0.0);
	Body body;
	bool toy = Landscape() == LANDSCAPE_TOYLAND;
	body.absorption = toy ? TOY_ABSORPTION : SEA_ABSORPTION * share.x + CANAL_ABSORPTION * share.y + RIVER_ABSORPTION * share.z;
	body.scatter = toy ? TOY_SCATTER : SEA_SCATTER * share.x + CANAL_SCATTER * share.y + RIVER_SCATTER * share.z;
	body.calm = 1.0 - share.x;
	body.murk = INLAND_MURK * body.calm;
	body.sea = share.x;
	body.canal = share.y;
	body.sheen = toy ? 0.0 : body.calm;
	return body;
}

/* How much of the water shows at a point: natural water draws back from the tile edges to the curved line of its field, canals fill to their walls. */
float Outline(vec4 field, Body body)
{
	float edge = max(fwidth(field.r), 1e-3);
	return mix(smoothstep(WATERLINE - edge, WATERLINE + edge, field.r), 1.0, body.canal);
}

/* Which way and how fast the water runs, a share of full pace; a current running off the map's edge runs on past it, dying away. */
vec2 FlowAt(vec2 p)
{
	vec2 flow = (textureLod(u_flow, p / MapSize(), 0.0).rg * 255.0 - FLOW_TEXEL_STILL) / FLOW_TEXEL_SPAN;
	return flow * max(1.0 - DistancePastEdge(p) / PAST_EDGE_FLOW_TILES, 0.0);
}

/* Foam gathers in a band along the shore, thickest at the water's edge, broken into lace and drifting with the swell. */
float Foam(vec2 p, float band, Body body)
{
	if (band <= 0.0) return 0.0;
	vec2 drift = vec2(Clock() * FOAM_DRIFT, -Clock() * FOAM_DRIFT * 0.7);
	float lace = Noise(p * FOAM_SCALE + drift) * 0.6 + Noise(p * FOAM_SCALE * 2.3 - drift * 1.4) * 0.4;
	float threshold = mix(0.75, FOAM_EDGE_THRESHOLD, band * band);
	return band * smoothstep(threshold, threshold + 0.2, lace) * FOAM_OPACITY * mix(INLAND_FOAM, 1.0, body.sea) * Resolved(FOAM_SCALE);
}

/* Near the shore: close inside the water's outline, which curves where the ground's own edge runs in steps. */
float ShoreBand(float level)
{
	return 1.0 - smoothstep(0.0, OUTLINE_FOAM_WIDTH, level - WATERLINE);
}

/* Off the shore the swell breaks in lines that run along it and roll in toward it, broken where the waves are lower. */
float Breakers(vec2 p, float level, Body body)
{
	float offshore = level - WATERLINE;
	float reach = smoothstep(0.03, 0.08, offshore) * (1.0 - smoothstep(0.15, BREAKER_REACH, offshore));
	if (reach <= 0.0) return 0.0;
	float phase = offshore * BREAKERS_PER_REACH + Clock() * BREAKER_SPEED + Noise(p * 0.7) * 3.0;
	float crest = smoothstep(0.78, 0.98, 0.5 + 0.5 * sin(phase));
	float broken = smoothstep(0.4, 0.75, Noise(p * 2.2 + vec2(Clock() * 0.07, 0.0)));
	return crest * broken * reach * BREAKER_OPACITY * body.sea * Resolved(4.0);
}

/* Wake foam breaks into streaks drawn out the way the ship ran, lying still on the water once it has passed; thick, it fills in whole. */
float WakeLace(vec2 along, float amount)
{
	vec2 q = along * vec2(WAKE_STREAK_ALONG, WAKE_STREAK_ACROSS);
	float lace = Noise(q + vec2(0.0, Clock() * WAKE_DRIFT)) * 0.6 + Noise(q * 2.3 + 5.1) * 0.4;
	float threshold = mix(0.75, 0.15, amount);
	return smoothstep(threshold, threshold + 0.25, lace) * min(amount * 1.4, 1.0);
}

/* A ship under way throws foam off its bow that hugs its sides, then spreads back in two arms at the Kelvin angle around a churned wash
 * trailing from its stern; the faster it runs the longer and whiter its wake, which fades as it spreads. */
float Wake(vec2 p)
{
	float strongest = 0.0;
	vec2 streak = vec2(0.0);
	for (int index = 0; index < u_wakes; index++) {
		vec4 hull = u_wake_hulls[index];
		vec4 shape = u_wake_shapes[index];
		vec2 across_way = vec2(-hull.w, hull.z);
		vec2 offset = p - hull.xy;
		float behind_bow = shape.x - dot(offset, hull.zw);
		float side = abs(dot(offset, across_way));
		float reach = shape.x + shape.w;
		if (behind_bow < -shape.y || behind_bow > reach || side > KELVIN_SLOPE * max(behind_bow, 0.0) + shape.y * 2.0) continue;

		float fade = 1.0 - smoothstep(0.2, 1.0, behind_bow / reach);
		float flank = shape.y * smoothstep(-shape.y, shape.x * BOW_FLARE_LENGTHS, behind_bow) + WAKE_ARM_WIDTH;
		float arm_line = max(KELVIN_SLOPE * behind_bow, flank);
		float width = max(WAKE_ARM_WIDTH + WAKE_ARM_SPREAD * max(behind_bow, 0.0), WAKE_LINE_PIXELS / tile_pixels);
		float off_arm = (side - arm_line) / width;
		float bow = exp(-max(behind_bow, 0.0) / (shape.x * 2.0 * BOW_FOAM_LENGTHS));
		float arm = exp(-off_arm * off_arm) * mix(WAKE_ARM_FOAM, 1.0, bow);

		float behind_stern = behind_bow - 2.0 * shape.x;
		float wash_width = shape.y * (1.0 + WASH_SPREAD * max(behind_stern, 0.0) / shape.x);
		float wash_fade = 1.0 - smoothstep(0.0, shape.w * WASH_SHARE, behind_stern);
		float wash = smoothstep(-shape.y, 0.0, behind_stern) * (1.0 - smoothstep(wash_width * 0.3, wash_width, side)) * wash_fade * WASH_FOAM;

		float churn = max(arm * fade, wash) * shape.z;
		if (churn <= strongest) continue;
		strongest = churn;
		streak = vec2(dot(p, hull.zw), dot(p, across_way));
	}
	return strongest > 0.0 ? WakeLace(streak, strongest) : 0.0;
}

/* Sun on water: a soft path where the waves are too fine to make out, and close up glints off single wavelets turned to catch it, each twinkling as it turns. */
vec3 SunOnWater(Surface surface, vec3 view, vec2 p, float sunlit, float sparkle)
{
	vec3 light = SunDirection();
	vec3 swaying = normalize(mix(UP, surface.broad, PATH_SWAY));
	float path = min(Highlight(swaying, view, light, PATH_COAT) * PATH_STRENGTH * sparkle, PATH_CEILING) * surface.lost;

	float shown = Resolved(GLINTS_PER_TILE);
	float glint = 0.0;
	if (shown > 0.0) {
		vec2 q = p * GLINTS_PER_TILE;
		ivec2 cell = ivec2(floor(q));
		vec2 seed = Hash2(cell);
		float turn = seed.x * TAU + Clock() * GLINT_TWINKLE * (0.5 + seed.y);
		vec3 facet = normalize(surface.normal + vec3(vec2(cos(turn), sin(turn)) * GLINT_TILT * seed.y, 0.0));
		float spot = 1.0 - smoothstep(GLINT_SIZE * 0.4, GLINT_SIZE, length(fract(q) - 0.25 - 0.5 * seed));
		glint = min(Highlight(facet, view, light, GLINT_COAT) * GLINT_STRENGTH * sparkle, GLINT_CEILING) * spot * shown * (1.0 - surface.lost);
	}
	return Sunlight(surface.normal, sunlit) * (path + glint);
}

/* The sky the water mirrors, a little deepened, seen never quite at the horizon, and never mirrored whole however low the view.
 * Where the wind ruffles it the water mirrors less, and that from higher in the sky, so gusts darken it in patches between bright slicks. */
vec3 SkyOnWater(vec3 way, Surface surface, vec3 view, float reflectance, out float reflected)
{
	float ceiling = Landscape() == LANDSCAPE_TOYLAND ? TOY_REFLECTANCE_CEILING : REFLECTANCE_CEILING;
	reflected = min(Fresnel(dot(surface.normal, view), reflectance), ceiling) * (1.0 - surface.ruffled * RUFFLED_DIMMING);
	return MirroredSky(way + UP * surface.ruffled * RUFFLED_SPREAD, REFLECTED_LIFT) * REFLECTED_SKY;
}

vec4 MirroredWorld(vec3 from, vec3 way)
{
	for (int tap = 1; tap <= MIRROR_STEPS; tap++) {
		float share = float(tap) / float(MIRROR_STEPS);
		vec4 clip = u_view_projection * vec4(from + way * MIRROR_REACH * share * share, 1.0);
		vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
		if (clip.w <= 0.0 || any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) break;
		float scene = AheadOf(texture(u_scene_depth, uv).r);
		if (clip.w > scene && clip.w < scene + MIRROR_THICKNESS) {
			vec2 edge = min(uv, 1.0 - uv);
			return vec4(texture(u_scene_colour, uv).rgb, smoothstep(0.0, MIRROR_EDGE, min(edge.x, edge.y)) * (1.0 - share));
		}
	}
	return vec4(0.0);
}

vec3 WorldOver(vec3 sky, vec3 from, vec3 way, float mirror)
{
	if (mirror <= 0.0) return sky;
	vec4 world = MirroredWorld(from, way);
	return mix(sky, world.rgb, world.a * mirror);
}

/* The water's depth behind a screen point, along the sight line and straight down. */
vec2 DepthBehind(vec2 uv, vec3 sight, float surface)
{
	float behind = SightDistance(texture(u_scene_depth, uv).r, sight);
	vec3 bed = Eye() + sight * behind;
	return vec2(behind - surface, v_position.z - bed.z);
}

/* The level a tile's water stands at a point: down the plane of a river running down a slope, and level with its lowest corner elsewhere. */
float WaterLevelOn(ivec2 tile, vec2 f)
{
	Corners c = SurfaceOf(tile);
	bool inclined = texelFetch(u_water, Clamped(tile), 0).a > HALF_TILE && min(c.north, c.south) != max(c.north, c.south);
	if (!inclined) return min(min(c.north, c.west), min(c.east, c.south));
	return mix(mix(c.north, c.west, f.x), mix(c.east, c.south, f.x), f.y);
}

/* The water's level near a point, on the tile there where it holds water and on this tile's water carried on where it does not; past the map's edge, the open sea's. */
float WaterLevelNear(vec2 at, ivec2 home)
{
	ivec2 tile = ivec2(floor(at));
	if (!OnMap(tile)) return OPEN_SEA_LEVEL;
	ivec2 source = texelFetch(u_water, Clamped(tile), 0).r > HALF_TILE ? tile : home;
	return WaterLevelOn(source, at - vec2(source));
}

/* How the water's surface falls about a point, measured across a reach so a drop eases in above its lip and runs out into the pool below; open sea lies level. */
Drop Descent(vec2 p)
{
	ivec2 home = ivec2(floor(p));
	if (OpenSeaAbout(home)) return Drop(vec2(0.0), 0.0, 0.0, 0.0);
	vec2 dx = vec2(RAPIDS_REACH, 0.0);
	vec2 dy = vec2(0.0, RAPIDS_REACH);
	vec4 around = vec4(WaterLevelNear(p - dx, home), WaterLevelNear(p + dx, home), WaterLevelNear(p - dy, home), WaterLevelNear(p + dy, home));
	vec2 fall = vec2(around.x - around.y, around.z - around.w) * LevelRise() / (2.0 * RAPIDS_REACH);
	float slope = length(fall);
	float bend = (dot(around, vec4(1.0)) - 4.0 * WaterLevelNear(p, home)) * LevelRise() / RAPIDS_REACH;
	return Drop(slope > 0.0 ? fall / slope : vec2(0.0), slope, smoothstep(RAPIDS_TILT, 2.0 * RAPIDS_TILT, slope), bend);
}

/* Where a river drops it breaks white: glassy over the lip, torn into streaks racing down the slope, and churned thick where it plunges into the pool at the foot. */
float Whitewater(vec2 p, Drop drop, float glassy)
{
	float plunge = smoothstep(0.0, PLUNGE_BEND, drop.bend);
	float broken = max(drop.steep * (1.0 - glassy), plunge);
	if (broken <= 0.0) return 0.0;
	vec2 across = vec2(-drop.down.y, drop.down.x);
	vec2 q = vec2(dot(p, drop.down) * RAPIDS_ALONG - Clock() * RAPIDS_SPEED, dot(p, across) * RAPIDS_ACROSS);
	float torn = Resolved(RAPIDS_ACROSS * 2.3);
	float churn = 0.55 * Noise(q) + 0.3 * Noise(q * 2.3 + 5.1) + 0.15 * mix(0.5, Noise(q * 5.3 + 1.7), torn);
	vec2 edges = (Landscape() == LANDSCAPE_TOYLAND ? TOY_WHITEWATER_EDGES : vec2(0.4, mix(0.65, 0.58, torn))) - PLUNGE_FROTH * plunge;
	return broken * smoothstep(edges.x, edges.y, churn) * RAPIDS_FOAM;
}

bool Spanned(ivec2 tile)
{
	return (texelFetch(u_network, Clamped(tile), 0).a & NETWORK_BRIDGE_BIT) != 0u;
}

/* How much of the sky a bridge deck overhead keeps from a point: the deck runs along the line of tiles it spans, down their middles. */
float DeckCover(vec2 p)
{
	ivec2 tile = ivec2(floor(p));
	if (!Spanned(tile)) return 0.0;
	bool along_x = Spanned(tile + ivec2(1, 0)) || Spanned(tile - ivec2(1, 0));
	float across = abs((along_x ? fract(p.y) : fract(p.x)) - HALF_TILE);
	return 1.0 - smoothstep(DECK_SHADE_INNER, DECK_SHADE_OUTER, across);
}

/* Along snowbound shores, in snowfields or above the snow line, the water freezes in a rim of ice that reaches out further here and there and breaks into floes at its outer edge. */
float ShoreIce(vec2 p, float level, out float floes)
{
	floes = 0.5;
	if (Landscape() != LANDSCAPE_ARCTIC) return 0.0;
	float snowbound = max(smoothstep(0.05, 0.35, ClimateAt(p).blanket), smoothstep(0.0, SNOW_LINE_PULL, SnowLift(v_world.z)));
	if (snowbound <= 0.0) return 0.0;
	floes = 0.6 * Noise(p * FLOES_PER_TILE) + 0.4 * Noise(p * FLOES_PER_TILE * 2.3 + 4.7);
	float reach = ICE_REACH * snowbound * (0.5 + Noise(p * 0.6 + 2.9));
	float edge = level - WATERLINE + (floes - 0.5) * ICE_ROUGHNESS;
	return 1.0 - smoothstep(reach * 0.7, reach, edge);
}

/* How little open water lies about a point for the wind to raise waves over: a pond or a small lake shut in by land lies still, as the open sea past the edge never does. */
float Enclosed(vec2 p)
{
	vec2 to_edge = min(p, MapSize() - p);
	float inland = smoothstep(0.0, FETCH_TILES, min(to_edge.x, to_edge.y));
	return inland * (1.0 - smoothstep(OPEN_FETCH.x, OPEN_FETCH.y, textureLod(u_water, p / MapSize(), FETCH_LOD).r));
}

/* Off a sea shore the water deepens smoothly with the distance out from the land, measured straight across the water,
 * however the ground under it steps, so the bed of coast tiles shows no saw teeth through it and the shallows keep one breadth round every corner.
 * Past the map's edge it deepens on with the distance out from the edge. */
float ShelfPath(vec2 p, vec3 sight)
{
	float offshore = texture(u_shore, p / MapSize()).r * SHELF_TILES + DistancePastEdge(p);
	float depth = SHELF_DEPTH * smoothstep(0.0, SHELF_REACH, offshore);
	return depth / max(-sight.z, GRAZING_SIGHT);
}

/* The water at a point: its field and body, how its surface falls, how glassy it runs over a lip and how shut in it lies from the wind. */
struct Water {
	vec2 p;
	vec4 field;
	Body body;
	Drop drop;
	float glassy;
	float enclosed;
};

Water WaterAt(vec2 p)
{
	Water water;
	water.p = p;
	water.field = FieldAt(p, v_world.z);
	water.body = BodyOf(water.field);
	water.drop = Descent(p);
	water.glassy = smoothstep(0.0, LIP_BEND, -water.drop.bend);
	water.enclosed = Enclosed(p);
	return water;
}

float Speck(vec2 q, vec2 run, vec2 extent)
{
	ivec2 cell = ivec2(floor(q));
	vec2 roll = Hash2(cell + FLECK_SEED);
	if (roll.x > FLECK_SHARE) return 0.0;
	vec2 offset = fract(q) - mix(vec2(FLECK_MARGIN), vec2(1.0 - FLECK_MARGIN), Hash2(cell));
	vec2 local = vec2(dot(offset, run), dot(offset, vec2(-run.y, run.x))) / (extent * mix(FLECK_SMALLEST, 1.0, roll.y));
	return 1.0 - smoothstep(FLECK_SOFTNESS, 1.0, length(local));
}

float Flecks(vec2 p, vec2 flow, float pace)
{
	float shown = Resolved(FLECK_SCALE / (2.0 * FLECK_EXTENT.x)) * smoothstep(FLECK_PACES.x, FLECK_PACES.y, pace);
	if (shown <= 0.0) return 0.0;
	vec2 run = flow / pace;
	vec2 extent = vec2(FLECK_EXTENT.x * max(PitchSine(), LOWEST_PITCH_SINE), FLECK_EXTENT.y);
	Carried carried = CarriedBy(p, flow);
	float specks = mix(Speck(carried.leading * FLECK_SCALE, run, extent), Speck(carried.trailing * FLECK_SCALE, run, extent), carried.blend);
	return specks * FLECK_FOAM * shown;
}

/* The waves streaked and flecked where the current runs, stilled glassy over a lip and leaning down a drop, ruffled where it races down. */
Surface Flowing(Water water, out float flecks)
{
	Surface waves = Waves(water.p, max(water.body.calm, water.enclosed));
	vec2 flow = FlowAt(water.p);
	float pace = length(flow);
	flecks = 0.0;
	if (pace > MIN_PACE) {
		vec3 current = Current(water.p, flow, STREAK_STRETCH);
		float streaked = Resolved(STREAK_STRETCH.y) * pace;
		waves = Stirred(Stilled(waves, pace * FLOW_STILLING), current.yz * mix(FLOW_STEEPNESS, RAPIDS_STEEPNESS, water.drop.steep) * streaked);
		flecks = Flecks(water.p, flow, pace);
	}
	waves = Tilted(Stilled(waves, water.glassy), water.drop.down * water.drop.slope);
	waves.ruffled = max(waves.ruffled * (1.0 - water.glassy), water.drop.steep * (1.0 - water.glassy) * RAPIDS_RUFFLE);
	return waves;
}

/* Where what lies behind the water shows through it bent by its ripples, falling back to straight behind where the bend would reach past the water. */
vec2 Refracted(vec2 uv, vec3 ripples, Body body, vec2 straight, float surface, out vec2 depth)
{
	vec2 bend = (mat3(u_view) * ripples).xy * mix(REFRACTION, INLAND_REFRACTION, body.sheen) * clamp(straight.y / REFRACTION_DEPTH, 0.0, 1.0);
	vec2 bent_uv = uv + bend;
	depth = DepthBehind(bent_uv, SightAt(bent_uv * u_screen.xy), surface);
	if (depth.x > 0.0 && depth.y > 0.0) return bent_uv;
	depth = straight;
	return uv;
}

/* How far light runs through the water to what shows through it: as far as that lies behind, and never less than a sea shelf deepens or a drop churns. */
float PathThrough(Water water, vec3 sight, float behind)
{
	return max(behind, max(ShelfPath(water.p, sight) * water.body.sea, RAPIDS_PATH * water.drop.steep));
}

/* What shows through the water, dimmed and tinted along the light's path through it, under the light the water scatters back and a glow off its crests. */
vec3 Below(vec2 uv, float path, Body body, vec3 light, vec3 ripples)
{
	vec3 transmittance = exp(-body.absorption * path) * (1.0 - body.murk * smoothstep(0.0, MURK_DEPTH, path));
	vec3 crest = body.scatter * light * CREST_GLOW * smoothstep(0.03, 0.25, length(ripples.xy));
	return texture(u_scene_colour, uv).rgb * transmittance + body.scatter * light * (1.0 - transmittance) + crest;
}

/* Every kind of foam the water churns up, lying under any ice along the shore. */
vec3 Frothed(vec3 colour, Water water, float flecks, float behind, vec3 light, vec3 glitter)
{
	float contact = 1.0 - smoothstep(0.0, CONTACT_FOAM_REACH, behind);
	float shore = Foam(water.p, max(ShoreBand(water.field.r), contact), water.body);
	float breakers = Breakers(water.p, water.field.r, water.body) * (1.0 - water.enclosed);
	float river = water.body.calm * (1.0 - water.body.canal);
	float floes;
	float ice = ShoreIce(water.p, water.field.r, floes);
	float foam = max(max(shore, breakers), max(max(Whitewater(water.p, water.drop, water.glassy), flecks) * river, Wake(water.p))) * (1.0 - ice);
	colour = mix(colour, Linear(Landscape() == LANDSCAPE_TOYLAND ? TOY_FOAM : FOAM) * light, foam);
	if (ice > 0.0) colour = mix(colour, Linear(SHORE_ICE) * light * Varied(floes, 0.15) + glitter * 0.3, ice);
	return colour;
}

void main()
{
	vec2 p = v_world.xy;
	tile_pixels = 1.0 / max(max(length(dFdx(p)), length(dFdy(p))), MIN_SPREAD);
	Water water = WaterAt(p);
	float flecks;
	Surface waves = Flowing(water, flecks);
	vec3 facing = normalize(vec3(water.drop.down * water.drop.slope, 1.0));
	vec3 ripples = waves.normal - facing + UP;

	vec3 sight = SightAt(gl_FragCoord.xy);
	float surface = length(v_position - Eye());
	vec3 view = -sight;
	vec2 uv = gl_FragCoord.xy / u_screen.xy;
	vec2 straight = DepthBehind(uv, sight, surface);
	vec2 bent;
	vec2 bent_uv = Refracted(uv, ripples, water.body, straight, surface, bent);

	float sunlit = SunVisibility(v_position, facing);
	vec3 sun = Sunlight(facing, sunlit);
	vec3 ambient = AmbientLight(facing);
	float sheltered = 1.0 - DECK_SHELTER * DeckCover(p);
	vec3 light = sun + ambient * sheltered;

	float reflected;
	vec3 way = reflect(sight, waves.normal);
	vec3 sky = SkyOnWater(way, waves, view, mix(WATER_REFLECTANCE, INLAND_REFLECTANCE, water.body.sheen), reflected) * sheltered;
	vec3 mirrored = WorldOver(sky, v_position, Skyward(way, REFLECTED_LIFT), water.body.calm * (1.0 - waves.ruffled));
	vec3 glitter = SunOnWater(waves, view, p, sunlit, mix(1.0, INLAND_SPARKLE, water.body.sheen));
	vec3 below = Below(bent_uv, PathThrough(water, sight, bent.x), water.body, light, ripples);
	vec3 colour = Frothed(mix(below, mirrored, reflected) + glitter, water, flecks, straight.x, light, glitter);

	vec3 ground = texture(u_scene_colour, uv).rgb;
	float filled = max(water.body.canal, water.drop.steep);
	float shown = max(smoothstep(0.0, mix(CONTACT_DEPTH, INLAND_CONTACT_DEPTH, water.body.calm), straight.y), filled) * Outline(water.field, water.body);
	frag_colour = vec4(mix(ground, colour, shown), AmbientShare(ambient, sun));
}
