uniform int u_way;
uniform bool u_own_colours;
uniform vec2 u_fade;

in vec3 v_position;
in vec3 v_normal;
in vec4 v_colour;
in float v_trait;

out vec4 frag_colour;

const float MATTE = 0.85;
const float POLISHED = 0.2;
const float GLOW_RADIANCE = 5.0;
const float GRAIN_FREQUENCY = 9.0;
const float GRAIN_VARIETY = 0.22;

/* Matte surfaces show a fine grain where they come near enough for it to resolve; glossy ones stay clean. */
float Grain(vec3 position, float gloss)
{
	vec2 p = position.xy + position.zz * vec2(0.7, -0.4);
	return mix(Varied(Layered(p, GRAIN_FREQUENCY), GRAIN_VARIETY), 1.0, gloss);
}

/* Solids fade out where they grow too small to show, dithered, handing over to what the ground paints in their place.
 * Under the overlay, ways take their layer's accent, while solids keeping their own colours only sink when out of it.
 * A positive trait is how glossy the surface is, a negative one how brightly it glows. */
void main()
{
	tile_pixels = TilePixelsAt(distance(Eye(), v_position));
	if (smoothstep(u_fade.x, u_fade.y, tile_pixels) <= ScreenNoise(gl_FragCoord.xy)) discard;
	float gloss = max(v_trait, 0.0);
	vec3 grained = v_colour.rgb * Grain(v_position, gloss);
	vec3 albedo = Linear(u_own_colours ? Kept(grained, u_way) : Overlaid(grained, u_way));
	vec3 colour = Radiance(albedo, normalize(v_normal), v_position, mix(MATTE, POLISHED, gloss), v_colour.a);
	frag_colour = vec4(colour + albedo * max(-v_trait, 0.0) * GLOW_RADIANCE, 1.0);
}
