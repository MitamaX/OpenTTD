uniform sampler2D u_scene_colour;
uniform sampler2D u_scene_depth;
uniform vec4 u_wake_hulls[MOST_WAKES];
uniform vec4 u_wake_shapes[MOST_WAKES];
uniform int u_wakes;

in vec3 v_world;
in vec3 v_position;

out vec4 frag_colour;

const float TAU = 6.2831853;

const int WAVE_OCTAVES = 5;
const int BROAD_OCTAVES = 2;
const float WAVE_FREQUENCY = 0.55;
const float WAVE_LACUNARITY = 1.95;
const float WAVE_PERSISTENCE = 0.55;
const float WAVE_TURN = 1.17;
const float WAVE_SPEED = 0.32;
const float WAVE_STEEPNESS = 0.38;
const float CALM_STEEPNESS = 0.16;
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
const vec2 FLECK_EXTENT = vec2(0.24, 0.12);
const SpeckLattice FLECK_LATTICE = SpeckLattice(ivec2(7919, 3571), 0.25, 0.5, 0.35);
const vec2 FLECK_PACES = vec2(0.14, 0.3);
const float LOWEST_PITCH_SINE = 0.3;
const float RAPIDS_TILT = 0.12;
const float RAPIDS_REACH = 0.35;
const float RAPIDS_STEEPNESS = 0.045;
const float RAPIDS_FOAM = 0.8;
const float RAPIDS_PATH = 0.4;
const float RAPIDS_SCALE = 1.6;
const vec2 RAPIDS_STRETCH = vec2(0.55, 9.0);
const float RAPIDS_RUSH = 2.0;
const vec2 RAPIDS_EDGES = vec2(0.42, 0.6);
const float RAPIDS_AERATION = 0.5;
const float APRON_REACH = 1.2;
const int APRON_STEPS = 3;
const float APRON_LEAST_FALL = 0.1;
const float APRON_FULL_FALL = 0.8;
const float APRON_SCALE = 2.4;
const vec2 APRON_STRETCH = vec2(1.2, 3.0);
const vec2 APRON_EDGES = vec2(0.78, 0.38);
const float APRON_FEATHER = 0.15;
const float APRON_CLOUDING = 0.6;
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
const vec3 SEA_SCATTER = vec3(0.006, 0.020, 0.034);
const vec3 RIVER_ABSORPTION = vec3(3.8, 1.3, 1.5);
const vec3 RIVER_SCATTER = vec3(0.012, 0.055, 0.078);
const vec3 CANAL_ABSORPTION = vec3(2.6, 1.6, 1.4);
const vec3 CANAL_SCATTER = vec3(0.022, 0.060, 0.074);
const float INLAND_MURK = 0.2;
const float MURK_DEPTH = 0.5;
const vec3 TOY_ABSORPTION = vec3(2.4, 0.6, 0.3);
const vec3 TOY_SCATTER = vec3(0.012, 0.075, 0.2);
const float TOY_REFLECTANCE_CEILING = 0.3;

const vec3 FOAM = vec3(0.80, 0.84, 0.86);
const vec3 TOY_FOAM = vec3(0.95, 0.97, 1.0);
const vec3 AERATED_WATER = vec3(0.62, 0.74, 0.76);
const vec2 TOY_WHITEWATER_EDGES = vec2(0.55, 0.61);
const float FOAM_SCALE = 3.1;
const float FOAM_DRIFT = 0.12;
const float CONTACT_DEPTH = 0.025;
const float INLAND_CONTACT_DEPTH = 0.08;
const float INLAND_FOAM = 0.45;
const float FOAM_EDGE_THRESHOLD = 0.3;
const float FOAM_OPACITY = 0.85;
const float OUTLINE_FOAM_WIDTH = 0.12;
const float BREAKER_REACH = 0.38;
const float BREAKERS_PER_REACH = 40.0;
const float BREAKER_SPEED = 1.3;
const float BREAKER_OPACITY = 0.8;
const vec2 BREAKER_CREST_EDGES = vec2(0.45, 0.85);
const float BREAKER_CREST_TOP = 0.99;
const float BREAKER_SWELLS_PER_TILE = 0.9;
const vec2 BREAKER_SWELL_EDGES = vec2(0.25, 0.75);
const float BREAKER_THINNING = 0.85;
const float BREAKER_LACE_SCALE = 6.5;
const vec3 BREAKER_WAKE = vec3(0.25, 0.32, 0.75);
const float BREAKER_WAKE_SHARE = 0.85;
const float BREAKER_FAR_OPACITY = 0.3;
const float CONTACT_FOAM_REACH = 0.05;
const float SHELF_DEPTH = 0.4;
const float DECK_SHELTER = 0.75;
const float DECK_SHADE_INNER = 0.2;
const float DECK_SHADE_OUTER = 0.5;
const float SHELF_REACH = 2.5;
const float CHANNEL_DEPTH = 0.25;
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

/* How the water's surface falls about a point: which way is down, how steeply, how surely that reads as a drop, how it bends, bowing over a lip and hollowing at a foot, and where it stands. */
struct Drop {
	vec2 down;
	float slope;
	float steep;
	float bend;
	float level;
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

/* The water's normal from every wave the pixels show, the normal its broadest waves alone give, the share of the waves too fine to show,
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

/* Where the current has carried a point's pattern from in two phases half a cycle apart, and how far they blend toward the trailing one. */
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

float StreamLace(vec2 p, vec2 flow, float scale, vec2 stretch)
{
	if (length(flow) <= MIN_PACE) return 0.5;
	return mix(0.5, Current(p * scale, flow * scale, stretch).x, Resolved(scale * stretch.y));
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
	float open_height = 1.0;
	float calm_height = 0.0;
	float frequency = WAVE_FREQUENCY;
	float lost = 0.0;
	float total = 0.0;
	for (int octave = 0; octave < WAVE_OCTAVES; octave++) {
		float angle = float(octave) * WAVE_TURN;
		mat2 turn = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
		vec2 drift = vec2(cos(angle), sin(angle)) * WAVE_SPEED * Clock() / sqrt(frequency);
		float shown = smoothstep(UNRESOLVED_WAVE_PIXELS, RESOLVED_WAVE_PIXELS, tile_pixels / frequency);
		float height = mix(open_height, calm_height, calm);
		vec2 rise = vec2(0.0);
		if (octave == 0 || shown > 0.0) rise = (NoiseSlope(turn * p * frequency + drift + float(octave) * 17.3).yz * turn) * frequency;
		slope += rise * height * shown;
		if (octave < BROAD_OCTAVES) broad += rise * height;
		lost += height * (1.0 - shown);
		total += height;
		calm_height = open_height;
		open_height *= WAVE_PERSISTENCE;
		frequency *= WAVE_LACUNARITY;
	}
	float steepness = mix(WAVE_STEEPNESS * mix(LULL_STEEPNESS, GUST_STEEPNESS, mix(gust, 0.5, calm)), CALM_STEEPNESS, calm) / total;
	slope = slope * steepness + Swell(p) * (1.0 - calm);
	float ruffled = gust * (1.0 - calm);
	return Surface(normalize(vec3(-slope, 1.0)), normalize(vec3(-broad * steepness * 0.5, 1.0)), mix(lost / total, 1.0, ruffled * 0.4), ruffled * lost / total);
}

float BrimShare(float level, float low, float high)
{
	return smoothstep(0.0, BRIM_FADE, level - low) * (1.0 - smoothstep(0.0, BRIM_FADE, high - level));
}

/* A tile's water as wholly as it joins a sheet at this level. */
vec4 SheetWater(ivec2 tile, float level, out float pooled, out float brim)
{
	vec4 water = TileWater(tile);
	Corners c = SurfaceOf(tile);
	float low = LowestCorner(c);
	float top = HighestCorner(c);
	float high = water.r >= 1.0 ? top : low;
	float apart = max(max(low - level, level - high), 0.0);
	vec4 joined = water * (1.0 - smoothstep(SHEET_HOLD, SHEET_REACH, apart));
	pooled = high > low ? 0.0 : joined.a;
	brim = water.r > 0.0 ? 0.0 : BrimShare(level, low, top);
	return joined;
}

/* Whether the four by four tiles from one before the cell to two past it are all open sea, from the water's texels averaged four tiles to a texel;
 * read between texels at the corner the tiles share, the two by two texels blended hold every one of the tiles, and any other water leaves the blend short of full. */
bool OpenSeaAbout(ivec2 cell)
{
	vec4 water = textureLod(u_water, (vec2(cell) + 1.0) / MapSize(), OPEN_SEA_LOD);
	return water.r >= 1.0 && water.g >= 1.0;
}

/* The water field over only the tiles whose water joins this sheet. */
vec4 SheetField(vec2 p, vec2 warped, float level)
{
	vec2 t = warped - TILE_CENTRE;
	ivec2 cell = ivec2(floor(t));
	if (OpenSeaAbout(cell)) return OPEN_SEA;
	float wx[4] = SplineWeights(fract(t.x));
	float wy[4] = SplineWeights(fract(t.y));
	vec4 field = vec4(0.0);
	float pooled = 0.0;
	float brim = 0.0;
	for (int j = 0; j < 4; j++) {
		for (int i = 0; i < 4; i++) {
			float weight = wx[i] * wy[j];
			float tile_pooled;
			float tile_brim;
			field += weight * SheetWater(cell + ivec2(i - 1, j - 1), level, tile_pooled, tile_brim);
			pooled += weight * tile_pooled;
			brim += weight * tile_brim;
		}
	}
	field.r -= BRIM_PULL * brim * pooled / max(dot(field.gba, vec3(1.0)), LEAST_DIVISOR);
	return CanalWalled(field, p);
}

float DistancePastEdge(vec2 p)
{
	return distance(p, clamp(p, vec2(0.0), MapSize()));
}

/* Past the map's edge lies open sea. */
vec4 FieldAt(vec2 p, vec2 warped, float level)
{
	return DistancePastEdge(p) > 0.0 ? OPEN_SEA : SheetField(p, warped, level);
}

Body BodyOf(vec4 field)
{
	float total = dot(field.gba, vec3(1.0));
	vec3 share = total > LEAST_DIVISOR ? field.gba / total : vec3(1.0, 0.0, 0.0);
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

/* Foam breaks into lace drifting with the swell, filling in whole the thicker it lies. */
float Laced(vec2 p, float scale, float thickness)
{
	vec2 drift = vec2(Clock() * FOAM_DRIFT, -Clock() * FOAM_DRIFT * 0.7);
	float lace = Noise(p * scale + drift) * 0.6 + Noise(p * scale * 2.3 - drift * 1.4) * 0.4;
	float threshold = mix(0.75, FOAM_EDGE_THRESHOLD, thickness * thickness);
	return smoothstep(threshold, threshold + 0.2, lace) * Resolved(scale);
}

/* Foam gathers in a band along the shore, thickest at the water's edge. */
float Foam(vec2 p, float band, Body body)
{
	if (band <= 0.0) return 0.0;
	return band * Laced(p, FOAM_SCALE, band) * FOAM_OPACITY * mix(INLAND_FOAM, 1.0, body.sea);
}

/* Near the shore: close inside the water's outline, which curves where the ground's own edge runs in steps. */
float ShoreBand(float level)
{
	return 1.0 - smoothstep(0.0, OUTLINE_FOAM_WIDTH, level - WATERLINE);
}

/* Off the shore the swell breaks in lines that run along it and roll in toward it, thinning away where the waves are lower;
 * close in a line breaks broad and white, further out thinner and fainter, and it frays into lace about its crest and the foam it leaves behind. */
float Breakers(vec2 p, float level, Body body)
{
	float offshore = level - WATERLINE;
	float reach = smoothstep(0.03, 0.08, offshore) * (1.0 - smoothstep(0.15, BREAKER_REACH, offshore));
	if (reach <= 0.0) return 0.0;
	float far_out = smoothstep(0.03, BREAKER_REACH, offshore);
	float phase = offshore * BREAKERS_PER_REACH + Clock() * BREAKER_SPEED + Noise(p * 0.7) * 3.0;
	float lull = 1.0 - smoothstep(BREAKER_SWELL_EDGES.x, BREAKER_SWELL_EDGES.y, Noise(p * BREAKER_SWELLS_PER_TILE + vec2(Clock() * 0.07, 0.0)));
	float edge = mix(mix(BREAKER_CREST_EDGES.x, BREAKER_CREST_EDGES.y, far_out), BREAKER_CREST_TOP, lull * BREAKER_THINNING);
	float crest = smoothstep(edge, BREAKER_CREST_TOP, 0.5 + 0.5 * sin(phase));
	float cycle = fract(phase / TAU);
	float wake = smoothstep(BREAKER_WAKE.x, BREAKER_WAKE.y, cycle) * (1.0 - smoothstep(BREAKER_WAKE.y, BREAKER_WAKE.z, cycle)) * BREAKER_WAKE_SHARE;
	float lace = Laced(p, BREAKER_LACE_SCALE, max(crest, wake));
	return lace * max(crest, wake * (1.0 - lull)) * reach * mix(1.0, BREAKER_FAR_OPACITY, far_out) * BREAKER_OPACITY * body.sea * Resolved(4.0);
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
	reflected = min(Fresnel(dot(surface.broad, view), reflectance), ceiling) * (1.0 - surface.ruffled * RUFFLED_DIMMING);
	return MirroredSky(way + UP * surface.ruffled * RUFFLED_SPREAD, REFLECTED_LIFT) * REFLECTED_SKY;
}

vec4 MirroredWorld(vec3 from, vec3 way)
{
	vec4 start = u_view_projection * vec4(from, 1.0);
	vec4 run = u_view_projection * vec4(way * MIRROR_REACH, 0.0);
	for (int tap = 1; tap <= MIRROR_STEPS; tap++) {
		float share = float(tap) / float(MIRROR_STEPS);
		vec4 clip = start + run * (share * share);
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

/* The level a tile's water stands at a point: down the plane of a river running down a slope or of a bank sloping beside it, and level with its lowest corner elsewhere. */
float WaterLevelOn(ivec2 tile, vec2 f)
{
	Corners c = SurfaceOf(tile);
	vec4 water = TileWater(tile);
	bool inclined = (water.a > HALF_TILE || water.r <= 0.0) && c.north != c.south;
	if (!inclined) return LowestCorner(c);
	return mix(mix(c.north, c.west, f.x), mix(c.east, c.south, f.x), f.y);
}

/* The water's level near a point, on the tile there where it holds water and on this tile's water carried on where it does not; past the map's edge, the open sea's. */
float WaterLevelNear(vec2 at, ivec2 home)
{
	ivec2 tile = ivec2(floor(at));
	if (!OnMap(tile)) return OPEN_SEA_LEVEL;
	ivec2 source = TileWater(tile).r > HALF_TILE ? tile : home;
	return WaterLevelOn(source, at - vec2(source));
}

/* How the water's surface falls about a point, measured across a reach so a drop eases in above its lip and runs out into the pool below; open sea lies level. */
Drop Descent(vec2 p)
{
	ivec2 home = ivec2(floor(p));
	if (OpenSeaAbout(home)) return Drop(vec2(0.0), 0.0, 0.0, 0.0, OPEN_SEA_LEVEL);
	vec2 dx = vec2(RAPIDS_REACH, 0.0);
	vec2 dy = vec2(0.0, RAPIDS_REACH);
	vec4 around = vec4(WaterLevelNear(p - dx, home), WaterLevelNear(p + dx, home), WaterLevelNear(p - dy, home), WaterLevelNear(p + dy, home));
	vec2 fall = vec2(around.x - around.y, around.z - around.w) * LevelRise() / (2.0 * RAPIDS_REACH);
	float slope = length(fall);
	float level = WaterLevelNear(p, home);
	float bend = (dot(around, vec4(1.0)) - 4.0 * level) * LevelRise() / RAPIDS_REACH;
	return Drop(slope > 0.0 ? fall / slope : vec2(0.0), slope, smoothstep(RAPIDS_TILT, 2.0 * RAPIDS_TILT, slope), bend, level);
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
float ShelfDepth(vec2 p)
{
	float offshore = texture(u_shore, p / MapSize()).r * SHELF_TILES + DistancePastEdge(p);
	return SHELF_DEPTH * smoothstep(0.0, SHELF_REACH, offshore);
}

float ChannelDepth(vec4 field)
{
	return CHANNEL_DEPTH * smoothstep(WATERLINE, WATERLINE + CHANNEL_FALL, field.r);
}

/* The water at a point: its field and body, how its surface falls and runs, and how it breaks and lies from the wind. */
struct Water {
	vec2 p;
	vec4 field;
	Body body;
	Drop drop;
	vec2 flow;
	float glassy;
	float aerated;
	float churned;
	float enclosed;
};

float Churned(vec2 at, Drop drop)
{
	vec2 from = at;
	for (int step = 0; step < APRON_STEPS; step++) {
		vec2 flow = FlowAt(from);
		if (length(flow) <= MIN_PACE) break;
		from -= normalize(flow) * (APRON_REACH / float(APRON_STEPS));
	}
	return smoothstep(APRON_LEAST_FALL, APRON_FULL_FALL, WaterLevelNear(from, ivec2(floor(at))) - drop.level);
}

Water WaterAt(vec2 p)
{
	vec2 warped = p + ShoreWarp(p);
	Water water;
	water.p = p;
	water.field = FieldAt(p, warped, v_world.z);
	water.body = BodyOf(water.field);
	water.drop = Descent(warped);
	water.flow = FlowAt(p);
	water.glassy = smoothstep(0.0, LIP_BEND, -water.drop.bend);
	water.aerated = water.drop.steep * (1.0 - water.glassy);
	water.churned = Churned(warped, water.drop);
	water.enclosed = Enclosed(p);
	return water;
}

float Apron(Water water)
{
	if (water.churned <= 0.0) return 0.0;
	float threshold = mix(APRON_EDGES.x, APRON_EDGES.y, water.churned);
	return smoothstep(threshold, threshold + APRON_FEATHER, StreamLace(water.p, water.flow, APRON_SCALE, APRON_STRETCH)) * water.churned;
}

/* Where a river drops it breaks white: glassy over the lip, in fine streaks drawn out down the current as it races down the slope, churned thick where it plunges into the pool at the foot,
 * and spreading from the foot down the current in lace that thins out as the water below settles. */
float Whitewater(Water water)
{
	float plunge = smoothstep(0.0, PLUNGE_BEND, water.drop.bend);
	float broken = max(water.aerated, plunge);
	float white = 0.0;
	if (broken > 0.0) {
		float churn = StreamLace(water.p, water.flow * RAPIDS_RUSH, RAPIDS_SCALE, RAPIDS_STRETCH);
		vec2 edges = (Landscape() == LANDSCAPE_TOYLAND ? TOY_WHITEWATER_EDGES : RAPIDS_EDGES) - PLUNGE_FROTH * plunge;
		white = broken * smoothstep(edges.x, edges.y, churn);
	}
	return max(white, Apron(water)) * RAPIDS_FOAM;
}

float Fleck(vec2 q, vec2 run, vec2 extent)
{
	return Speck(q, FLECK_SHARE, run, extent, FLECK_LATTICE);
}

float Flecks(vec2 p, vec2 flow, float pace)
{
	float shown = Resolved(FLECK_SCALE / (2.0 * FLECK_EXTENT.x)) * smoothstep(FLECK_PACES.x, FLECK_PACES.y, pace);
	if (shown <= 0.0) return 0.0;
	vec2 run = flow / pace;
	vec2 extent = vec2(FLECK_EXTENT.x * max(PitchSine(), LOWEST_PITCH_SINE), FLECK_EXTENT.y);
	Carried carried = CarriedBy(p, flow);
	float specks = mix(Fleck(carried.leading * FLECK_SCALE, run, extent), Fleck(carried.trailing * FLECK_SCALE, run, extent), carried.blend);
	return specks * FLECK_FOAM * shown;
}

/* The waves streaked and flecked where the current runs, stilled glassy over a lip and leaning down a drop, ruffled where it races down. */
Surface Flowing(Water water, out float flecks)
{
	Surface waves = Waves(water.p, max(water.body.calm, water.enclosed));
	vec2 flow = water.flow;
	float pace = length(flow);
	flecks = 0.0;
	if (pace > MIN_PACE) {
		vec3 current = Current(water.p, flow, STREAK_STRETCH);
		float streaked = Resolved(STREAK_STRETCH.y) * pace;
		waves = Stirred(Stilled(waves, pace * FLOW_STILLING), current.yz * mix(FLOW_STEEPNESS, RAPIDS_STEEPNESS, water.drop.steep) * streaked);
		flecks = Flecks(water.p, flow, pace);
	}
	waves = Tilted(Stilled(waves, water.glassy), water.drop.down * water.drop.slope);
	waves.ruffled = max(waves.ruffled * (1.0 - water.glassy), water.aerated);
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

/* How far light runs through the water to what shows through it, never less than the least depth. */
float PathThrough(Water water, vec3 sight, float behind, float least_depth)
{
	return max(behind, max(least_depth / max(-sight.z, GRAZING_SIGHT), RAPIDS_PATH * water.drop.steep));
}

/* What shows through the water, dimmed and tinted along the light's path through it, under the light the water scatters back and a glow off its crests. */
vec3 Below(vec2 uv, float path, Body body, vec3 light, vec3 ripples)
{
	vec3 transmittance = exp(-body.absorption * path) * (1.0 - body.murk * smoothstep(0.0, MURK_DEPTH, path));
	vec3 crest = body.scatter * light * CREST_GLOW * smoothstep(0.03, 0.25, length(ripples.xy));
	return texture(u_scene_colour, uv).rgb * transmittance + body.scatter * light * (1.0 - transmittance) + crest;
}

/* A river racing down a drop runs pale with the air churned into it, under every kind of foam the water churns up, lying under any ice along the shore. */
vec3 Frothed(vec3 colour, Water water, float flecks, float behind, vec3 light, vec3 glitter)
{
	float running = 1.0 - water.body.canal;
	float river = water.body.calm * running;
	colour = mix(colour, Linear(AERATED_WATER) * light, water.aerated * running * RAPIDS_AERATION);
	float contact = 1.0 - smoothstep(0.0, CONTACT_FOAM_REACH, behind);
	float shore = Foam(water.p, max(ShoreBand(water.field.r), contact), water.body);
	float breakers = Breakers(water.p, water.field.r, water.body) * (1.0 - water.enclosed);
	float floes;
	float ice = ShoreIce(water.p, water.field.r, floes);
	float foam = max(max(shore, breakers), max(max(Whitewater(water) * running, flecks * river), Wake(water.p))) * (1.0 - ice);
	colour = mix(colour, Linear(Landscape() == LANDSCAPE_TOYLAND ? TOY_FOAM : FOAM) * light, foam);
	if (ice > 0.0) colour = mix(colour, Linear(SHORE_ICE) * light * Varied(floes, 0.15) + glitter * 0.3, ice);
	return colour;
}

void main()
{
	vec2 p = v_world.xy;
	tile_pixels = 1.0 / max(max(length(dFdx(p)), length(dFdy(p))), LEAST_DIVISOR);
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
	vec3 mirrored = WorldOver(sky, v_position, Skyward(reflect(sight, waves.broad), REFLECTED_LIFT), water.body.calm * (1.0 - waves.ruffled));
	vec3 glitter = SunOnWater(waves, view, p, sunlit, mix(1.0, INLAND_SPARKLE, water.body.sheen));
	float least_depth = mix(ChannelDepth(water.field), ShelfDepth(water.p), water.body.sea);
	vec3 below = Below(bent_uv, PathThrough(water, sight, bent.x, least_depth), water.body, light, ripples);
	vec3 colour = Frothed(mix(below, mirrored, reflected) + glitter, water, flecks, straight.x, light, glitter);

	vec3 ground = texture(u_scene_colour, uv).rgb;
	float filled = max(water.body.canal, max(water.drop.steep, water.churned * APRON_CLOUDING));
	float shown = max(smoothstep(0.0, mix(CONTACT_DEPTH, INLAND_CONTACT_DEPTH, water.body.calm), max(straight.y, least_depth)), filled) * Outline(water.field, water.body);
	frag_colour = vec4(mix(ground, colour, shown), AmbientShare(ambient, sun));
}
