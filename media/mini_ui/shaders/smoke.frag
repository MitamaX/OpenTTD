in vec3 v_position;
in vec3 v_normal;
in vec4 v_colour;
in float v_age;

out vec4 frag_colour;

const float SOOT_SHARE = 0.25;
const float TRANSLUCENCY = 0.6;

/* Smoke thins out as it ages, dithered away, and starts out a little sooty. */
void main()
{
	float density = (1.0 - v_age) * smoothstep(0.0, 0.08, v_age);
	if (density <= ScreenNoise(gl_FragCoord.xy)) discard;
	vec3 albedo = Linear(Greyed(v_colour.rgb * mix(1.0 - SOOT_SHARE, 1.0, v_age)));
	frag_colour = vec4(FoliageRadiance(albedo, normalize(v_normal), v_position, TRANSLUCENCY, 1.0), 1.0);
}
