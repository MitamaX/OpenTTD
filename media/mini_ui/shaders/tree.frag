in vec3 v_colour;
in float v_openness;
in float v_reach;
in float v_tint;
in float v_hue;
in float v_wither;
in float v_snow;
flat in float v_crown_sunlit;

out vec4 frag_colour;

const vec3 TINT_VARIETY = vec3(0.32, 0.24, 0.1);
const vec3 HUE_VARIETY = vec3(0.36, 0.24, -0.12);
const vec3 FRESH_TIPS = vec3(1.12, 1.1, 0.95);
const vec2 TIP_REACH = vec2(0.75, 1.0);
const vec3 AUTUMN_GOLD = vec3(0.80, 0.58, 0.16);
const vec3 AUTUMN_RUST = vec3(0.74, 0.40, 0.15);
const float AUTUMN_BRIGHTNESS = 0.95;
const float AUTUMN_LEAF_KEPT = 0.4;
const float TURN_EDGE = 0.25;
const float TURN_OUTER_LEAD = 0.3;
const float SNOW_SETTLES = 0.2;
const float SNOW_COVERS = 0.6;
const float SNOW_CLUMPING = 0.5;
const float LEAF_SPRAYS_PER_TILE = 7.0;
const vec3 SPRAY_VARIETY = vec3(0.16, 0.12, 0.04);
const vec2 SPRAY_EDGES = vec2(0.25, 0.75);
const float SPRAY_SEED = 2.3;
const float LEAF_POCKETS = 0.55;
const float MEAN_DOME = 0.5;
const vec2 LEAF_DOME_EDGES = vec2(0.25, 0.75);
const float LEAF_RELIEF = 0.018;
const float LEAST_FACING = 0.1;
const float LEAF_ROUGHNESS = 0.6;
const float BARK_ROUGHNESS = 0.9;
const float INSIDE_OPENNESS = 0.7;
const float INSIDE_SCREEN_OCCLUSION = 0.0;

float Unwooded()
{
	return step(WOOD_TRANSLUCENCY_BOUND, v_translucency);
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
	float outer_lead = (v_reach - 0.5) * TURN_OUTER_LEAD;
	return mix(v_wither, smoothstep(-TURN_EDGE, TURN_EDGE, v_wither + outer_lead - SprayNoise()), shown);
}

vec3 Withered(vec3 leaves, float rust)
{
	vec3 hue = mix(AUTUMN_GOLD, AUTUMN_RUST, rust);
	vec3 dry = hue * (dot(leaves, LUMA_WEIGHTS) * AUTUMN_BRIGHTNESS / dot(hue, LUMA_WEIGHTS));
	return mix(dry, leaves, AUTUMN_LEAF_KEPT);
}

vec3 Albedo(float upward)
{
	float unwooded = Unwooded();
	vec3 albedo = v_colour * Varied(v_tint, TINT_VARIETY * unwooded) * Varied(v_hue, HUE_VARIETY * unwooded) * Sprays();
	albedo *= mix(vec3(1.0), FRESH_TIPS, smoothstep(TIP_REACH.x, TIP_REACH.y, v_reach) * unwooded);
	float turned = Turning() * unwooded;
	albedo = mix(albedo, Withered(albedo, v_hue * turned), turned);
	return mix(albedo, SNOW, v_snow * unwooded * smoothstep(SNOW_SETTLES, SNOW_COVERS, upward));
}

float LeafDome(float clumps)
{
	return smoothstep(LEAF_DOME_EDGES.x, LEAF_DOME_EDGES.y, clumps);
}

float LeafResolvedClumps()
{
	return LeafResolved(LEAF_FRAYING.clumps_per_tile);
}

float PocketShade(float dome)
{
	float resolved = mix(1.0 - LEAF_POCKETS * (1.0 - MEAN_DOME), mix(1.0 - LEAF_POCKETS, 1.0, dome), LeafResolvedClumps());
	return mix(1.0, resolved, LeafShare());
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
	vec3 normal = normalize(v_normal) * (gl_FrontFacing ? 1.0 : -1.0);
	vec3 across = dFdx(v_position);
	vec3 down = dFdy(v_position);
	float foliage = Foliage(LEAF_FRAYING);
	float clumps = Clumps(foliage, LEAF_FRAYING);
	vec3 view = ToEye(v_position);
	float dome = LeafDome(clumps);
	vec3 leaves = Bumped(normal, (dome - 0.5) * LEAF_RELIEF * foliage, view, across, down);
	DropOutOfWindow();
	DropFrayed(FrayingReach(normal, view, foliage, LEAF_FRAYING), clumps);

	float clouds = CloudLight(v_position);
	float sunlit = SunVisibility(v_position, leaves, clouds);
	float canopy_openness = gl_FrontFacing ? v_openness : v_openness * INSIDE_OPENNESS;
	float sky_openness = canopy_openness * PocketShade(dome);
	float upward = FacetNormal(across, down, normal).z + (clumps - 0.5) * SNOW_CLUMPING * foliage;
	vec3 albedo = Albedo(upward);
	Coat coat = Coat(mix(BARK_ROUGHNESS, LEAF_ROUGHNESS, Unwooded()), DIELECTRIC_REFLECTANCE);
	vec4 radiance = FoliageRadiance(Linear(albedo), leaves, view, v_translucency, coat, sky_openness, 1.0 - canopy_openness, sunlit, v_crown_sunlit * clouds);
	frag_colour = vec4(radiance.rgb, radiance.a * (gl_FrontFacing ? 1.0 : INSIDE_SCREEN_OCCLUSION));
}
