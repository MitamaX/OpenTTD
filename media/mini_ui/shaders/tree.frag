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
flat in float v_crown_sunlit;

out vec4 frag_colour;

const vec3 TINT_VARIETY = vec3(0.32, 0.24, 0.1);
const vec3 HUE_VARIETY = vec3(0.36, 0.24, -0.12);
const vec3 FRESH_TIPS = vec3(1.12, 1.1, 0.84);
const vec2 TIP_REACH = vec2(0.75, 1.0);
const vec3 AUTUMN_GOLD = vec3(0.80, 0.58, 0.16);
const vec3 AUTUMN_RUST = vec3(0.74, 0.40, 0.15);
const float AUTUMN_GAIN = 1.9;
const float TURN_EDGE = 0.06;
const float SNOW_SETTLES = 0.2;
const float SNOW_COVERS = 0.6;
const float SNOW_CLUMPING = 0.5;
const float LEAFY = 0.01;
const vec2 LEAF_TRANSLUCENCY = vec2(0.2, 0.4);
const float LEAF_CLUMPS_PER_TILE = 24.0;
const float LEAF_SPRAYS_PER_TILE = 7.0;
const vec3 SPRAY_VARIETY = vec3(0.32, 0.24, 0.08);
const vec2 SPRAY_EDGES = vec2(0.25, 0.75);
const float SPRAY_SEED = 2.3;
const float LEAF_POCKETS = 0.4;
const vec2 LEAF_DOME_EDGES = vec2(0.2, 0.8);
const float LEAF_RELIEF = 0.018;
const float LEAST_FACING = 0.1;
const float RAGGED_EDGE = 0.55;
const float RAGGED_FACING = 0.45;
const float LEAF_ROUGHNESS = 0.6;
const float BARK_ROUGHNESS = 0.9;
const float INSIDE_OPENNESS = 0.35;

float Leafy()
{
	return step(LEAFY, v_translucency);
}

float LeafResolved(float per_tile)
{
	return ResolvedAt(per_tile, TilePixelsAt(distance(Eye(), v_position)));
}

float LeafShare()
{
	return smoothstep(LEAF_TRANSLUCENCY.x, LEAF_TRANSLUCENCY.y, v_translucency);
}

/* How far a surface is foliage, clumped into sprays of leaves, rather than flesh, sweets or wood, and how far its clumps are still big enough to show. */
float Foliage()
{
	return LeafShare() * LeafResolved(LEAF_CLUMPS_PER_TILE);
}

float SprayShown()
{
	return LeafShare() * LeafResolved(LEAF_SPRAYS_PER_TILE);
}

float SprayNoise()
{
	return Noise(v_grown * LEAF_SPRAYS_PER_TILE + SPRAY_SEED);
}

vec3 Sprays()
{
	float shown = SprayShown();
	if (shown <= 0.0) return vec3(1.0);
	return Varied(smoothstep(SPRAY_EDGES.x, SPRAY_EDGES.y, SprayNoise()), SPRAY_VARIETY * shown);
}

float Turning()
{
	if (v_wither <= 0.0) return 0.0;
	float shown = SprayShown();
	if (shown <= 0.0) return v_wither;
	return mix(v_wither, smoothstep(-TURN_EDGE, TURN_EDGE, v_wither - SprayNoise()), shown);
}

vec3 Albedo(float upward)
{
	float leafy = Leafy();
	vec3 albedo = v_colour * Varied(v_tint, TINT_VARIETY * leafy) * Varied(v_hue, HUE_VARIETY * leafy) * Sprays();
	albedo *= mix(vec3(1.0), FRESH_TIPS, smoothstep(TIP_REACH.x, TIP_REACH.y, v_reach) * leafy);
	vec3 autumn = mix(AUTUMN_GOLD, AUTUMN_RUST, v_hue) * dot(albedo, LUMA_WEIGHTS) * AUTUMN_GAIN;
	albedo = mix(albedo, autumn, Turning() * leafy);
	return mix(albedo, SNOW, v_snow * leafy * smoothstep(SNOW_SETTLES, SNOW_COVERS, upward));
}

float LeafDome(float clumps)
{
	return smoothstep(LEAF_DOME_EDGES.x, LEAF_DOME_EDGES.y, clumps);
}

vec3 FacetNormal(vec3 across, vec3 down, vec3 normal)
{
	vec3 facet = cross(across, down);
	if (dot(facet, facet) <= 0.0) return normal;
	facet = normalize(facet);
	return dot(facet, normal) < 0.0 ? -facet : facet;
}

vec3 Bumped(vec3 normal, float relief, vec3 view, vec3 across, vec3 down)
{
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
	vec3 across = dFdx(v_position);
	vec3 down = dFdy(v_position);
	float foliage = Foliage();
	float clumps = foliage > 0.0 ? Noise(v_grown * LEAF_CLUMPS_PER_TILE) : 0.5;
	vec3 view = ToEye(v_position);
	float dome = LeafDome(clumps);
	vec3 leaves = Bumped(normal, (dome - 0.5) * LEAF_RELIEF * foliage, view, across, down);
	if (clumps < RAGGED_EDGE * (1.0 - smoothstep(0.0, RAGGED_FACING, abs(dot(normal, view)))) * foliage) discard;

	float clouds = CloudLight(v_position);
	float sunlit = SunVisibility(v_position, leaves, clouds);
	float openness = gl_FrontFacing ? v_openness : v_openness * INSIDE_OPENNESS;
	float upward = FacetNormal(across, down, normal).z + (clumps - 0.5) * SNOW_CLUMPING * foliage;
	vec3 albedo = Albedo(upward) * mix(1.0, mix(1.0 - LEAF_POCKETS, 1.0, dome), foliage);
	Coat coat = Coat(mix(BARK_ROUGHNESS, LEAF_ROUGHNESS, Leafy()), DIELECTRIC_REFLECTANCE);
	frag_colour = FoliageRadiance(Linear(albedo), leaves, view, v_translucency, coat, openness, sunlit, v_crown_sunlit * clouds);
}
