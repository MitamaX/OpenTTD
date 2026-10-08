in vec3 v_position;
in vec3 v_normal;
in vec3 v_colour;
in vec3 v_grown;
in float v_openness;
in float v_reach;
in float v_translucency;
in float v_tint;
in float v_hue;
in float v_wither;
in float v_snow;
flat in vec2 v_window;

out vec4 frag_colour;

const vec3 TINT_SPREAD = vec3(0.16, 0.12, 0.05);
const vec3 HUE_SPREAD = vec3(0.16, 0.04, -0.18);
const vec3 FRESH_TIPS = vec3(1.12, 1.1, 0.84);
const vec2 TIP_REACH = vec2(0.75, 1.0);
const vec3 WITHERED = vec3(0.56, 0.45, 0.24);
const vec3 SNOW_COVER = vec3(0.90, 0.93, 0.97);
const float SNOW_SETTLES = 0.0;
const float SNOW_COVERS = 0.5;
const float LEAFY = 0.01;
const vec2 LEAF_TRANSLUCENCY = vec2(0.2, 0.4);
const float LEAF_CLUMPS_PER_TILE = 24.0;
const vec2 LEAF_SHOWN_PIXELS = vec2(1.5, 4.0);
const float LEAF_POCKETS = 0.3;
const float LEAF_RELIEF = 0.008;
const float LEAST_FACING = 0.1;
const float RAGGED_EDGE = 0.45;
const float RAGGED_FACING = 0.45;
const float LEAF_ROUGHNESS = 0.6;
const float BARK_ROUGHNESS = 0.9;

float Leafy()
{
	return step(LEAFY, v_translucency);
}

/* How far a surface is foliage, clumped into sprays of leaves, rather than flesh, sweets or wood, and how far its clumps are still big enough to show. */
float Foliage()
{
	float clump_pixels = TilePixelsAt(distance(Eye(), v_position)) / LEAF_CLUMPS_PER_TILE;
	return smoothstep(LEAF_TRANSLUCENCY.x, LEAF_TRANSLUCENCY.y, v_translucency) * smoothstep(LEAF_SHOWN_PIXELS.x, LEAF_SHOWN_PIXELS.y, clump_pixels);
}

/* Leaves vary from tree to tree in brightness and, stand by stand, in hue, freshen toward the tips of the crown and brown as a tree dies; snow lies on what faces up. Wood keeps its colour. */
vec3 Albedo(vec3 normal)
{
	float leafy = Leafy();
	vec3 albedo = v_colour * (1.0 + leafy * TINT_SPREAD * (v_tint * 2.0 - 1.0)) * (1.0 + leafy * HUE_SPREAD * (v_hue * 2.0 - 1.0));
	albedo *= mix(vec3(1.0), FRESH_TIPS, smoothstep(TIP_REACH.x, TIP_REACH.y, v_reach) * leafy);
	albedo = mix(albedo, WITHERED * dot(albedo, LUMA_WEIGHTS) * 2.0, v_wither * leafy);
	return mix(albedo, SNOW_COVER, v_snow * leafy * smoothstep(SNOW_SETTLES, SNOW_COVERS, normal.z));
}

/* The clumps' relief bends the normal by how it slopes across the screen, so each clump is lit on its own bulge; bent, it still faces the eye, as everything seen does. */
vec3 Bumped(vec3 normal, float relief, vec3 view)
{
	vec3 across = dFdx(v_position);
	vec3 down = dFdy(v_position);
	vec3 r1 = cross(down, normal);
	vec3 r2 = cross(normal, across);
	float det = dot(across, r1);
	vec3 bent = abs(det) * normal - sign(det) * (dFdx(relief) * r1 + dFdy(relief) * r2);
	if (dot(bent, bent) <= 0.0) return normal;
	bent = normalize(bent);
	return normalize(bent + view * max(0.0, LEAST_FACING - dot(bent, view)));
}

/* Foliage breaks into clumps of leaves, dark in the pockets between them and frayed where the crown turns away at its rim. */
void main()
{
	if (!InWindow(v_window, ScreenNoise(gl_FragCoord.xy))) discard;
	vec3 normal = normalize(v_normal) * (gl_FrontFacing ? 1.0 : -1.0);
	float foliage = Foliage();
	float clumps = Noise(v_grown * LEAF_CLUMPS_PER_TILE);
	vec3 view = normalize(Eye() - v_position);
	vec3 leaves = Bumped(normal, (clumps - 0.5) * LEAF_RELIEF * foliage, view);
	if (clumps < RAGGED_EDGE * (1.0 - smoothstep(0.0, RAGGED_FACING, abs(dot(normal, view)))) * foliage) discard;

	float sunlit = SunVisibility(v_position, leaves);
	vec3 albedo = Albedo(normal) * mix(1.0, mix(1.0 - LEAF_POCKETS, 1.0, smoothstep(0.25, 0.75, clumps)), foliage);
	vec4 lit = FoliageRadiance(Linear(albedo), leaves, v_position, v_translucency, v_openness, sunlit);
	lit.rgb += Reflected(leaves, v_position, mix(BARK_ROUGHNESS, LEAF_ROUGHNESS, Leafy()), DIELECTRIC_REFLECTANCE, v_openness, sunlit);
	frag_colour = lit;
}
