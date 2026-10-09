uniform vec2 u_fade;

in vec3 v_position;
in vec3 v_normal;
in vec4 v_colour;
in vec4 v_glass;
in vec2 v_pattern;
flat in uvec4 v_surface;

out vec4 frag_colour;

const vec3 SNOW = vec3(0.87, 0.9, 0.95);
const float SNOW_LINE_RAGGED = 1.2;
const float SNOW_LINE_PER_TILE = 0.7;
const float SNOW_SETTLES = 0.5;
const float SNOW_EDGE_SOFTNESS = 0.04;
const vec2 SNOW_FACING_EDGES = vec2(0.4, 0.7);
const float SNOW_PATCHES_PER_TILE = 6.0;
const float SNOW_DRIFTS_PER_TILE = 2.3;
const vec2 SWEPT_EDGES = vec2(0.6, 0.85);
const float SWEPT_THINNING = 0.6;
const float SNOW_RELIEF = 0.6;
const vec2 RELIEF_SPAN = vec2(0.6, 1.1);
const float SEED_SCALE = 255.0;
const vec2 GLOSS_RESOLVED_PIXELS = vec2(6.0, 24.0);
const float DISTANT_ROUGHNESS = 0.7;
const float FILM_TINT = 0.35;
const float COATED_FILM_TINT = 0.5;

bool Has(uint flag)
{
	return (v_surface.z & flag) != 0u;
}

Pane PaneOf(vec3 room, float lamp, uint material)
{
	bool coated = material == CLAD_GLASS;
	vec3 tint = Linear(v_glass.rgb);
	vec3 film = mix(vec3(1.0), tint / max(Luminance(tint), MIN_SPREAD), coated ? COATED_FILM_TINT : FILM_TINT);
	return Pane(room, film, coated ? GLASS_REFLECTANCE : PANE_REFLECTANCE, lamp);
}

/* A face's axes: across it, to the right as seen from outside, up it, and out of it. */
mat3 FaceFrame(vec3 normal)
{
	vec3 across = abs(normal.z) > 0.95 ? vec3(1.0, 0.0, 0.0) : normalize(vec3(-normal.y, normal.x, 0.0));
	return mat3(across, cross(normal, across), normal);
}

float RoofSnow(vec3 normal)
{
	if (Landscape() != LANDSCAPE_ARCTIC) return 0.0;
	float upward = smoothstep(SNOW_FACING_EDGES.x, SNOW_FACING_EDGES.y, normal.z);
	if (upward <= 0.0) return 0.0;
	float lying = clamp(v_position.z / LevelRise() - SnowLine() + 0.5 + (Noise(v_position.xy * SNOW_LINE_PER_TILE) - 0.5) * SNOW_LINE_RAGGED, 0.0, 1.0);
	if (lying <= 0.0) return 0.0;
	float patchy = (Noise(v_position.xy * SNOW_PATCHES_PER_TILE) - 0.5) * (1.0 - lying) * lying;
	float swept = smoothstep(SWEPT_EDGES.x, SWEPT_EDGES.y, Noise(v_position.xy * SNOW_DRIFTS_PER_TILE)) * SWEPT_THINNING;
	float soft = max(SNOW_EDGE_SOFTNESS, SNOW_PATCHES_PER_TILE / tile_pixels);
	return smoothstep(SNOW_SETTLES - soft, SNOW_SETTLES + soft, lying + patchy - swept) * upward;
}

vec3 Snowed(vec3 cladding, vec3 normal)
{
	float snow = RoofSnow(normal);
	if (snow <= 0.0) return cladding;
	float relief = clamp(Luminance(cladding) / max(Luminance(v_colour.rgb), MIN_SPREAD), RELIEF_SPAN.x, RELIEF_SPAN.y);
	return mix(cladding, SNOW * mix(1.0, relief, SNOW_RELIEF), snow);
}

Clad CladdingSeen(uint material, vec3 view, mat3 frame)
{
	bool facade = Has(SURFACE_FACADE);
	Clad clad = facade ? FacadeWall(material, v_pattern, v_colour.rgb) : CladOf(material, v_pattern, v_colour.rgb);
	clad.albedo *= Plinth(material, v_pattern, 1.0 - abs(frame[2].z));
	if (!facade) return clad;
	Outlook outlook = Outlook(InTexels(view * frame), InTexels(SunDirection() * frame));
	return Facade(clad, v_surface.y, v_pattern, Has(SURFACE_FRONT), uint(round(v_glass.a * SEED_SCALE)), outlook);
}

/* Buildings fade out where they grow too small to show, dithered; open claddings show only what they are made of, and glass mirrors the sky.
 * Far off the sun's highlight spreads matte, as a roof too small to make out would flash its whole face in a single pixel. */
void main()
{
	tile_pixels = TilePixelsAt(distance(Eye(), v_position));
	float noise = ScreenNoise(gl_FragCoord.xy);
	if (smoothstep(u_fade.x, u_fade.y, tile_pixels) <= noise) discard;
	uint material = v_surface.x;
	if (Cover(material, v_pattern) < noise) discard;

	vec3 view = ToEye(v_position);
	mat3 frame = FaceFrame(normalize(v_normal) * (gl_FrontFacing ? 1.0 : -1.0));
	Clad clad = CladdingSeen(material, view, frame);
	vec3 normal = normalize(frame * clad.facing);
	if (Has(SURFACE_ROOF) || Has(SURFACE_FACADE)) clad.albedo = Snowed(clad.albedo, normal);

	vec3 albedo = Linear(Overlaid(clad.albedo, int(v_surface.w)));
	float roughness = mix(max(clad.roughness, DISTANT_ROUGHNESS), clad.roughness, smoothstep(GLOSS_RESOLVED_PIXELS.x, GLOSS_RESOLVED_PIXELS.y, tile_pixels));
	float sunlit = SunVisibility(v_position, normal) * clad.sunlit;
	float occlusion = v_colour.a * clad.occlusion;
	vec4 lit = clad.glass < 1.0 ? SunlitRadiance(albedo, normal, view, Coat(roughness, DIELECTRIC_REFLECTANCE), occlusion, 0.0, sunlit) : vec4(0.0);
	if (clad.glass > 0.0) lit = mix(lit, PaneRadiance(PaneOf(albedo, clad.lamp, material), normal, view, roughness, occlusion, sunlit, v_pattern.y), clad.glass);
	frag_colour = lit;
}
