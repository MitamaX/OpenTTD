uniform vec2 u_fade;

in vec3 v_position;
in vec3 v_normal;
in vec4 v_colour;
in vec4 v_glass;
in vec2 v_pattern;
flat in uvec4 v_surface;

out vec4 frag_colour;

const vec3 SNOW = vec3(0.93, 0.95, 0.97);
const float SNOW_LINE_RAGGED = 1.2;
const float SNOW_PATCHES_PER_TILE = 6.0;
const float GLASS_REFLECTANCE = 0.2;
const float SEED_SCALE = 255.0;
const vec2 GLOSS_RESOLVED_PIXELS = vec2(6.0, 24.0);
const float DISTANT_ROUGHNESS = 0.7;

bool Has(uint flag)
{
	return (v_surface.z & flag) != 0u;
}

/* A surface tilted off its face across it, to the right as seen from outside, and up it. */
vec3 Tilted(vec3 normal, vec2 tilt)
{
	vec3 across = abs(normal.z) > 0.95 ? vec3(1.0, 0.0, 0.0) : normalize(vec3(-normal.y, normal.x, 0.0));
	return normalize(normal + across * tilt.x + cross(normal, across) * tilt.y);
}

float RoofSnow(vec3 normal)
{
	float lying = clamp(v_position.z / LevelRise() - SnowLine() + 0.5 + (Noise(v_position.xy * 0.7) - 0.5) * SNOW_LINE_RAGGED, 0.0, 1.0);
	float patchy = (Noise(v_position.xy * SNOW_PATCHES_PER_TILE) - 0.5) * (1.0 - lying);
	return smoothstep(0.35, 0.65, lying + patchy) * smoothstep(0.4, 0.7, normal.z);
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

	Clad clad = CladOf(material, v_pattern, v_colour.rgb);
	clad.albedo *= Plinth(material, v_pattern, 1.0 - abs(normalize(v_normal).z));
	if (Has(SURFACE_FACADE)) clad = Facade(clad, v_surface.y, v_pattern, Has(SURFACE_FRONT), v_glass.rgb, uint(round(v_glass.a * SEED_SCALE)));
	vec3 normal = Tilted(normalize(v_normal) * (gl_FrontFacing ? 1.0 : -1.0), clad.tilt);
	if (Has(SURFACE_ROOF)) clad.albedo = mix(clad.albedo, SNOW, RoofSnow(normal));

	vec3 albedo = Linear(Overlaid(clad.albedo, int(v_surface.w)));
	float roughness = mix(max(clad.roughness, DISTANT_ROUGHNESS), clad.roughness, smoothstep(GLOSS_RESOLVED_PIXELS.x, GLOSS_RESOLVED_PIXELS.y, tile_pixels));
	vec4 lit = Radiance(albedo, normal, v_position, roughness, v_colour.a * clad.occlusion);
	vec3 view = normalize(Eye() - v_position);
	lit.rgb += clad.glass * Fresnel(dot(normal, view), GLASS_REFLECTANCE) * CloudedSky(reflect(-view, normal));
	frag_colour = lit;
}
