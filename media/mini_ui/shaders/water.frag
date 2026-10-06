uniform sampler2D u_scene_colour;
uniform sampler2D u_scene_depth;

in vec3 v_world;
in vec3 v_position;

out vec4 frag_colour;

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

const float WATER_REFLECTANCE = 0.02;
const float SMOOTH_WATER = 0.08;
const float ROUGH_WATER = 0.30;
const float GLINT_CEILING = 1.0;
const float REFRACTION = 0.012;
const float REFRACTION_DEPTH = 0.5;

const vec3 SEA_ABSORPTION = vec3(1.6, 0.55, 0.35);
const vec3 SEA_SCATTER = vec3(0.004, 0.022, 0.060);
const vec3 RIVER_ABSORPTION = vec3(3.0, 1.8, 1.6);
const vec3 RIVER_SCATTER = vec3(0.026, 0.066, 0.068);
const vec3 CANAL_ABSORPTION = vec3(2.6, 1.6, 1.4);
const vec3 CANAL_SCATTER = vec3(0.022, 0.060, 0.074);
const float INLAND_MURK = 0.85;

const vec3 FOAM = vec3(0.80, 0.84, 0.86);
const float FOAM_DEPTH = 0.16;
const float FOAM_SCALE = 3.1;
const float FOAM_DRIFT = 0.12;
const float CONTACT_DEPTH = 0.025;
const float INLAND_FOAM = 0.15;

struct Body {
	vec3 absorption;
	vec3 scatter;
	float calm;
	float murk;
	float sea;
};

/* Value noise with its gradient, smooth enough in both to light waves with. */
vec3 NoiseSlope(vec2 p)
{
	ivec2 cell = ivec2(floor(p));
	vec2 f = fract(p);
	vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
	vec2 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);
	float a = Hash(cell);
	float b = Hash(cell + ivec2(1, 0));
	float c = Hash(cell + ivec2(0, 1));
	float d = Hash(cell + ivec2(1, 1));
	float twist = a - b - c + d;
	float value = a + (b - a) * u.x + (c - a) * u.y + twist * u.x * u.y;
	return vec3(value, du * (vec2(b - a, c - a) + twist * u.yx));
}

/* Octaves of noise swell turned against each other, each drifting at its own pace; octaves finer than the pixels can show give their share to roughness instead. */
vec4 Waves(vec2 p, float calm)
{
	vec2 slope = vec2(0.0);
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
		slope += (noise.yz * turn) * frequency * height * shown;
		lost += height * (1.0 - shown);
		total += height;
		height *= WAVE_PERSISTENCE;
		frequency *= WAVE_LACUNARITY;
	}
	float steepness = mix(WAVE_STEEPNESS, CALM_STEEPNESS, calm) / total;
	return vec4(normalize(vec3(-slope * steepness, 1.0)), lost / total);
}

Body BodyAt(vec2 p)
{
	vec2 uv = p / MapSize();
	bool inside = all(greaterThanEqual(uv, vec2(0.0))) && all(lessThanEqual(uv, vec2(1.0)));
	vec3 kinds = inside ? texture(u_water, uv).gba : vec3(1.0, 0.0, 0.0);
	float total = dot(kinds, vec3(1.0));
	vec3 share = total > MIN_SPREAD ? kinds / total : vec3(1.0, 0.0, 0.0);
	Body body;
	body.absorption = SEA_ABSORPTION * share.x + CANAL_ABSORPTION * share.y + RIVER_ABSORPTION * share.z;
	body.scatter = SEA_SCATTER * share.x + CANAL_SCATTER * share.y + RIVER_SCATTER * share.z;
	body.calm = 1.0 - share.x;
	body.murk = INLAND_MURK * body.calm;
	body.sea = share.x;
	return body;
}

/* Foam gathers in a band along the shore, broken up and drifting with the swell. */
float Foam(vec2 p, float depth, Body body)
{
	float band = 1.0 - smoothstep(0.0, FOAM_DEPTH, depth);
	vec2 drift = vec2(Clock() * FOAM_DRIFT, -Clock() * FOAM_DRIFT * 0.7);
	float lace = Noise(p * FOAM_SCALE + drift) * 0.6 + Noise(p * FOAM_SCALE * 2.3 - drift * 1.4) * 0.4;
	return band * smoothstep(1.0 - band, 1.0, lace + band * 0.35) * mix(INLAND_FOAM, 1.0, body.sea) * Resolved(FOAM_SCALE);
}

/* The water's depth behind a screen point, along the sight line and straight down. */
vec2 DepthBehind(vec2 uv, vec3 sight, float surface)
{
	float behind = SightDistance(texture(u_scene_depth, uv).r, sight);
	vec3 bed = Eye() + sight * behind;
	return vec2(behind - surface, v_position.z - bed.z);
}

void main()
{
	vec2 p = v_world.xy;
	tile_pixels = 1.0 / max(max(length(dFdx(p)), length(dFdy(p))), MIN_SPREAD);
	Body body = BodyAt(p);
	vec4 waves = Waves(p, body.calm);
	vec3 normal = waves.xyz;

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
	vec3 light = sun * max(SunDirection().z, 0.0) + AmbientLight(up);

	vec3 transmittance = exp(-body.absorption * max(bent.x, 0.0)) * (1.0 - body.murk);
	vec3 below = texture(u_scene_colour, bent_uv).rgb * transmittance + body.scatter * light * (1.0 - transmittance);

	float roughness = mix(SMOOTH_WATER, ROUGH_WATER, waves.w);
	float reflected = Fresnel(dot(normal, view), WATER_REFLECTANCE);
	vec3 colour = mix(below, SkyRadiance(reflect(sight, normal)), reflected);
	colour += sun * max(dot(normal, SunDirection()), 0.0) * min(Highlight(normal, view, SunDirection(), roughness), GLINT_CEILING);

	float foam = Foam(p, straight.y, body);
	colour = mix(colour, Linear(FOAM) * light, foam);

	vec3 ground = texture(u_scene_colour, uv).rgb;
	float shown = max(smoothstep(0.0, CONTACT_DEPTH, straight.y), body.calm);
	frag_colour = vec4(mix(ground, colour, shown), 1.0);
}
