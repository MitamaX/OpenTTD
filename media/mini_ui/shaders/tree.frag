in vec3 v_position;
in vec3 v_normal;
in vec3 v_colour;
in float v_openness;
in float v_translucency;
in float v_tint;
in float v_wither;
in float v_snow;
flat in vec2 v_window;

out vec4 frag_colour;

const vec3 TINT_SPREAD = vec3(0.16, 0.12, 0.05);
const vec3 WITHERED = vec3(0.56, 0.45, 0.24);
const vec3 SNOW_COVER = vec3(0.90, 0.93, 0.97);
const float SNOW_SETTLES = 0.2;
const float SNOW_COVERS = 0.65;
const float LEAFY = 0.01;

/* Leaves vary a little from tree to tree and brown as a tree dies; snow lies on what faces up. Wood keeps its colour. */
vec3 Albedo(vec3 normal)
{
	float leafy = step(LEAFY, v_translucency);
	vec3 albedo = v_colour * (1.0 + leafy * TINT_SPREAD * (v_tint * 2.0 - 1.0));
	albedo = mix(albedo, WITHERED * dot(albedo, LUMA_WEIGHTS) * 2.0, v_wither * leafy);
	return mix(albedo, SNOW_COVER, v_snow * leafy * smoothstep(SNOW_SETTLES, SNOW_COVERS, normal.z));
}

void main()
{
	if (!InWindow(v_window, ScreenNoise(gl_FragCoord.xy))) discard;
	vec3 normal = normalize(v_normal) * (gl_FrontFacing ? 1.0 : -1.0);
	frag_colour = vec4(FoliageRadiance(Linear(Albedo(normal)), normal, v_position, v_translucency, v_openness), 1.0);
}
