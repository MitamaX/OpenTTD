uniform vec2 u_fade;
uniform float u_snow_level;

in vec3 v_position;
in vec3 v_normal;
in vec4 v_colour;
in vec4 v_glass;
in vec2 v_pattern;
flat in uvec4 v_surface;

out vec4 frag_colour;

const vec3 SNOW = vec3(0.93, 0.95, 0.97);
const float GLASS_REFLECTANCE = 0.2;
const float SEED_SCALE = 255.0;

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

/* Buildings fade out where they grow too small to show, dithered; open claddings show only what they are made of, and glass mirrors the sky. */
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
	if (Has(SURFACE_ROOF) && v_position.z > u_snow_level) clad.albedo = mix(clad.albedo, SNOW, smoothstep(0.4, 0.7, normal.z));

	vec3 albedo = Linear(Overlaid(clad.albedo, int(v_surface.w)));
	vec3 colour = Radiance(albedo, normal, v_position, clad.roughness, v_colour.a * clad.occlusion);
	vec3 view = normalize(Eye() - v_position);
	colour += clad.glass * Fresnel(dot(normal, view), GLASS_REFLECTANCE) * SkyRadiance(reflect(-view, normal));
	frag_colour = vec4(colour, 1.0);
}
