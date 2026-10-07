uniform int u_way;
uniform bool u_own_colours;
uniform vec2 u_fade;
uniform vec2 u_recede;

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

/* How far a solid has sunk into the ground it stands on: from where it stands whole at the first pixels per tile given, by the second's share by the time it fades. */
float Receded()
{
	return u_recede.y * (1.0 - smoothstep(u_fade.y, u_recede.x, tile_pixels));
}

/* Solids fade out where they grow too small to show, dithered, handing over to what the ground paints in their place;
 * some first sink into the ground over a longer reach, thinning and lying flatter so their sides draw no dark lines.
 * Under the overlay, ways take their layer's accent, while solids keeping their own colours only sink when out of it.
 * A positive trait is how glossy the surface is, a negative one how brightly it glows. */
void main()
{
	tile_pixels = TilePixelsAt(distance(Eye(), v_position));
	float receded = Receded();
	if (smoothstep(u_fade.x, u_fade.y, tile_pixels) * (1.0 - receded) <= ScreenNoise(gl_FragCoord.xy)) discard;
	float gloss = max(v_trait, 0.0);
	vec3 grained = v_colour.rgb * Grain(v_position, gloss);
	vec3 albedo = Linear(u_own_colours ? Kept(grained, u_way) : Overlaid(grained, u_way));
	vec3 normal = normalize(mix(normalize(v_normal), vec3(0.0, 0.0, 1.0), receded / max(u_recede.y, MIN_SPREAD)));
	vec3 colour = Radiance(albedo, normal, v_position, mix(MATTE, POLISHED, gloss), v_colour.a);
	frag_colour = vec4(colour + albedo * max(-v_trait, 0.0) * GLOW_RADIANCE, 1.0);
}
