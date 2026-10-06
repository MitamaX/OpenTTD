uniform int u_way;
uniform vec2 u_fade;

in vec3 v_position;
in vec3 v_normal;
in vec4 v_colour;
in float v_trait;

out vec4 frag_colour;

const float MATTE = 0.85;
const float POLISHED = 0.2;
const float GLOW_RADIANCE = 5.0;

/* Solids fade out where they grow too small to show, dithered, handing over to what the ground paints in their place.
 * A positive trait is how glossy the surface is, a negative one how brightly it glows. */
void main()
{
	float shown = smoothstep(u_fade.x, u_fade.y, TilePixelsAt(distance(Eye(), v_position)));
	if (shown <= ScreenNoise(gl_FragCoord.xy)) discard;
	vec3 albedo = Linear(Overlaid(v_colour.rgb, u_way));
	float roughness = mix(MATTE, POLISHED, max(v_trait, 0.0));
	vec3 colour = Radiance(albedo, normalize(v_normal), v_position, roughness, v_colour.a);
	frag_colour = vec4(colour + albedo * max(-v_trait, 0.0) * GLOW_RADIANCE, 1.0);
}
