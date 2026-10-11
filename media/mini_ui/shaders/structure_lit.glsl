in vec3 v_position;
in vec3 v_normal;
in vec4 v_colour;
in vec3 v_film;
in vec3 v_transmission;
in vec2 v_pattern;
flat in uvec4 v_surface;
flat in uint v_seed;

out vec4 frag_colour;

const float SNOW_LINE_RAGGED = 1.2;
const float SNOW_LINE_PER_TILE = 0.7;
const float SNOW_SETTLES = 0.5;
const float SNOW_EDGE_SOFTNESS = 0.04;
const vec2 SNOW_FACING_EDGES = vec2(0.4, 0.7);
const float SNOW_PATCHES_PER_TILE = 6.0;
const float SNOW_DRIFTS_PER_TILE = 3.0;
const float SNOW_RAGS_PER_TILE = 9.0;
const float SNOW_RAGGEDNESS = 0.25;
const float SNOW_STREAK_STRETCH = 0.3;
const float LEANING_ROOF = 0.1;
const vec2 SWEPT_EDGES = vec2(0.6, 0.85);
const float SWEPT_THINNING = 0.4;
const float SNOW_RELIEF = 0.6;
const vec2 RELIEF_SPAN = vec2(0.6, 1.1);
const float SNOW_BURIES = 0.25;
const vec2 SUNNY_PITCH_EDGES = vec2(0.1, 0.45);
const float SUNNY_THINNING = 0.3;
const float THAWS_PER_TILE = 8.0;
const vec2 THAW_EDGES = vec2(0.62, 0.8);
const float THAW_DEPTH = 0.6;
const float MELT_REACH = 0.06;
const float MELTWATER_SHADE = 0.75;
const float SNOW_RIPPLES_PER_TILE = 7.0;
const float SNOW_RIPPLE_TILT = 0.3;
const float SNOW_RIPPLE_SEED = 3.1;
const float SNOW_ROUGHNESS = 0.8;
const vec2 GLOSS_RESOLVED_PIXELS = vec2(6.0, 24.0);
const float DISTANT_ROUGHNESS = 0.7;

bool Has(uint flag)
{
	return (v_surface.z & flag) != 0u;
}

Pane PaneOf(vec3 room, Clad clad)
{
	return Pane(room * v_transmission, v_film, v_surface.x == CLAD_GLASS ? GLASS_REFLECTANCE : PANE_REFLECTANCE, clad.daylight, clad.sunbeam, clad.lamp);
}

/* A face's axes: across it, to the right as seen from outside, up it, and out of it. */
mat3 FaceFrame()
{
	vec3 normal = normalize(v_normal) * (gl_FrontFacing ? 1.0 : -1.0);
	vec3 across = abs(normal.z) > 0.95 ? vec3(1.0, 0.0, 0.0) : normalize(vec3(-normal.y, normal.x, 0.0));
	return mat3(across, cross(normal, across), normal);
}

vec3 Facing(Clad clad, mat3 frame)
{
	return normalize(frame * clad.facing);
}

Clad Worn(Clad clad, mat3 frame)
{
	clad.albedo = Walled(v_surface.x, v_pattern, clad.albedo, 1.0 - abs(frame[2].z));
	return clad;
}

/* Where a point lies for the wind's patterns: drawn out down a pitched roof's fall line, where snow slides off in streaks, and as it lies on a flat one. */
vec2 DriftPoint(vec3 normal)
{
	float lean = length(normal.xy);
	if (lean < LEANING_ROOF) return v_position.xy;
	vec2 fall = normal.xy / lean;
	return vec2(dot(v_position.xy, vec2(-fall.y, fall.x)), dot(v_position.xy, fall) * SNOW_STREAK_STRETCH);
}

float SunnyPitch(vec3 normal)
{
	return smoothstep(SUNNY_PITCH_EDGES.x, SUNNY_PITCH_EDGES.y, dot(normal.xy, normalize(SunDirection().xy)));
}

float SnowDepth(vec2 drift, float sunny)
{
	float lying = clamp(v_position.z / LevelRise() - SnowLine() + 0.5 + (Noise(v_position.xy * SNOW_LINE_PER_TILE) - 0.5) * SNOW_LINE_RAGGED, 0.0, 1.0);
	if (lying <= 0.0) return lying - SNOW_SETTLES;
	float patchy = (Noise(v_position.xy * SNOW_PATCHES_PER_TILE) - 0.5) * (1.0 - lying) * lying;
	float swept = smoothstep(SWEPT_EDGES.x, SWEPT_EDGES.y, Layered(drift, SNOW_DRIFTS_PER_TILE)) * SWEPT_THINNING;
	float thawed = smoothstep(THAW_EDGES.x, THAW_EDGES.y, Layered(drift, THAWS_PER_TILE)) * THAW_DEPTH * sunny;
	float ragged = (Octave(drift, SNOW_RAGS_PER_TILE) - 0.5) * SNOW_RAGGEDNESS;
	return lying + patchy + ragged - swept - thawed - SNOW_SETTLES;
}

Clad Snowed(Clad clad, vec3 normal)
{
	float upward = smoothstep(SNOW_FACING_EDGES.x, SNOW_FACING_EDGES.y, normal.z);
	if (Landscape() != LANDSCAPE_ARCTIC || upward <= 0.0) return clad;
	vec2 drift = DriftPoint(normal);
	float sunny = SunnyPitch(normal);
	float depth = SnowDepth(drift, sunny);
	float soft = max(SNOW_EDGE_SOFTNESS, SNOW_PATCHES_PER_TILE / tile_pixels);
	float cover = smoothstep(-soft, soft, depth) * upward;
	float buried = smoothstep(0.0, SNOW_BURIES, depth - SUNNY_THINNING * sunny);
	float relief = Ponds(v_surface.x) ? 1.0 : clamp(Luminance(clad.albedo) / max(Luminance(v_colour.rgb), LEAST_DIVISOR), RELIEF_SPAN.x, RELIEF_SPAN.y);
	vec2 ripples = (vec2(Octave(drift, SNOW_RIPPLES_PER_TILE), Octave(drift + SNOW_RIPPLE_SEED, SNOW_RIPPLES_PER_TILE)) - 0.5) * SNOW_RIPPLE_TILT;
	vec3 wet = clad.albedo * mix(1.0, MELTWATER_SHADE, smoothstep(-MELT_REACH, 0.0, depth) * upward);
	clad.albedo = mix(wet, SNOW * mix(1.0, relief, SNOW_RELIEF * (1.0 - buried)), cover);
	clad.facing = mix(clad.facing, vec3(ripples, 1.0), buried * cover);
	clad.roughness = mix(clad.roughness, SNOW_ROUGHNESS, cover);
	return clad;
}

vec4 CladRadiance(Clad clad, vec3 normal, mat3 frame)
{
	vec3 view = ToEye(v_position);
	vec3 albedo = Linear(Overlaid(clad.albedo, int(v_surface.w)));
	float roughness = mix(max(clad.roughness, DISTANT_ROUGHNESS), clad.roughness, smoothstep(GLOSS_RESOLVED_PIXELS.x, GLOSS_RESOLVED_PIXELS.y, tile_pixels));
	float sunlit = SunVisibility(v_position, normal) * clad.sunlit;
	float occlusion = v_colour.a * clad.occlusion;
	Lighting lighting = LightingOf(normal, view, roughness, Sunlight(normal, sunlit));
	vec3 mirror_way = reflect(-view, normal);
	vec3 light = clad.glass < 1.0 ? SurfaceLight(albedo, Surroundings(mirror_way), normal, view, Coat(roughness, DIELECTRIC_REFLECTANCE), occlusion, lighting, lighting.sun) : vec3(0.0);
	if (clad.glass > 0.0) {
		vec3 reflection = Mirrored(StreetBefore(v_position, frame[2], v_seed), mirror_way, roughness);
		light = mix(light, PaneLight(PaneOf(albedo, clad), reflection, normal, view, roughness, occlusion, lighting), clad.glass);
	}
	return vec4(light + albedo * clad.glow, AmbientShare(lighting.ambient, lighting.sun));
}
