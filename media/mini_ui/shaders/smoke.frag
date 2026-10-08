uniform sampler2D u_scene_depth;

in vec3 v_centre;
in vec2 v_corner;
in float v_size;
in float v_age;
in float v_seed;

out vec4 frag_colour;

const vec3 SMOKE = vec3(0.88, 0.87, 0.85);
const vec3 SOOT = vec3(0.55, 0.54, 0.52);
const float SOOT_CLEARS = 0.35;
const float DENSITY = 0.75;
const float BILLOWS = 2.3;
const float RAGGEDNESS = 0.55;
const float TRANSLUCENCY = 0.8;
const float CHURN_RATE = 0.25;

/* The puff's outline billows and churns as it ages, so no two puffs nor two moments look alike. */
float Billowing(vec2 corner)
{
	float angle = v_seed * 6.2831853 + v_age * 1.7;
	mat2 turn = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
	vec2 q = turn * corner * BILLOWS + v_seed * 31.0;
	float churn = Clock() * CHURN_RATE;
	return 0.65 * Noise(q + churn) + 0.35 * Noise(q * 2.3 - churn);
}

/* Where the puff meets the ground or a building behind it, it thins out instead of being cut off along the meeting. */
float Softened(float density)
{
	vec3 sight = SightAt(gl_FragCoord.xy);
	float behind = SightDistance(texelFetch(u_scene_depth, ivec2(gl_FragCoord.xy), 0).r, sight);
	float own = dot(v_centre - Eye(), sight);
	return density * clamp((behind - own) / v_size, 0.0, 1.0);
}

/* A puff is a soft ball, lit from the sun through its thinner edges, sooty as it leaves the stack and clearing as it thins out. */
void main()
{
	float reach = length(v_corner) + (Billowing(v_corner) - 0.5) * RAGGEDNESS;
	float body = 1.0 - smoothstep(0.35, 1.0, reach);
	float density = Softened(body * DENSITY * (1.0 - v_age) * smoothstep(0.0, 0.08, v_age));
	if (density <= 1e-3) discard;

	vec3 right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
	vec3 up = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);
	vec3 back = vec3(u_view[0][2], u_view[1][2], u_view[2][2]);
	vec2 bulge = v_corner * 0.8;
	vec3 normal = normalize(right * bulge.x + up * bulge.y + back * sqrt(max(1.0 - dot(bulge, bulge), 0.0)));
	vec3 albedo = Linear(mix(SOOT, SMOKE, smoothstep(0.0, SOOT_CLEARS, v_age)));
	vec3 lit = FoliageRadiance(albedo, normal, v_centre, TRANSLUCENCY, 1.0).rgb;
	frag_colour = vec4(lit * density, density);
}
