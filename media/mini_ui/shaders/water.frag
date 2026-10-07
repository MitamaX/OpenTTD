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
const float STREAKS_ALONG = 0.9;
const float STREAKS_ACROSS = 5.0;
const float FLOW_SPEED = 0.6;
const float FLOW_STEEPNESS = 0.006;
const float RAPIDS_TILT = 0.12;
const float RAPIDS_REACH = 0.35;
const float RAPIDS_SPEED = 2.4;
const float RAPIDS_STEEPNESS = 0.03;
const float RAPIDS_FOAM = 0.8;
const float CREST_GLOW = 0.35;

const float WATER_REFLECTANCE = 0.02;
const float REFLECTANCE_CEILING = 0.62;
const float REFLECTED_LIFT = 0.04;
const float REFLECTED_SKY = 0.9;
const float REFRACTION = 0.012;
const float REFRACTION_DEPTH = 0.5;

const float PATH_ROUGHNESS = 0.3;
const float PATH_STRENGTH = 0.03;
const float PATH_SWAY = 0.3;
const float PATH_CEILING = 0.35;
const float GLINTS_PER_TILE = 9.0;
const float GLINT_ROUGHNESS = 0.06;
const float GLINT_TILT = 0.22;
const float GLINT_TWINKLE = 0.7;
const float GLINT_SIZE = 0.3;
const float GLINT_STRENGTH = 0.05;
const float GLINT_CEILING = 3.0;

const vec3 SEA_ABSORPTION = vec3(1.6, 0.55, 0.35);
const vec3 SEA_SCATTER = vec3(0.004, 0.022, 0.060);
const vec3 RIVER_ABSORPTION = vec3(3.0, 1.8, 1.6);
const vec3 RIVER_SCATTER = vec3(0.026, 0.066, 0.068);
const vec3 CANAL_ABSORPTION = vec3(2.6, 1.6, 1.4);
const vec3 CANAL_SCATTER = vec3(0.022, 0.060, 0.074);
const float INLAND_MURK = 0.85;

const vec3 FOAM = vec3(0.80, 0.84, 0.86);
const float FOAM_SCALE = 3.1;
const float FOAM_DRIFT = 0.12;
const float CONTACT_DEPTH = 0.025;
const float INLAND_FOAM = 0.15;
const float FOAM_EDGE_THRESHOLD = 0.3;
const float FOAM_OPACITY = 0.85;
const float OUTLINE_FOAM_WIDTH = 0.12;
const float SHEET_TOLERANCE = 0.05;
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
const float SHELF_LEVEL = 0.8;
const float GRAZING_SIGHT = 0.2;

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

struct Body {
	vec3 absorption;
	vec3 scatter;
	float calm;
	float murk;
	float sea;
	float canal;
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

/* Water running one way is streaked along its run, the streaks carried downstream at its pace. */
vec2 Streaks(vec2 p, vec2 run, float speed)
{
	vec2 across = vec2(-run.y, run.x);
	vec3 noise = NoiseSlope(vec2(dot(p, run) * STREAKS_ALONG - Clock() * speed, dot(p, across) * STREAKS_ACROSS));
	return (run * noise.y * STREAKS_ALONG + across * noise.z * STREAKS_ACROSS) * Resolved(STREAKS_ACROSS);
}

Surface Stirred(Surface surface, vec2 slope)
{
	surface.normal = normalize(surface.normal - vec3(slope, 0.0));
	return surface;
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
		vec3 noise = NoiseSlope(turn * p * frequency + drift + float(octave) * 17.3);
		float shown = smoothstep(UNRESOLVED_WAVE_PIXELS, RESOLVED_WAVE_PIXELS, tile_pixels / frequency);
		vec2 rise = (noise.yz * turn) * frequency * height;
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

/* The water field over only the tiles whose water joins this sheet, so a pool's outline is not drawn out over the water of the pool below it;
 * a stream running down between two pools joins both. */
vec4 SheetField(vec2 p, float level)
{
	float reach = fwidth(level) > STREAM_TILT * length(fwidth(p)) ? STREAM_REACH : SHEET_TOLERANCE;
	vec2 t = p + ShoreWarp(p) - TILE_CENTRE;
	ivec2 cell = ivec2(floor(t));
	vec4 wx = SplineWeights(fract(t.x));
	vec4 wy = SplineWeights(fract(t.y));
	vec4 field = vec4(0.0);
	for (int j = 0; j < 4; j++) {
		for (int i = 0; i < 4; i++) field += wx[i] * wy[j] * SheetWater(cell + ivec2(i - 1, j - 1), level, reach);
	}
	return Walled(field, p);
}

/* Past the map's edge lies open sea. */
vec4 FieldAt(vec2 p, float level)
{
	bool inside = all(greaterThanEqual(p, vec2(0.0))) && all(lessThanEqual(p, MapSize()));
	return inside ? SheetField(p, level) : vec4(1.0, 1.0, 0.0, 0.0);
}

Body BodyOf(vec4 field)
{
	float total = dot(field.gba, vec3(1.0));
	vec3 share = total > MIN_SPREAD ? field.gba / total : vec3(1.0, 0.0, 0.0);
	Body body;
	body.absorption = SEA_ABSORPTION * share.x + CANAL_ABSORPTION * share.y + RIVER_ABSORPTION * share.z;
	body.scatter = SEA_SCATTER * share.x + CANAL_SCATTER * share.y + RIVER_SCATTER * share.z;
	body.calm = 1.0 - share.x;
	body.murk = INLAND_MURK * body.calm;
	body.sea = share.x;
	body.canal = share.y;
	return body;
}

/* How much of the water shows at a point: natural water draws back from the tile edges to the curved line of its field, canals fill to their walls. */
float Outline(vec4 field, Body body)
{
	float edge = max(fwidth(field.r), 1e-3);
	return mix(smoothstep(WATERLINE - edge, WATERLINE + edge, field.r), 1.0, body.canal);
}

/* Which way a river runs at a point: along its channel, square to the slope of its field, turned to the side the wind blows toward;
 * nothing where the field lies level, out in wide water. */
vec2 ChannelFlow(vec2 p, float level)
{
	mat2 pixel = mat2(dFdx(p), dFdy(p));
	float span = determinant(pixel);
	if (abs(span) < MIN_SQUARED_SPAN * MIN_SQUARED_SPAN) return vec2(0.0);
	vec2 slope = inverse(transpose(pixel)) * vec2(dFdx(level), dFdy(level));
	vec2 along = vec2(-slope.y, slope.x);
	float steadiness = smoothstep(0.05, 0.3, length(slope));
	return length(along) > 0.0 ? normalize(along) * sign(dot(along, WIND) + 1e-4) * steadiness : vec2(0.0);
}

/* Foam gathers in a band along the shore, thickest at the water's edge, broken into lace and drifting with the swell. */
float Foam(vec2 p, float band, Body body)
{
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
vec3 SunOnWater(Surface surface, vec3 view, vec2 p, vec3 sun)
{
	vec3 light = SunDirection();
	vec3 swaying = normalize(mix(vec3(0.0, 0.0, 1.0), surface.broad, PATH_SWAY));
	float path = min(Highlight(swaying, view, light, PATH_ROUGHNESS) * PATH_STRENGTH, PATH_CEILING) * surface.lost;

	float shown = Resolved(GLINTS_PER_TILE);
	float glint = 0.0;
	if (shown > 0.0) {
		vec2 q = p * GLINTS_PER_TILE;
		ivec2 cell = ivec2(floor(q));
		vec2 seed = Hash2(cell);
		float turn = seed.x * TAU + Clock() * GLINT_TWINKLE * (0.5 + seed.y);
		vec3 facet = normalize(surface.normal + vec3(vec2(cos(turn), sin(turn)) * GLINT_TILT * seed.y, 0.0));
		float spot = 1.0 - smoothstep(GLINT_SIZE * 0.4, GLINT_SIZE, length(fract(q) - 0.25 - 0.5 * seed));
		glint = min(Highlight(facet, view, light, GLINT_ROUGHNESS) * GLINT_STRENGTH, GLINT_CEILING) * spot * shown * (1.0 - surface.lost);
	}
	return sun * max(dot(surface.normal, light), 0.0) * (path + glint);
}

/* The sky the water mirrors, a little deepened, seen never quite at the horizon, and never mirrored whole however low the view.
 * Where the wind ruffles it the water mirrors less, and that from higher in the sky, so gusts darken it in patches between bright slicks. */
vec3 SkyOnWater(vec3 sight, Surface surface, vec3 view, out float reflected)
{
	vec3 mirrored = reflect(sight, surface.normal);
	mirrored.z = max(mirrored.z, REFLECTED_LIFT) + surface.ruffled * RUFFLED_SPREAD;
	reflected = min(Fresnel(dot(surface.normal, view), WATER_REFLECTANCE), REFLECTANCE_CEILING) * (1.0 - surface.ruffled * RUFFLED_DIMMING);
	return SkyRadiance(normalize(mirrored)) * REFLECTED_SKY;
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

/* The water's level near a point, on the tile there where it holds water and on this tile's water carried on where it does not. */
float WaterLevelNear(vec2 at, ivec2 home)
{
	ivec2 tile = ivec2(floor(at));
	ivec2 source = texelFetch(u_water, Clamped(tile), 0).r > HALF_TILE ? tile : home;
	return WaterLevelOn(source, at - vec2(source));
}

/* How steeply the water's surface leans about a point, measured across a reach so a drop eases in above its lip and runs out into the pool below, and which way is down. */
vec3 Descent(vec2 p)
{
	ivec2 home = ivec2(floor(p));
	vec2 dx = vec2(RAPIDS_REACH, 0.0);
	vec2 dy = vec2(0.0, RAPIDS_REACH);
	vec2 fall = vec2(WaterLevelNear(p - dx, home) - WaterLevelNear(p + dx, home), WaterLevelNear(p - dy, home) - WaterLevelNear(p + dy, home));
	fall *= LevelRise() / (2.0 * RAPIDS_REACH);
	float tilt = length(fall);
	return vec3(tilt > 0.0 ? fall / tilt : vec2(0.0), smoothstep(RAPIDS_TILT, 2.0 * RAPIDS_TILT, tilt));
}

/* Where a river drops it breaks white, the broken water racing down the slope in streaks. */
float Whitewater(vec2 p, vec3 descent)
{
	vec2 down = descent.xy;
	vec2 across = vec2(-down.y, down.x);
	vec2 q = vec2(dot(p, down) * 2.2 - Clock() * RAPIDS_SPEED, dot(p, across) * 7.0);
	float churn = 0.6 * Noise(q) + 0.4 * Noise(q * 2.3 + 5.1);
	return descent.z * smoothstep(0.35, 0.65, churn) * RAPIDS_FOAM;
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

/* Off a sea shore the water deepens smoothly with the curved line of its field, however the ground under it steps, so the bed of coast tiles shows no saw teeth through it. */
float ShelfPath(float level, vec3 sight)
{
	float depth = SHELF_DEPTH * smoothstep(WATERLINE, SHELF_LEVEL, level);
	return depth / max(-sight.z, GRAZING_SIGHT);
}

void main()
{
	vec2 p = v_world.xy;
	tile_pixels = 1.0 / max(max(length(dFdx(p)), length(dFdy(p))), MIN_SPREAD);
	vec4 field = FieldAt(p, v_world.z);
	Body body = BodyOf(field);
	float outline = Outline(field, body);
	float river = body.calm * (1.0 - body.canal);
	vec2 flow = ChannelFlow(p, field.r) * river;
	vec3 descent = Descent(p);

	Surface waves = Waves(p, body.calm);
	waves = Stirred(waves, Streaks(p, flow, FLOW_SPEED) * FLOW_STEEPNESS * length(flow));
	waves = Stirred(waves, Streaks(p, descent.xy, RAPIDS_SPEED) * RAPIDS_STEEPNESS * descent.z);
	vec3 normal = waves.normal;

	vec3 sight = SightAt(gl_FragCoord.xy);
	float surface = length(v_position - Eye());
	vec3 view = -sight;
	vec2 uv = gl_FragCoord.xy / u_screen.xy;
	vec2 straight = DepthBehind(uv, sight, surface);

	vec2 bend = (mat3(u_view) * normal).xy * REFRACTION * clamp(straight.y / REFRACTION_DEPTH, 0.0, 1.0);
	vec2 bent_uv = uv + bend;
	vec2 bent = DepthBehind(bent_uv, SightAt(bent_uv * u_screen.xy), surface);
	if (bent.x <= 0.0 || bent.y <= 0.0) {
		bent_uv = uv;
		bent = straight;
	}

	vec3 up = vec3(0.0, 0.0, 1.0);
	float sunlit = SunVisibility(v_position, up);
	vec3 sun = SunRadiance() * sunlit;
	float sheltered = 1.0 - DECK_SHELTER * DeckCover(p);
	vec3 light = sun * max(SunDirection().z, 0.0) + AmbientLight(up) * sheltered;

	vec3 transmittance = exp(-body.absorption * max(bent.x, ShelfPath(field.r, sight) * body.sea)) * (1.0 - body.murk);
	vec3 crest = body.scatter * light * CREST_GLOW * smoothstep(0.03, 0.25, length(normal.xy));
	vec3 below = texture(u_scene_colour, bent_uv).rgb * transmittance + body.scatter * light * (1.0 - transmittance) + crest;

	float reflected;
	vec3 sky = SkyOnWater(sight, waves, view, reflected) * sheltered;
	vec3 colour = mix(below, sky, reflected) + SunOnWater(waves, view, p, sun);

	float contact = 1.0 - smoothstep(0.0, CONTACT_FOAM_REACH, straight.x);
	float shore = Foam(p, max(ShoreBand(field.r), contact), body);
	float foam = max(max(shore, Breakers(p, field.r, body)), max(Whitewater(p, descent) * river, Wake(p)));
	colour = mix(colour, Linear(FOAM) * light, foam);

	vec3 ground = texture(u_scene_colour, uv).rgb;
	float shown = max(smoothstep(0.0, CONTACT_DEPTH, straight.y), body.calm) * outline;
	frag_colour = vec4(mix(ground, colour, shown), 1.0);
}
