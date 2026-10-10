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
const float SWEPT_THINNING = 0.6;
const float SNOW_RELIEF = 0.6;
const vec2 RELIEF_SPAN = vec2(0.6, 1.1);
const vec2 GLOSS_RESOLVED_PIXELS = vec2(6.0, 24.0);
const float DISTANT_ROUGHNESS = 0.7;

bool Has(uint flag)
{
	return (v_surface.z & flag) != 0u;
}

Pane PaneOf(vec3 room, Clad clad)
{
	return Pane(room * v_transmission, v_film, v_surface.x == CLAD_GLASS ? GLASS_REFLECTANCE : PANE_REFLECTANCE, clad.daylight, clad.lamp);
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

float RoofSnow(vec3 normal)
{
	if (Landscape() != LANDSCAPE_ARCTIC) return 0.0;
	float upward = smoothstep(SNOW_FACING_EDGES.x, SNOW_FACING_EDGES.y, normal.z);
	if (upward <= 0.0) return 0.0;
	float lying = clamp(v_position.z / LevelRise() - SnowLine() + 0.5 + (Noise(v_position.xy * SNOW_LINE_PER_TILE) - 0.5) * SNOW_LINE_RAGGED, 0.0, 1.0);
	if (lying <= 0.0) return 0.0;
	float patchy = (Noise(v_position.xy * SNOW_PATCHES_PER_TILE) - 0.5) * (1.0 - lying) * lying;
	vec2 drift = DriftPoint(normal);
	float swept = smoothstep(SWEPT_EDGES.x, SWEPT_EDGES.y, Layered(drift, SNOW_DRIFTS_PER_TILE)) * SWEPT_THINNING;
	float ragged = (Octave(drift, SNOW_RAGS_PER_TILE) - 0.5) * SNOW_RAGGEDNESS;
	float soft = max(SNOW_EDGE_SOFTNESS, SNOW_PATCHES_PER_TILE / tile_pixels);
	return smoothstep(SNOW_SETTLES - soft, SNOW_SETTLES + soft, lying + patchy - swept + ragged) * upward;
}

vec3 Snowed(vec3 cladding, vec3 normal)
{
	float snow = RoofSnow(normal);
	if (snow <= 0.0) return cladding;
	float relief = clamp(Luminance(cladding) / max(Luminance(v_colour.rgb), LEAST_DIVISOR), RELIEF_SPAN.x, RELIEF_SPAN.y);
	return mix(cladding, SNOW * mix(1.0, relief, SNOW_RELIEF), snow);
}

vec4 CladRadiance(Clad clad, vec3 normal, mat3 frame)
{
	vec3 view = ToEye(v_position);
	vec3 albedo = Linear(Overlaid(clad.albedo, int(v_surface.w)));
	float roughness = mix(max(clad.roughness, DISTANT_ROUGHNESS), clad.roughness, smoothstep(GLOSS_RESOLVED_PIXELS.x, GLOSS_RESOLVED_PIXELS.y, tile_pixels));
	float sunlit = SunVisibility(v_position, normal) * clad.sunlit;
	float occlusion = v_colour.a * clad.occlusion;
	Lighting lighting = LightingOf(normal, view, roughness, Sunlight(normal, sunlit));
	vec3 light = clad.glass < 1.0 ? SurfaceLight(albedo, normal, view, Coat(roughness, DIELECTRIC_REFLECTANCE), occlusion, lighting, lighting.sun) : vec3(0.0);
	if (clad.glass > 0.0) {
		vec3 reflection = Mirrored(StreetBefore(v_position, frame[2], v_seed), reflect(-view, normal), roughness);
		light = mix(light, PaneLight(PaneOf(albedo, clad), reflection, normal, view, roughness, occlusion, lighting), clad.glass);
	}
	return vec4(light + albedo * clad.glow, AmbientShare(lighting.ambient, lighting.sun));
}
